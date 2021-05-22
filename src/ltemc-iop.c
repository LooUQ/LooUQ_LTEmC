/******************************************************************************
 *  \file ltemc-iop.c
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020, 2021 LooUQ Incorporated.
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
 * IOP is the Input/Output processor for the LTEm line of LooUQ IoT modems. It
 * manages the IO via SPI-UART bridge and multiplexes cmd\data streams.
 *****************************************************************************/

#define _DEBUG 0                        // set to non-zero value for PRINTF debugging output, 
// debugging output options             // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if defined(_DEBUG) && _DEBUG > 0
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG == 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#endif


#include "ltemc.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define IOP_RXCTRLBLK_ADVINDEX(INDX) INDX = (++INDX == IOP_RXCTRLBLK_COUNT) ? 0 : INDX

#define QBG_APPREADY_MILLISMAX 5000


// shortcut to this
static iop_t *iopPtr;
// peers
static sockets_t *scktPtr;
static mqtt_t *mqttPtr;

// private function declarations
static cbuf_t *s_txBufCreate();
static iopBuffer_t *s_rxBufCreate();
static uint16_t s_txPut(const char *data, uint16_t dataSz);
static uint16_t s_txTake(char *data, uint16_t dataSz);
// static void txSendChunk();
static void s_rxBufReset(iopBuffer_t *rxBuf);
static uint8_t s_getDataBuffer(iopDataPeer_t dataPeer);
static void s_interruptCallbackISR();


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
void iop_create()
{
    iopPtr = calloc(1, sizeof(iop_t));
	if (iopPtr == NULL)
	{
        ltem_notifyApp(ltemNotifType_memoryAllocFault, "iop-could not alloc iop status struct");
	}
    iopPtr->txBuf = s_txBufCreate();
    iopPtr->rxCmdBuf = s_rxBufCreate(IOP_RX_CMDBUF_SZ);      // create cmd/default RX buffer
    iopPtr->rxDataPeer = iopDataPeer__NONE;
    iopPtr->rxDataBufIndx = IOP_NO_BUFFER;
    iopPtr->rxDataBufs[0] = s_rxBufCreate(IOP_RX_DATABUF_SZ);      // create cmd/default RX buffer

    // set global reference
    g_ltem->iop = iopPtr;
}


void iop_registerProtocol(ltemOptnModule_t proto, void *protoPtr)
{
    switch (proto)
    {
    case ltemOptnModule_sockets:
        scktPtr = protoPtr;
        break;
    case ltemOptnModule_mqtt:
        mqttPtr = protoPtr;
        break;
    }
}


/**
 *	\brief Complete initialization and start running iop processes.
 */
void iop_start()
{
    // attach ISR and enable NXP interrupt mode
    gpio_attachIsr(g_ltem->pinConfig.irqPin, true, gpioIrqTriggerOn_falling, s_interruptCallbackISR);
    spi_protectFromInterrupt(g_ltem->spi, g_ltem->pinConfig.irqPin);
    sc16is741a_enableIrqMode();
}



/**
 *	\brief Verify LTEm1 is ready for driver operations.
 */
void iop_awaitAppReady()
{
    unsigned long apprdyWaitStart = lMillis();
    while (g_ltem->qbgReadyState < qbg_readyState_appReady)
    {
        lYield();
        if (apprdyWaitStart + QBG_APPREADY_MILLISMAX < lMillis())
        ltem_notifyApp(ltemNotifType_hwInitFailed,  "qbg-BGx module failed to start in the allowed time");
    }
}



/**
 *	\brief Start a (raw) send operation.
 *
 *  \param sendData [in] - Pointer to char data to send out, input buffer can be discarded following call.
 *  \param sendSz [in] - The number of characters to send.
 *  \param sendReady [in] - If true, queue sendData then initiate the actual send process. If false continue queueing and wait to send.
 * 
 *  \return Number of characters queued for sending.
 */
uint16_t iop_txSend(const char *sendData, uint16_t sendSz, bool sendReady)
{
    uint16_t queuedSz = s_txPut(sendData, sendSz);    // put sendData into global send buffer
    // if (queuedSz < sendSz)
    //     ltem1_notifyApp(ltem1NotifType_bufferOverflow, "iop-tx buffer overflow");

    if (sendReady)                                          // if sender done adding, send initial block of data
    {                                                       // IOP ISR will continue sending chunks until global buffer empty
        char txData[SC16IS741A_FIFO_BUFFER_SZ] = {0};       // max size of the NXP TX FIFO
        do
        {
            uint16_t dataAvail = s_txTake(txData, sc16is741a_readReg(SC16IS741A_TXLVL_ADDR));

            //ASSERTBRK(dataAvail > 0);
            if (dataAvail == 0) {
                PRINTF(dbgColor_warn, "txSnd-noData dA=%d, rtry=%d\r", dataAvail, sc16is741a_readReg(SC16IS741A_TXLVL_ADDR)); }

            if (dataAvail > 0)
            {
                PRINTF(dbgColor_dCyan, "txChunk=%s\r", txData);
                sc16is741a_write(txData, dataAvail);
                break;
            }
        } while (true);                                     // loop until send at least 1 char
    }
    return queuedSz;                                        // return number of chars queued, not char send cnt
}



/**
 *	\brief Clear receive command response buffer.
 */
void iop_resetCmdBuffer()
{
    s_rxBufReset(iopPtr->rxCmdBuf);
}



/**
 *	\brief Clear receive command response buffer.
 *
 *  \param bufIndx [in] The IOP data buffer array index. Data buffers are referenced by an array of pointers.
 */
void iop_resetDataBuffer(uint8_t bufIndx)
{
    s_rxBufReset(iopPtr->rxDataBufs[bufIndx]);
}



/**
 *	\brief Response parser looking for ">" prompt to send data to network and then initiates the send.
 *
 *  \param response [in] The character string received from BGx (so far, may not be complete).
 *  \param endptr [out] Pointer to char immediately following match, nullptr for error.
 * 
 *  \return Result code enum value (http status code)
 */
resultCode_t iop_txDataPromptParser(const char *response, char **endptr)
{
    *endptr = strstr(response, "> ");
    if (*endptr != NULL)
    {
        *endptr += 2;                   // point past data prompt
        return RESULT_CODE_SUCCESS;
    }
    *endptr = strstr(response, "ERROR");
    if (*endptr != NULL)
    {
        return RESULT_CODE_ERROR;
    }
    return RESULT_CODE_PENDING;
}


#pragma endregion


/* private local (static) functions
------------------------------------------------------------------------------------------------ */
#pragma region private functions

/**
 *	\brief Create the IOP transmit buffer.
 */
static cbuf_t *s_txBufCreate()
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


/**
 *	\brief Create a receive buffer, the buffer could be a command or data stream buffer.
 *
 *  \param bufSz [in] Size of the buffer to create.
 */
static iopBuffer_t *s_rxBufCreate(size_t bufSz)
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


/**
 *	\brief Get the index of the data buffer for a dataPeer (ex: socket). Note: peers can have one data buffer.
 *
 *  \param dataPeer [in] Struct representing the dataPeer.
 */
static uint8_t s_getDataBuffer(iopDataPeer_t dataPeer)
{
    for (size_t i = 0; i < IOP_RX_DATABUFFERS_MAX; i++)                         // return buffer already assigned to dataPeer, if exists
    {
        if (iopPtr->rxDataBufs[i] != NULL && iopPtr->rxDataBufs[i]->dataPeer == dataPeer)
            return i;
    }

    for (size_t i = 0; i < IOP_RX_DATABUFFERS_MAX; i++)                         // otherwise, look for empty buffer or create a new buffer to up to buf cnt limit
    {
        if (iopPtr->rxDataBufs[i] != NULL && iopPtr->rxDataBufs[i]->dataPeer == iopDataPeer__NONE)
        {
            iopPtr->rxDataBufs[i]->dataPeer = dataPeer;
            return i;
        }
        else if (iopPtr->rxDataBufs[i] == NULL)                       // empty buffer slot, create a new buffer & assign to peer
        {
            iopPtr->rxDataBufs[i] = s_rxBufCreate(IOP_RX_DATABUF_SZ);
            iopPtr->rxDataBufs[i]->dataPeer = dataPeer;
            return i;
        }
    }
    return IOP_NO_BUFFER;
}



/**
 *	\brief Reset a receive buffer for reuse.
 *
 *  \param rxBuf [in] pointer to the buffer struct.
 */
static void s_rxBufReset(iopBuffer_t *rxBuf)
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
 *  \brief Puts data into the TX buffer control structure. NOTE: this operation performs a copy.
 * 
 *  \param data [in] - Pointer to where source data is now. 
 *  \param dataSz [in] - How much data to put in the TX struct.
 * 
 *  \return The number of bytes of actual queued in the TX struct, compare to dataSz to determine if all data queued. 
*/
static uint16_t s_txPut(const char *data, uint16_t dataSz)
{
    uint16_t putCnt = 0;

    for (size_t i = 0; i < dataSz; i++)
    {
        if(cbuf_push(iopPtr->txBuf, data[i]))
        {
            putCnt++;
            continue;
        }
        break;
    }
    iopPtr->txPend += putCnt;
    return putCnt;
}



/**
 *  \brief Gets (dequeues) data from the TX buffer control structure.
 * 
 *  \param data [in] - Pointer to where taken data will be placed.
 *  \param dataSz [in] - How much data to take, if possible.
 * 
 *  \return The number of bytes of data being returned. 
*/
static uint16_t s_txTake(char *data, uint16_t dataSz)
{
    uint16_t takeCnt = 0;

    for (size_t i = 0; i < dataSz; i++)
    {
        if (cbuf_pop(iopPtr->txBuf, data + i))
        {
            takeCnt++;
            continue;
        }
        break;
    }
    iopPtr->txPend -= takeCnt;
    return takeCnt;
}

#pragma endregion



#pragma region ISR

/**
 *  \brief Parse recv'd data (in command RX buffer) for async event preambles that need to be handled immediately.
*/
void iop_rxParseImmediate()
{
    char *urcPrefix = memchr(iopPtr->rxCmdBuf->prevHead, '+', 6);             // all URC start with '+', skip leading \r\n 
    if (urcPrefix)
    {
        if (iopPtr->peerTypeMap.sslSocket && memcmp("+QSSLURC: \"recv", urcPrefix, strlen("+QSSLURC: \"recv")) == 0)
        {
            PRINTF(dbgColor_cyan, "-p=sslURC");
            char *connIdPtr = iopPtr->rxCmdBuf->prevHead + strlen("+QSSLURC: \"recv");
            char *endPtr = NULL;
            uint8_t socketId = (uint8_t)strtol(connIdPtr, &endPtr, 10);
            scktPtr->socketCtrls[socketId + iopDataPeer__SOCKET].dataPending = true;
            // discard this chunk, processed here
            iopPtr->rxCmdBuf->head = iopPtr->rxCmdBuf->prevHead;
        }

        else if (iopPtr->peerTypeMap.tcpudpSocket && memcmp("+QIURC: \"recv", urcPrefix, strlen("+QIURC: \"recv")) == 0)
        {
            PRINTF(dbgColor_cyan, "-p=ipURC");
            char *connIdPtr = iopPtr->rxCmdBuf->prevHead + strlen("+QIURC: \"recv");
            char *endPtr = NULL;
            uint8_t socketId = (uint8_t)strtol(connIdPtr, &endPtr, 10);
            scktPtr->socketCtrls[socketId + iopDataPeer__SOCKET].dataPending = true;
            // discard this chunk, processed here
            iopPtr->rxCmdBuf->head = iopPtr->rxCmdBuf->prevHead;
        }

        else if (iopPtr->peerTypeMap.mqttSubscribe && memcmp("+QMTRECV:", urcPrefix, strlen("+QMTRECV:")) == 0)
        {
            PRINTF(dbgColor_cyan, "-p=mqttR");
            // this chunk, needs to stay here until the complete message is received, chunk will then will be copied to start of data buffer
            // props below define that copy
            mqttPtr->firstChunkBegin = urcPrefix;
            mqttPtr->firstChunkSz = iopPtr->rxCmdBuf->head - urcPrefix;
            iopPtr->rxDataPeer = iopDataPeer_MQTT;
        }

        else if (iopPtr->peerTypeMap.mqttConnection && memcmp("+QMTSTAT:", urcPrefix, strlen("+QMTSTAT:")) == 0)
        {
            PRINTF(dbgColor_cyan, "-p=mqttS");
            mqttPtr->state = mqttStatus_closed;
            // todo mark mqtt connection closed
        }

        else if (iopPtr->peerTypeMap.pdpContext && memcmp("+QIURC: \"pdpdeact", urcPrefix, strlen("+QIURC: \"pdpdeact")) == 0)
        {
            PRINTF(dbgColor_cyan, "-p=pdpD");

            char *connIdPtr = iopPtr->rxCmdBuf->prevHead + strlen("+QIURC: \"pdpdeact");
            char *endPtr = NULL;
            uint8_t contextId = (uint8_t)strtol(connIdPtr, &endPtr, 10);
            for (size_t i = 0; i < BGX_PDPCONTEXT_COUNT; i++)
            {
                if (g_ltem->network->pdpCntxts[i].contextId == contextId)
                {
                    g_ltem->network->pdpCntxts[i].contextId = 0;
                    g_ltem->network->pdpCntxts[i].ipAddress[0] = 0;
                    break;
                }
            }
            // discard this chunk, processed here
            iopPtr->rxCmdBuf->head = iopPtr->rxCmdBuf->prevHead;
        }
    }

    else if (g_ltem->qbgReadyState != qbg_readyState_appReady && memcmp("\r\nAPP RDY", iopPtr->rxCmdBuf->prevHead, strlen("\r\nAPP RDY")) == 0)
    {
        PRINTF(dbgColor_cyan, "-p=aRdy");
        g_ltem->qbgReadyState = qbg_readyState_appReady;
        // discard this chunk, processed here
        iopPtr->rxCmdBuf->head = iopPtr->rxCmdBuf->prevHead;
    }
}



/**
 *	\brief ISR for NXP UART interrupt events, the NXP UART performs all serial I/O with BGx.
 */
static void s_interruptCallbackISR()
{
    /* ----------------------------------------------------------------------------------------------------------------
     * NOTE: The IIR, TXLVL and RXLVL are read seemingly redundantly, this is required to ensure NXP SC16IS741
     * IRQ line is reset (belt AND suspenders).  During initial testing it was determined that without this 
     * duplication of register reads IRQ would latch in active IRQ state randomly.
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
            PRINTF(dbgColor_gray, "-sz=%d ", rxLevel);

            if (rxLevel > 0)
            {
                if (iopPtr->rxDataPeer == iopDataPeer__NONE)
                {
                    PRINTF(dbgColor_magenta, "-cmd ");
                    sc16is741a_read(iopPtr->rxCmdBuf->head, rxLevel);
                    iopPtr->rxCmdBuf->prevHead = iopPtr->rxCmdBuf->head;    // save last head if RX moved/discarded
                    iopPtr->rxCmdBuf->head += rxLevel;
                    //PRINTF(dbgColor_info, "c=%s", iopPtr->rxCmdBuf->prevHead);
                    iop_rxParseImmediate();                 // parse recv'd for immediate process/discard (ex switch to data context)
                }

                else if (iopPtr->rxDataPeer < iopDataPeer__SOCKET_CNT)            // TCP/UDP/SSL 
                {
                    PRINTF(dbgColor_magenta, "-sckt ");

                    if (iopPtr->rxDataBufIndx == IOP_NO_BUFFER)
                    {
                        iopPtr->rxDataBufIndx = s_getDataBuffer(iopPtr->rxDataPeer);
                    }
                    sc16is741a_read(iopPtr->rxDataBufs[iopPtr->rxDataBufIndx]->head, rxLevel);                      // read data from LTEm1
                    iopPtr->rxDataBufs[iopPtr->rxDataBufIndx]->prevHead = iopPtr->rxDataBufs[iopPtr->rxDataBufIndx]->head;
                    iopPtr->rxDataBufs[iopPtr->rxDataBufIndx]->head += rxLevel;
                    //PRINTF(dbgColor_info, "d=%s", iopPtr->rxDataBufs[iopPtr->rxDataBufIndx]->buffer);
                }

                // MQTT is unique: data is announced and delivered in same msg. Other data sources announce data, then you request it.
                else if (iopPtr->rxDataPeer == iopDataPeer_MQTT)
                {
                    PRINTF(dbgColor_magenta, "-mqtt ");

                    if (iopPtr->rxDataBufIndx == IOP_NO_BUFFER)
                    {
                        iopPtr->rxDataBufIndx = s_getDataBuffer(iopPtr->rxDataPeer);
                        iopPtr->rxDataBufs[iopPtr->rxDataBufIndx]->head += mqttPtr->firstChunkSz;
                    }
                    sc16is741a_read(iopPtr->rxDataBufs[iopPtr->rxDataBufIndx]->head, rxLevel);                      // read data from LTEm1
                    iopPtr->rxDataBufs[iopPtr->rxDataBufIndx]->prevHead = iopPtr->rxDataBufs[iopPtr->rxDataBufIndx]->head;
                    iopPtr->rxDataBufs[iopPtr->rxDataBufIndx]->head += rxLevel;

                    if (strncmp(ASCII_sCRLF, iopPtr->rxDataBufs[iopPtr->rxDataBufIndx]->head - 2, 2) == 0)      // test last recv'd for end-of-msg
                    {
                        // copy first chunk from rxCmdBuf (original URC recv'd data)
                        memcpy(iopPtr->rxDataBufs[iopPtr->rxDataBufIndx]->buffer, mqttPtr->firstChunkBegin, mqttPtr->firstChunkSz);
                        mqttPtr->dataBufferIndx = iopPtr->rxDataBufIndx;                            // recv complete hand off to MQTT
                        iopPtr->rxDataBufIndx = IOP_NO_BUFFER;                                            // IOP release this buffer, now owned by MQTT
                        iopPtr->rxDataPeer = iopDataPeer__NONE;
                    }
                }
            }
        }

        if (iirVal.IRQ_SOURCE == 1)                                 // priority 3 -- transmit THR (threshold) : TX ready for more data
        {
            uint8_t buf[SC16IS741A_FIFO_BUFFER_SZ] = {0};
            uint8_t thisTxSz;

            txAvailable = sc16is741a_readReg(SC16IS741A_TXLVL_ADDR);
            PRINTF(dbgColor_gray, "TX ");
            PRINTF(dbgColor_gray, "-sz=%d ", txAvailable);

            if ((thisTxSz = s_txTake(buf, txAvailable)) > 0)
            {
                PRINTF(dbgColor_dCyan, "txChunk=%s", buf);
                sc16is741a_write(buf, thisTxSz);
            }
        }

        /* -- NOT USED --
        // priority 4 -- modem interrupt
        // priority 6 -- receive XOFF/SpecChar
        // priority 7 -- nCTS, nRTS state change:
        */

        iirVal.reg = sc16is741a_readReg(SC16IS741A_IIR_ADDR);

    } while (iirVal.IRQ_nPENDING == 0);

    PRINTF(dbgColor_white, "]\r");

    gpioPinValue_t irqPin = gpio_readPin(g_ltem->pinConfig.irqPin);
    if (irqPin == gpioValue_low)
    {
        iirVal.reg = sc16is741a_readReg(SC16IS741A_IIR_ADDR);
        txAvailable = sc16is741a_readReg(SC16IS741A_TXLVL_ADDR);
        rxLevel = sc16is741a_readReg(SC16IS741A_RXLVL_ADDR);

        PRINTF(dbgColor_warn, "IRQ failed to reset!!! nIRQ=%d, iir=%d, txLvl=%d, rxLvl=%d \r", iirVal.IRQ_nPENDING, iirVal.reg, txAvailable, rxLevel);
        //ltem1_notifyApp(ltem1NotifType_resetFailed, "IRQ failed to reset.");
        goto retryIsr;
    }
}


#pragma endregion

