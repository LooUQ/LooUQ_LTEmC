/******************************************************************************
 *  \file iop.c
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED
 * "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
 * LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 ******************************************************************************
 * IOP is the Input/Output processor for the LTEm1 SPI-UART bridge. It clocks
 * data to/from the LTEm1 to be handled by the command or protocol processes.
 *****************************************************************************/

// #define _DEBUG
#include "ltem1c.h"
// debugging output options             UNCOMMENT one of the next two lines to direct debug (PRINTF) output
// #include <jlinkRtt.h>                   // output debug PRINTF macros to J-Link RTT channel
// #define SERIAL_OPT 1                    // enable serial port comm with devl host (1=force ready test)


#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define IOP_RXCTRLBLK_ADVINDEX(INDX) INDX = (++INDX == IOP_RXCTRLBLK_COUNT) ? 0 : INDX

#define QBG_APPREADY_MILLISMAX 5000


// private function declarations
static cbuf_t *txBufCreate();
static iopBuffer_t *rxBufCreate();
static uint16_t txPut(const char *data, uint16_t dataSz);
static uint16_t txTake(char *data, uint16_t dataSz);
static void txSendChunk();
static void rxBufReset(iopBuffer_t *rxBuf);
static uint8_t getDataBuffer(iopDataPeer_t dataPeer);
static void interruptCallbackISR();


/*  ** Known Header Patterns
//
//     Area/Msg Prefix
//
    // -- BG96 init
    // \r\nAPP RDY\r\n      -- BG96 completed firmware initialization
    // 
    // -- Commands
    // +QPING:              -- PING response (instance and summary header)
    // +QIURC: "dnsgip"     -- DNS lookup reply
    //
    // -- Protocols
    // +QIURC: "recv",      -- "unsolicited response" proto tcp/udp
    // +QIRD: #             -- "read data" response 
    // +QSSLURC: "recv"     -- "unsolicited response" proto ssl tunnel
    // +QHTTPGET:           -- GET response, HTTP-READ 
    // CONNECT<cr><lf>      -- HTTP Read
    // +QMTSTAT:            -- MQTT state change message recv'd
    // +QMTRECV:            -- MQTT subscription data message recv'd
    // 
    // -- Async Status Change Messaging
    // +QIURC: "pdpdeact"   -- network pdp context timed out and deactivated

    // default content type is command response
*/




/* public functions
------------------------------------------------------------------------------------------------ */
#pragma region public functions


/**
 *	\brief Initialize the Input/Output Process subsystem.
 */
iop_t *iop_create()
{
    iop_t *iop = calloc(1, sizeof(iop_t));
	if (iop == NULL)
	{
        ltem1_faultHandler(0, "iop-could not alloc iop status struct");
	}
    iop->txBuf = txBufCreate();
    iop->rxCmdBuf = rxBufCreate(IOP_RX_CMDBUF_SZ);      // create cmd/default RX buffer
    iop->rxDataPeer = iopDataPeer__NONE;
    iop->rxDataBufIndx = IOP_NO_BUFFER;
    iop->rxDataBufs[0] = rxBufCreate(IOP_RX_DATABUF_SZ);      // create cmd/default RX buffer
    return iop;
}



/**
 *	\brief Complete initialization and start running iop processes.
 */
void iop_start()
{
    // attach ISR and enable NXP interrupt mode
    gpio_attachIsr(g_ltem1->pinConfig.irqPin, true, gpioIrqTriggerOn_falling, interruptCallbackISR);
    spi_protectFromInterrupt(g_ltem1->spi, g_ltem1->pinConfig.irqPin);
    sc16is741a_enableIrqMode();
}



/**
 *	\brief Verify LTEm1 is ready for IOP operations.
 */
void iop_awaitAppReady()
{
    unsigned long apprdyWaitStart = timing_millis();
    while (g_ltem1->qbgReadyState < qbg_readyState_appReady)
    {
        timing_yield();
        if (apprdyWaitStart + QBG_APPREADY_MILLISMAX < timing_millis())
            ltem1_faultHandler(500, "qbg-BGx module failed to start in the allowed time");
    }
}



/**
 *	\brief Start a (raw) send operation.
 *
 *  \param [in] sendData Pointer to char data to send out, input buffer can be discarded following call.
 *  \param [in] sendSz The number of characters to send.
 *  \param [in] sendImmediate Send queued data, if false continue queueing and wait to send.
 */
void iop_txSend(const char *sendData, uint16_t sendSz, bool sendImmediate)
{
    uint16_t queuedSz = txPut(sendData, sendSz);
    if (queuedSz == sendSz)
    {
        if ( sendImmediate )
            txSendChunk();
    }
    else
        ltem1_faultHandler(500, "iop-tx buffer overflow");
}



/**
 *	\brief Clear receive command response buffer.
 */
void iop_resetCmdBuffer()
{
    rxBufReset(g_ltem1->iop->rxCmdBuf);
}



/**
 *	\brief Clear receive command response buffer.
 */
void iop_resetDataBuffer(uint8_t bufIndx)
{
    rxBufReset(g_ltem1->iop->rxDataBufs[bufIndx]);
}



/**
 *	\brief Response parser looking for ">" prompt to send data to network and then initiates the send .
 */
resultCode_t iop_txDataPromptParser(const char *response, char **endptr)
{
    *endptr = strstr(response, "> ");
    if (*endptr != NULL)
    {
        *endptr += 2;                   // point past data prompt
        return RESULT_CODE_SUCCESS;
    }
    return RESULT_CODE_PENDING;
}


#pragma endregion


/* private local (static) functions
------------------------------------------------------------------------------------------------ */
#pragma region private functions


static cbuf_t *txBufCreate()
{
    cbuf_t *txBuf = calloc(1, sizeof(cbuf_t));
    if (txBuf == NULL)
        return NULL;

    txBuf->buffer = calloc(1, IOP_TX_BUFFER_SZ);
    if (txBuf->buffer == NULL)
    {
        free(txBuf);
        return NULL;
    }
    txBuf->maxlen = IOP_TX_BUFFER_SZ;
    return txBuf;
}


static iopBuffer_t *rxBufCreate(size_t bufSz)
{
    iopBuffer_t *rxBuf = calloc(1, sizeof(iopBuffer_t));
    if (rxBuf == NULL)
        return NULL;

    rxBuf->buffer = calloc(1, bufSz);
    if (rxBuf->buffer == NULL)
    {
        free(rxBuf);
        return NULL;
    }
    rxBuf->head = rxBuf->buffer;
    rxBuf->prevHead = rxBuf->buffer;
    rxBuf->tail = rxBuf->buffer;
    rxBuf->dataPeer = iopDataPeer__NONE;
    return rxBuf;
}



static uint8_t getDataBuffer(iopDataPeer_t dataPeer)
{
    for (size_t i = 0; i < IOP_RX_DATABUFFERS_MAX; i++)                         // return buffer already assigned to dataPeer, if exists
    {
        if (g_ltem1->iop->rxDataBufs[i] != NULL && g_ltem1->iop->rxDataBufs[i]->dataPeer == dataPeer)
            return i;
    }

    for (size_t i = 0; i < IOP_RX_DATABUFFERS_MAX; i++)                         // otherwise, look for empty buffer or create a new buffer to limit
    {
        if (g_ltem1->iop->rxDataBufs[i] != NULL && g_ltem1->iop->rxDataBufs[i]->dataPeer == iopDataPeer__NONE)
        {
            g_ltem1->iop->rxDataBufs[i]->dataPeer = dataPeer;
            return i;
        }
        else if (g_ltem1->iop->rxDataBufs[i] == NULL)                       // empty buffer slot, create a new buffer & assign to peer
        {
            g_ltem1->iop->rxDataBufs[i] = rxBufCreate(IOP_RX_DATABUF_SZ);
            g_ltem1->iop->rxDataBufs[i]->dataPeer = dataPeer;
            return i;
        }
    }
    return IOP_NO_BUFFER;
}



static void rxBufReset(iopBuffer_t *rxBuf)
{
    memset(rxBuf->buffer, 0, (rxBuf->head - rxBuf->buffer));
    rxBuf->head = rxBuf->buffer;
    rxBuf->prevHead = rxBuf->buffer;
    rxBuf->tail = rxBuf->buffer;
    rxBuf->dataPeer = iopDataPeer__NONE;
    rxBuf->irdSz = 0;
    rxBuf->dataReady = false;
}


/**
 *  \brief Puts data into the TX buffer control structure.
 * 
 *  \param [in] data Pointer to where source data is now.
 *  \param [in] dataSz How much data to put in the TX struct.
 * 
 *  \return The number of bytes of stored into the TX struct, compare to dataSz to validate results. 
*/
static uint16_t txPut(const char *data, uint16_t dataSz)
{
    uint16_t result = 0;

    for (size_t i = 0; i < dataSz; i++)
    {
        if(cbuf_push(g_ltem1->iop->txBuf, data[i]))
        {
            result++;
            continue;
        }
        break;
    }
    return result;
}



/**
 *  \brief Gets data from the TX buffer control structure.
 * 
 *  \param [in] data Pointer to where taken data will be placed.
 *  \param [in] dataSz How much data to take, if possible.
 * 
 *  \return The number of bytes of data being returned. 
*/
static uint16_t txTake(char *data, uint16_t dataSz)
{
    uint16_t result = 0;
    
    for (size_t i = 0; i < dataSz; i++)
    {
        if (cbuf_pop(g_ltem1->iop->txBuf, data + i))
        {
            result++;
            continue;
        }
        break;
    }
    return result;
}



/**
 *	\brief Test for send active, if not start a new send flow with chunk.
 */
static void txSendChunk()
{
    // if TX buffer empty, start a TX flow
    // otherwise, TX is underway and ISR will continue servicing queue until emptied
    uint8_t txAvail = sc16is741a_readReg(SC16IS741A_TXLVL_ADDR);

    if (txAvail == SC16IS741A_FIFO_BUFFER_SZ)           // bridge buffer is empty, no "in-flight" TX chars
    {
        char txData[65] = {0};
        uint16_t dataAvail = txTake(txData, txAvail);

        if (dataAvail > 0)
        {
            PRINTF(dbgColor_dCyan, "txChunk=%s\r", txData);
            sc16is741a_write(txData, dataAvail);
        }
    }
}


#pragma endregion

#pragma region ISR


/**
 *  \brief Parse recv'd data (in command RX buffer) for async event preambles that need to be handled immediately.
*/
void iop_rxParseImmediate()
{
    char *urcPrefix = memchr(g_ltem1->iop->rxCmdBuf->prevHead, '+', 6);             // all URC start with '+', skip leading \r\n 
    if (urcPrefix)
    {
        if (g_ltem1->iop->peerTypeMap.sslSocket && memcmp("+QSSLURC: \"recv", urcPrefix, strlen("+QSSLURC: \"recv")) == 0)
        {
            PRINTF(dbgColor_cyan, "-e=sslURC");
            char *connIdPtr = g_ltem1->iop->rxCmdBuf->prevHead + strlen("+QSSLURC: \"recv");
            char *endPtr = NULL;
            uint8_t socketId = (uint8_t)strtol(connIdPtr, &endPtr, 10);
            g_ltem1->sockets->socketCtrls[socketId + iopDataPeer__SOCKET].dataPending = true;
            // discard this chunk, processed here
            g_ltem1->iop->rxCmdBuf->head = g_ltem1->iop->rxCmdBuf->prevHead;
        }

        else if (g_ltem1->iop->peerTypeMap.tcpudpSocket && memcmp("+QIURC: \"recv", urcPrefix, strlen("+QIURC: \"recv")) == 0)
        {
            PRINTF(dbgColor_cyan, "-e=ipURC");
            char *connIdPtr = g_ltem1->iop->rxCmdBuf->prevHead + strlen("+QIURC: \"recv");
            char *endPtr = NULL;
            uint8_t socketId = (uint8_t)strtol(connIdPtr, &endPtr, 10);
            g_ltem1->sockets->socketCtrls[socketId + iopDataPeer__SOCKET].dataPending = true;
            // discard this chunk, processed here
            g_ltem1->iop->rxCmdBuf->head = g_ltem1->iop->rxCmdBuf->prevHead;
        }

        else if (g_ltem1->iop->peerTypeMap.mqttSubscribe && memcmp("+QMTRECV:", urcPrefix, strlen("+QMTRECV:")) == 0)
        {
            PRINTF(dbgColor_cyan, "-e=mqttR");
            // this chunk, needs to stay here until the complete message is received, chunk will then will be copied to start of data buffer
            // props below define that copy
            g_ltem1->mqtt->firstChunkBegin = urcPrefix;
            g_ltem1->mqtt->firstChunkSz = g_ltem1->iop->rxCmdBuf->head - urcPrefix;
            g_ltem1->iop->rxDataPeer = iopDataPeer_MQTT;
        }

        else if (g_ltem1->iop->peerTypeMap.mqttConnection && memcmp("+QMTSTAT:", urcPrefix, strlen("+QMTSTAT:")) == 0)
        {
            PRINTF(dbgColor_cyan, "-e=mqttS");
            // todo mark mqtt connection closed
        }

        else if (g_ltem1->iop->peerTypeMap.pdpContext && memcmp("+QIURC: \"pdpdeact", urcPrefix, strlen("+QIURC: \"pdpdeact")) == 0)
        {
            PRINTF(dbgColor_cyan, "-e=pdpD");

            char *connIdPtr = g_ltem1->iop->rxCmdBuf->prevHead + strlen("+QIURC: \"pdpdeact");
            char *endPtr = NULL;
            uint8_t contextId = (uint8_t)strtol(connIdPtr, &endPtr, 10);
            g_ltem1->network->contexts[contextId].contextState = 0;
            g_ltem1->network->contexts[contextId].ipAddress[0] = '\0';
            // discard this chunk, processed here
            g_ltem1->iop->rxCmdBuf->head = g_ltem1->iop->rxCmdBuf->prevHead;
        }
    }

    else if (g_ltem1->qbgReadyState != qbg_readyState_appReady && memcmp("\r\nAPP RDY", g_ltem1->iop->rxCmdBuf->prevHead, strlen("\r\nAPP RDY")) == 0)
    {
        PRINTF(dbgColor_cyan, "-e=aRdy");
        g_ltem1->qbgReadyState = qbg_readyState_appReady;
        // discard this chunk, processed here
        g_ltem1->iop->rxCmdBuf->head = g_ltem1->iop->rxCmdBuf->prevHead;
    }
}



/**
 *	\brief ISR for NXP bridge interrupt events; primary (1st bridge chunk) read/write actions to LTEm1.
 */
static void interruptCallbackISR()
{
    /* ----------------------------------------------------------------------------------------------------------------
     * NOTE: The IIR, TXLVL and RXLVL are read seemingly redundantly, this is required to ensure NXP SC16IS741
     * IRQ line is reset (belt AND suspenders).  During initial testing it was determined that without this 
     * duplication of register reads IRQ would latch active randomly.
     * ------------------------------------------------------------------------------------------------------------- */
    /*
    * IIR servicing:
    *   read (RHR) : buffer full (need to empty), timeout (chars recv'd, buffer not full but no more coming)
    *   write (THR): buffer emptied sufficiently to send more chars
    */

    SC16IS741A_IIR iirVal;
    uint8_t rxLevel;
    uint8_t txAvailable;

    iirVal.reg = sc16is741a_readReg(SC16IS741A_IIR_ADDR);
    retryIsr:
    PRINTF(dbgColor_white, "\rISR[");

    do
    {
        while(iirVal.IRQ_nPENDING == 1)                             // wait for register, IRQ was signaled
        {
            iirVal.reg = sc16is741a_readReg(SC16IS741A_IIR_ADDR);
            PRINTF(dbgColor_warn, "*");
        }


        if (iirVal.IRQ_SOURCE == 3)                                 // priority 1 -- receiver line status error : clear fifo of bad char
        {
            PRINTF(dbgColor_error, "RXErr ");
            sc16is741a_flushRxFifo();
        }

        
        if (iirVal.IRQ_SOURCE == 2 || iirVal.IRQ_SOURCE == 6)       // priority 2 -- receiver RHR full (src=2), receiver time-out (src=6)
        {                                                           // Service Action: read RXLVL, read FIFO to empty
            PRINTF(dbgColor_gray, "RX=%d ", iirVal.IRQ_SOURCE);
            rxLevel = sc16is741a_readReg(SC16IS741A_RXLVL_ADDR);
            PRINTF(dbgColor_gray, "-lvl=%d ", rxLevel);

            if (rxLevel > 0)
            {
                if (g_ltem1->iop->rxDataPeer == iopDataPeer__NONE)
                {
                    PRINTF(dbgColor_magenta, "-cmd ");
                    sc16is741a_read(g_ltem1->iop->rxCmdBuf->head, rxLevel);
                    g_ltem1->iop->rxCmdBuf->prevHead = g_ltem1->iop->rxCmdBuf->head;    // save last head if RX moved/discarded
                    g_ltem1->iop->rxCmdBuf->head += rxLevel;
                    //PRINTF(dbgColor_info, "c=%s", g_ltem1->iop->rxCmdBuf->prevHead);
                    iop_rxParseImmediate();                 // parse recv'd for immediate process/discard (ex switch to data context)
                }

                else if (g_ltem1->iop->rxDataPeer < iopDataPeer__SOCKET_CNT)            // TCP/UDP/SSL 
                {
                    PRINTF(dbgColor_magenta, "-sckt ");

                    if (g_ltem1->iop->rxDataBufIndx == IOP_NO_BUFFER)
                    {
                        g_ltem1->iop->rxDataBufIndx = getDataBuffer(g_ltem1->iop->rxDataPeer);
                    }
                    sc16is741a_read(g_ltem1->iop->rxDataBufs[g_ltem1->iop->rxDataBufIndx]->head, rxLevel);                      // read data from LTEm1
                    g_ltem1->iop->rxDataBufs[g_ltem1->iop->rxDataBufIndx]->prevHead = g_ltem1->iop->rxDataBufs[g_ltem1->iop->rxDataBufIndx]->head;
                    g_ltem1->iop->rxDataBufs[g_ltem1->iop->rxDataBufIndx]->head += rxLevel;
                    //PRINTF(dbgColor_info, "d=%s", g_ltem1->iop->rxDataBufs[g_ltem1->iop->rxDataBufIndx]->buffer);
                }

                // MQTT is unique: data is announced and delivered in same msg. Other data sources announce data, then you request it.
                else if (g_ltem1->iop->rxDataPeer == iopDataPeer_MQTT)
                {
                    PRINTF(dbgColor_magenta, "-mqtt ");

                    if (g_ltem1->iop->rxDataBufIndx == IOP_NO_BUFFER)
                    {
                        g_ltem1->iop->rxDataBufIndx = getDataBuffer(g_ltem1->iop->rxDataPeer);
                        g_ltem1->iop->rxDataBufs[g_ltem1->iop->rxDataBufIndx]->head += g_ltem1->mqtt->firstChunkSz;
                    }
                    sc16is741a_read(g_ltem1->iop->rxDataBufs[g_ltem1->iop->rxDataBufIndx]->head, rxLevel);                      // read data from LTEm1
                    g_ltem1->iop->rxDataBufs[g_ltem1->iop->rxDataBufIndx]->prevHead = g_ltem1->iop->rxDataBufs[g_ltem1->iop->rxDataBufIndx]->head;
                    g_ltem1->iop->rxDataBufs[g_ltem1->iop->rxDataBufIndx]->head += rxLevel;

                    if (strncmp(ASCII_sCRLF, g_ltem1->iop->rxDataBufs[g_ltem1->iop->rxDataBufIndx]->head - 2, 2) == 0)      // test last recv'd for end-of-msg
                    {
                        // copy first chunk from rxCmdBuf (original URC recv'd data)
                        memcpy(g_ltem1->iop->rxDataBufs[g_ltem1->iop->rxDataBufIndx]->buffer, g_ltem1->mqtt->firstChunkBegin, g_ltem1->mqtt->firstChunkSz);
                        g_ltem1->mqtt->dataBufferIndx = g_ltem1->iop->rxDataBufIndx;                            // recv complete hand off to MQTT
                        g_ltem1->iop->rxDataBufIndx = IOP_NO_BUFFER;                                            // IOP release this buffer, now owned by MQTT
                        g_ltem1->iop->rxDataPeer = iopDataPeer__NONE;
                    }
                }
            }
        }

        if (iirVal.IRQ_SOURCE == 1)                                 // priority 3 -- transmit THR (threshold) : TX ready for more data
        {
            uint8_t buf[65] = {0};
            uint8_t thisTxSz;

            txAvailable = sc16is741a_readReg(SC16IS741A_TXLVL_ADDR);
            PRINTF(dbgColor_gray, "TX ");
            PRINTF(dbgColor_gray, "-lvl=%d ", txAvailable);

            if ((thisTxSz = txTake(buf, txAvailable)) > 0)
            {
                PRINTF(dbgColor_dCyan, "txChunk=%s", buf);
                sc16is741a_write(buf, thisTxSz);
            }

            // if (g_ltem1->iop->txCtrl->remainSz > 0)
            // {
            //     txSendNextChunk(txAvailable);
            //     PRINTF_GRAY("-chunk ");
            // }
        }

        /* -- NOT USED --
        // priority 4 -- modem interrupt
        // priority 6 -- receive XOFF/SpecChar
        // priority 7 -- nCTS, nRTS state change:
        */

        iirVal.reg = sc16is741a_readReg(SC16IS741A_IIR_ADDR);

    } while (iirVal.IRQ_nPENDING == 0);

    PRINTF(dbgColor_white, "]\r");

    gpioPinValue_t irqPin = gpio_readPin(g_ltem1->pinConfig.irqPin);
    if (irqPin == gpioValue_low)
    {
        txAvailable = sc16is741a_readReg(SC16IS741A_TXLVL_ADDR);
        rxLevel = sc16is741a_readReg(SC16IS741A_RXLVL_ADDR);
        iirVal.reg = sc16is741a_readReg(SC16IS741A_IIR_ADDR);
        PRINTF(dbgColor_warn, "IRQ failed to reset!!! nIRQ=%d, iir=%d, txLvl=%d, rxLvl=%d \r", iirVal.IRQ_nPENDING, iirVal.reg, txAvailable, rxLevel);

        //ltem1_faultHandler(500, "IRQ failed to reset.");
        goto retryIsr;
    }
}


#pragma endregion

