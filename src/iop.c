/******************************************************************************
 *  \file iop.h
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

#include "ltem1c.h"
//#include "components\nxp_sc16is741a.h"


#define RECV_HEADERSZ_URC_IPRECV 13
#define RECV_HEADERSZ_URC_SSLRECV 15
#define RECV_HEADERSZ_IRD 5
#define RECV_HEADERSZ_APPRDY 9
#define RECV_HEADERSZ_URCSTATE 8

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define ADV_RXCTRLBLK_INDEX(BFINDX) BFINDX = (++BFINDX == IOP_RX_CTRLBLOCK_COUNT) ? 0 : BFINDX


// private function declarations

static void txSendNextChunk(uint8_t fifoAvailable);

static uint8_t rxOpenCtrlBlock();
static void rxParseRecvContentPreamble(uint8_t rxIndx);
static void rxProcessSocketIrd(iopRxCtrlBlock_t *rxCtrlStruct, socket_t socketNm);
static void rxResetCtrlBlock(uint8_t bufIndx);

static void interruptCallbackISR();


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

    iop->txCtrl = calloc(1, sizeof(iopTxCtrlBlock_t));
	if (iop->txCtrl == NULL)
	{
        free(iop);
        ltem1_faultHandler(0, "iop-could not alloc iop->txCtrl status struct");
	}

    iop->txCtrl->txBuf = calloc(IOP_TX_BUFFER_SZ + 1, sizeof(char));
    if (iop->txCtrl->txBuf == NULL)
    {
        free(iop);
        free(iop->txCtrl);
        ltem1_faultHandler(0, "iop-could not alloc iop TX buffers");
    }

    // init TX and RX struct values
    iop->txCtrl->remainSz = 0;
    iop->cmdHead = iop->cmdTail = 0;
    for (size_t i = 0; i < IOP_SOCKET_COUNT; i++)
    {
        iop->socketHead[i] = iop->socketTail[i] = 0;
    }
    return iop;
}



/**
 *	\brief Complete initialization and start running iop processes.
 */
void iop_start()
{
    // attach ISR and enable NXP interrupt mode
    gpio_attachIsr(g_ltem1->gpio->irqPin, true, gpioIrqTriggerOn_falling, interruptCallbackISR);
    spi_protectFromInterrupt(g_ltem1->spi, g_ltem1->gpio->irqPin);
    sc16is741a_enableIrqMode();
}



/**
 *	\brief Tear down IOP (Input/Output Process) subsystem.
 */
void iop_destroy(void *iop)
{
	free(iop);
}



/**
 *	\brief Start a (raw) send operation.
 *
 *  \param[in] sendData Pointer to char data to send out, input buffer can be discarded following call.
 *  \param[in] sendSz The number of characters to send.
 *
 *  \return True if send is completes.
 */
void iop_txSend(const char *sendData, uint16_t sendSz)
{
    // if(IOP_TX_ACTIVE())
    //     return iop_xfrResult_busy;

    if (sendSz > IOP_TX_BUFFER_SZ)
        ltem1_faultHandler(500, "iop-send data max size exceeded");

    //g_ltem1->iop->txCtrl->txActive = true;
    memcpy(g_ltem1->iop->txCtrl->txBuf, sendData, sendSz);

    uint16_t thisChunkSz = MIN(sendSz, SC16IS741A_FIFO_BUFFER_SZ);
    sc16is741a_write(g_ltem1->iop->txCtrl->txBuf, thisChunkSz);
    while (g_ltem1->iop->txCtrl->remainSz)
    {
        timing_yield();
    }

    // if (sendSz <= SC16IS741A_FIFO_BUFFER_SZ)
    // {
    //     g_ltem1->iop->txCtrl->remainSz = 0;
    //     return iop_xfrResult_complete;
    // }
    // else
    // {
    //     g_ltem1->iop->txCtrl->remainSz = sendSz - thisChunkSz;
    //     g_ltem1->iop->txCtrl->chunkPtr = g_ltem1->iop->txCtrl->txBuf + thisChunkSz;
    // }
    // return iop_xfrResult_incomplete;
}




/**
 *	 \brief Dequeue received data.
 *
 *   \param[in] recvBuf Pointer to the command receive buffer.
 *   \param[in] recvMaxSz The command response max length (buffer size).
 */
iopXfrResult_t iop_rxGetCmdQueued(char *recvBuf, uint16_t recvBufSz)
{
    if ( !g_ltem1->iop->rxCtrlBlks[g_ltem1->iop->cmdHead].occupied )
        return iopXfrResult_incomplete;

    uint8_t tail = g_ltem1->iop->cmdTail;
    // memset(recvBuf, 0, recvBufSz);

    do
    {
        if (g_ltem1->iop->rxCtrlBlks[tail].occupied && g_ltem1->iop->rxCtrlBlks[tail].process == iopProcess_command)
        {
            uint16_t copySz = MIN(recvBufSz, g_ltem1->iop->rxCtrlBlks[tail].primSz);
            strncpy(recvBuf, g_ltem1->iop->rxCtrlBlks[tail].primBuf, copySz + 1);
            rxResetCtrlBlock(tail);
            recvBuf += copySz;
            recvBufSz -= copySz;                    // tally to check for overflow
        }

        if (tail == g_ltem1->iop->cmdHead)
            break;

        tail = ADV_RXCTRLBLK_INDEX(tail);
        if (tail == g_ltem1->iop->cmdTail)
            ltem1_faultHandler(500, "iop-buffer underflow in iop_rxGetCmdQueued");

    } while (true);
    g_ltem1->iop->cmdTail = tail;
    
    if (recvBufSz < 0)
        return iopXfrResult_truncated;

    return iopXfrResult_complete;
}


/**
 *   \brief Transfer received data from iop buffers to application.
 * 
 *   \param[in] recvBuf Pointer to the command receive buffer.
 *   \param[in] recvMaxSz The command response max length (buffer size).
 * 
 *   \return The number of bytes received (since previous call).
*/
uint16_t iop_rxGetSocketQueued(socket_t socketNm, char *recvBuf, uint16_t recvBufSz)
{
    uint16_t recvdBytes;
    int8_t tail = g_ltem1->iop->socketTail[socketNm];

    if ( !g_ltem1->iop->rxCtrlBlks[tail].occupied )
        return 0;

    // if extended buffer in use, copy primary to start of extended buffer
    // determine which buffer (prim, aux) to return pointer to
    if (g_ltem1->iop->rxCtrlBlks[tail].extsnBufHead)
    {
        recvdBytes == g_ltem1->iop->rxCtrlBlks[tail].extsnBufTail - g_ltem1->iop->rxCtrlBlks[tail].extsnBufHead;
        memcpy(recvBuf, g_ltem1->iop->rxCtrlBlks[tail].primBuf, g_ltem1->iop->rxCtrlBlks[tail].primSz);
        memcpy(recvBuf + recvdBytes, g_ltem1->iop->rxCtrlBlks[tail].primBuf, recvdBytes);
        recvdBytes += g_ltem1->iop->rxCtrlBlks[tail].primSz;
    }
    else
    {
        // offset IRD header from g_ltem1->iop->rxCtrlBlks[currIndx].primSz
        recvdBytes = MIN(g_ltem1->iop->rxCtrlBlks[tail].primSz, recvBufSz);
        memcpy(recvBuf, g_ltem1->iop->rxCtrlBlks[tail].dataStart, recvdBytes);
    }

    free(g_ltem1->iop->rxCtrlBlks[tail].extsnBufHead);          // if extended buffer, free malloc'd buffer space
    //iop_tailFinalize(socketNm);
    return recvdBytes;
}



/**
 *   \brief Closes the tail (control block) of the socket stream.
 * 
 *   \param[in] The socket number to close tail buffer and update controls.
*/
void iop_tailFinalize(socket_t socketNm)
{
    uint8_t tail = g_ltem1->iop->socketTail[socketNm];

    if (g_ltem1->iop->rxCtrlBlks[tail].occupied)
        rxResetCtrlBlock(tail);

    do                                                          // advance tail, next available message or head
    {
        if (tail == g_ltem1->iop->socketHead[socketNm])
        {
            return;
        }

        tail = ADV_RXCTRLBLK_INDEX(tail);

        if (g_ltem1->iop->rxCtrlBlks[tail].occupied && g_ltem1->iop->rxCtrlBlks[tail].process == socketNm)  // at next socket rxCtrlBlk
        {
            g_ltem1->iop->socketTail[socketNm] = tail;
            return;
        }
    } while (tail != g_ltem1->iop->socketTail[socketNm]);

    ltem1_faultHandler(500, "iop-iop_tailFinalize-buffer underflow");
}


#pragma endregion


/* private local (static) functions
------------------------------------------------------------------------------------------------ */
#pragma region private functions


/**
 *	\brief Send out next chunk (NXP bridge buffer) of data. *** CALLED FROM ISR, TX BUFFER EMPTY ***
 */
static void txSendNextChunk(uint8_t fifoAvailable)
{
    uint8_t thisTxSz = MIN(g_ltem1->iop->txCtrl->remainSz, fifoAvailable);
    sc16is741a_write(g_ltem1->iop->txCtrl->chunkPtr, thisTxSz);

    // update TX ctrl with remaining work to complete requested transmit
    g_ltem1->iop->txCtrl->remainSz -= thisTxSz;
    g_ltem1->iop->txCtrl->chunkPtr = g_ltem1->iop->txCtrl->chunkPtr + thisTxSz;
    //g_ltem1->iop->txCtrl->txActive = g_ltem1->iop->txCtrl->remainSz > 0;
}



/**
 *	\brief Scan IOP Buffers structure for next available read destination slot (from BG to host).
 *
 *  \return Index of read buffer location to receive the incoming chars.
 */
static uint8_t rxOpenCtrlBlock()
{
    uint8_t volatile bufIndx = g_ltem1->iop->rxRecvHead;
    do
    {
        bufIndx = ADV_RXCTRLBLK_INDEX(bufIndx);

        if (bufIndx == g_ltem1->iop->rxRecvHead)
            ltem1_faultHandler(500, "iop-rxCtrlBlk overflow, none available");
    }
    while (g_ltem1->iop->rxCtrlBlks[bufIndx].occupied);

    g_ltem1->iop->rxCtrlBlks[bufIndx].occupied = true;
    return g_ltem1->iop->rxRecvHead = bufIndx;
}



/**
 *	\brief Clear and dispose of IOP control stuct resources.
 *
 *	\param[in] bufferIndex The index into the buffer control struct to close.
 */
static void rxResetCtrlBlock(uint8_t bufIndx)
{
    iopRxCtrlBlock_t *rxCtrl = &g_ltem1->iop->rxCtrlBlks[bufIndx];

    rxCtrl->occupied = false;
    rxCtrl->process = iopProcess_void;
    memset(rxCtrl->primBuf, 0, IOP_RX_PRIMARY_BUFFER_SIZE + 1);
    if (rxCtrl->extsnBufHead != NULL)
    {
        free(rxCtrl->extsnBufHead);
        rxCtrl->extsnBufHead = NULL;
        rxCtrl->extsnBufTail = NULL;
    }
}



/**
 *   \brief Scan (in ISR) recv'd 1st stream chunk and prepare expansion buffer for remainder of incoming chunks.
 *
 *   \param[in\out] rxCtrlStruct receive control/buffer structure.
 */
static void rxProcessSocketIrd(iopRxCtrlBlock_t *rxCtrlStruct, socket_t socketNm)
{
    /* note the protocol session is in invoking AT+QIRD=#, not returned in +QIRD result
    *  Example: +QIRD: 4,  where 4 is the number of chars arriving
    *     if chars arriving <= to primbuf size: all good, read will fit in primary
    *     if chars arriving > primbuf size, need to setup expandbuf (malloc)
    */

    char *lenPtr = rxCtrlStruct->primBuf + RECV_HEADERSZ_IRD + 4;
    uint16_t irdLen = strtol(lenPtr, &rxCtrlStruct->dataStart, 10);
    rxCtrlStruct->dataStart += 2;
    rxCtrlStruct->primSz = irdLen;
    g_ltem1->iop->socketIrdBytes[socketNm] = irdLen;

    if (irdLen > IOP_RX_PRIMARY_BUFFER_SIZE)
    {
        uint8_t primLen = strlen(rxCtrlStruct->primBuf);
        rxCtrlStruct->extsnBufHead = calloc(irdLen, sizeof(uint8_t));
        rxCtrlStruct->extsnBufTail = rxCtrlStruct->extsnBufHead + primLen;
    }
    return irdLen;
}



/**
 *	\brief Scan (in ISR) the received data to determine the buffer contents type and process owner.
 *
 *  \param[in\out] rxCtrlStruct receive control/buffer structure.
 */
static void rxParseRecvContentPreamble(uint8_t rxIndx)
{
    /*
		// ** Known Header Patterns	** //
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

    iopRxCtrlBlock_t *rxCtrlBlock = &g_ltem1->iop->rxCtrlBlks[rxIndx];
    rxCtrlBlock->process = iopProcess_command;    //default

    if (strncmp("+QIURC: \"recv", rxCtrlBlock->primBuf + 2, RECV_HEADERSZ_URC_IPRECV) == 0 ||
        strncmp("+QSSLURC: \"recv", rxCtrlBlock->primBuf + 2, RECV_HEADERSZ_URC_SSLRECV) == 0 )
    {
        uint8_t hdrSz = (rxCtrlBlock->primBuf + 5 == 'I') ? RECV_HEADERSZ_URC_IPRECV : RECV_HEADERSZ_URC_SSLRECV;
        char *connIdPtr = rxCtrlBlock->primBuf + hdrSz;
        char *endPtr = NULL;
        uint8_t scktNm = (uint8_t)strtol(connIdPtr, &endPtr, 10);

        rxCtrlBlock->process = scktNm;
        g_ltem1->protocols->sockets[scktNm].dataPending = true;
        g_ltem1->iop->socketHead[scktNm] = rxIndx;
    }

    else if (strncmp("+QIRD", rxCtrlBlock->primBuf + 2, RECV_HEADERSZ_IRD) == 0)
    {
        socket_t socketNm = g_ltem1->iop->irdSocket;

        rxProcessSocketIrd(rxCtrlBlock, socketNm);
        rxCtrlBlock->process = socketNm;
        if (g_ltem1->iop->socketIrdBytes[socketNm] == 0)
        {
            g_ltem1->protocols->sockets[socketNm].dataPending = false;
            rxCtrlBlock->occupied = false;                              // special case: if no data, release ctrlBlk
            rxCtrlBlock->process = iopProcess_void;
        }
        else
            g_ltem1->iop->socketHead[socketNm] = rxIndx;                // data present, so point head to this ctrlBlk

        g_ltem1->iop->irdSocket = protocol_none;
        g_ltem1->action->cmdStr[0] = NULL;                                  // IRD is an action response, close it out
    }

    else if (strncmp("APP RDY\r\n", rxCtrlBlock->primBuf + 2, RECV_HEADERSZ_APPRDY) == 0)
    {
        PRINTF("-appRdy ");
        g_ltem1->qbgReadyState = qbg_readyState_appReady;
        rxCtrlBlock->occupied = false;                                  // release ctrlBlk, done with it
        rxCtrlBlock->process = iopProcess_void;
    }

    // catch async unsolicited state changes here
    else if (strncmp("+QIURC: ", rxCtrlBlock->primBuf + 2, RECV_HEADERSZ_URCSTATE) == 0)
    {
        // state message buffer is only 1 message
        if (g_ltem1->iop->urcStateMsg[0] != NULL)
            ltem1_faultHandler(500, "IOP-URC state msg buffer overflow.");

        strncpy(g_ltem1->iop->urcStateMsg + RECV_HEADERSZ_URCSTATE, rxCtrlBlock->primBuf, IOP_URC_STATEMSG_SZ);

        rxCtrlBlock->occupied = false;                                  // release ctrl/buffer, processed here
        rxCtrlBlock->process = iopProcess_void;
    }

    // if default of command message still holds set command index pointers
    if (rxCtrlBlock->process == iopProcess_command)
    {
        g_ltem1->iop->cmdHead = rxIndx;
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
    PRINTF_WHITE("ISR[");

    do
    {
        while(iirVal.IRQ_nPENDING == 1)
        {
            iirVal.reg = sc16is741a_readReg(SC16IS741A_IIR_ADDR);
            PRINTF_WARN("*");
        }

        // if (iirVal.IRQ_nPENDING == 1)
        // {
        //     // PRINTF_WARN("** ");
        //     // IRQ fired, so re-read IIR **VERIFY** nothing to service & reset source
        //     iirVal.reg = sc16is741a_readReg(SC16IS741A_IIR_ADDR);
        // }


        // priority 1 -- receiver line status error : clear fifo of bad char
        if (iirVal.IRQ_SOURCE == 3)
        {
            PRINTF_ERR("RXErr ");
            sc16is741a_flushRxFifo();
        }


        // priority 2 -- receiver time-out (src=6), receiver RHR full (src=2) 
        // Service Action: read RXLVL, read FIFO to empty
        if (iirVal.IRQ_SOURCE == 2 || iirVal.IRQ_SOURCE == 6)
        {
            PRINTF_GRAY("RX=%d ", iirVal.IRQ_SOURCE);
            rxLevel = sc16is741a_readReg(SC16IS741A_RXLVL_ADDR);
            PRINTF_GRAY("-lvl=%d ", rxLevel);

            if (rxLevel > 0)
            {
                uint8_t volatile rxIndx = rxOpenCtrlBlock();
                PRINTF_GRAY("-ix=%d ", rxIndx);

                // read SPI into buffer
                sc16is741a_read(g_ltem1->iop->rxCtrlBlks[rxIndx].primBuf, rxLevel);
                g_ltem1->iop->rxCtrlBlks[rxIndx].primSz = rxLevel;

                // PRINTF_DBG("(%d) ", rxLevel);
                PRINTF_DBG("(%d)\r%s\r", rxLevel, g_ltem1->iop->rxCtrlBlks[rxIndx].primBuf);

                // parse and update IOP ctrl block
                rxParseRecvContentPreamble(rxIndx);
            }
        }


        // priority 3 -- transmit THR (threshold) : TX ready for more data
        if (iirVal.IRQ_SOURCE == 1)                                         // TX available
        {
            uint8_t txAvailable = sc16is741a_readReg(SC16IS741A_TXLVL_ADDR);
            PRINTF_GRAY("TX ");
            PRINTF_GRAY("-lvl=%d ", txAvailable);

            if (g_ltem1->iop->txCtrl->remainSz > 0)
            {
                txSendNextChunk(txAvailable);
                PRINTF_GRAY("-chunk ");
            }
            // else
            //     g_ltem1->iop->txCtrl->txActive = false;
        }

        /* -- NOT USED --
        // priority 4 -- modem interrupt
        // priority 6 -- receive XOFF/SpecChar
        // priority 7 -- nCTS, nRTS state change:
        */

        iirVal.reg = sc16is741a_readReg(SC16IS741A_IIR_ADDR);

    } while (iirVal.IRQ_nPENDING == 0);

    PRINTF_WHITE("]\r");

    gpioPinValue_t irqPin = gpio_readPin(g_ltem1->gpio->irqPin);
    if (irqPin == gpioValue_low)
    {
        txAvailable = sc16is741a_readReg(SC16IS741A_TXLVL_ADDR);
        rxLevel = sc16is741a_readReg(SC16IS741A_RXLVL_ADDR);
        iirVal.reg = sc16is741a_readReg(SC16IS741A_IIR_ADDR);
        PRINTF_WARN("IRQ failed to reset!!! nIRQ=%d, iir=%d, txLvl=%d, rxLvl=%d \r", iirVal.IRQ_nPENDING, iirVal.reg, txAvailable, rxLevel);

        //ltem1_faultHandler(500, "IRQ failed to reset.");
        goto retryIsr;
    }
}


#pragma endregion
