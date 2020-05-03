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

// private function declarations

static void txSendNextChunk(uint8_t fifoAvailable);

static uint8_t rxOpenCtrlBlock();
static void rxParseRecvContentPreamble(uint8_t rxIndx);
static void rxIrdOpenExtendedBuffer(iop_rxCtrlBlock_t *rxCtrlStruct);
static void rxCloseCtrlBlock(uint8_t bufIndx);

static void interruptCallbackISR();

static uint8_t _rxGetNextProtoPayload(iop_process_t proto);


/* public functions
------------------------------------------------------------------------------------------------ */
#pragma region public functions


/**
 *	\brief Initialize the Input/Output Process subsystem.
 */
iop_state_t *iop_create()
{
    iop_state_t *iop = calloc(1, sizeof(iop_state_t));
	if (iop == NULL)
	{
        ltem1_faultHandler("iop-could not alloc iop status struct");
	}
    iop->txCtrl.txBuf = malloc(IOP_TX_BUFFER_SZ + 1);
    if (iop->txCtrl.txBuf == NULL)
    {
        free(iop);
        ltem1_faultHandler("iop-could not alloc iop TX buffers");
    }

    // init TX struct values
    *(iop->txCtrl.txBuf + IOP_TX_BUFFER_SZ) = "\0";
    iop->txCtrl.remainSz = 0;

    // init RX struct values to empty
    iop->rxCmdHeadIndx = IOP_EMPTY;
    for (size_t i = 0; i < IOP_PROTOCOLS_COUNT; i++)
    {
        iop->rxProtoHeadIndx[i] = IOP_EMPTY;
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
 *	\brief Determine if a send data can be initiated.
 *
 *  \return True if send is available.
 */
bool iop_txClearToSend()
{
    return g_ltem1->qbgReadyState == qbg_readyState_appReady && !g_ltem1->iop->txCtrl.sendActive;
}



/**
 *	\brief Start a (raw) send operation.
 *
 *  \param[in] sendData Pointer to char data to send out, input buffer can be discarded following call.
 *  \param[in] sendSz The number of characters to send.
 *
 *  \return True if send is completes.
 */
bool iop_txSend(const char *sendData, size_t sendSz)
{
    if (sendSz > IOP_TX_BUFFER_SZ)
        ltem1_faultHandler("iop-send data max size exceeded");

    g_ltem1->iop->txCtrl.sendActive = true;
    memcpy(g_ltem1->iop->txCtrl.txBuf, sendData, sendSz);

    uint16_t thisChunkSz = MIN(sendSz, SC16IS741A_FIFO_BUFFER_SZ);
    sc16is741a_write(g_ltem1->iop->txCtrl.txBuf, thisChunkSz);

    if (sendSz <= SC16IS741A_FIFO_BUFFER_SZ)
    {
        g_ltem1->iop->txCtrl.remainSz = 0;
        return true;
    }
    else
    {
        g_ltem1->iop->txCtrl.remainSz == sendSz - thisChunkSz;
        g_ltem1->iop->txCtrl.chunkPtr = g_ltem1->iop->txCtrl.txBuf + SC16IS741A_FIFO_BUFFER_SZ;
    }
    return false;
}



/**
 *	\brief Dequeue received data.
 */
iop_rx_result_t iop_rxGetCmdQueued(char *recvData, size_t responseSz)
{
    if (g_ltem1->iop->rxCmdHeadIndx == IOP_EMPTY)
        return iop_rx_result_nodata;

    int8_t currIndx = g_ltem1->iop->rxCmdHeadIndx;
    int8_t nextIndx;
    do
    {
        size_t copySz = MIN(responseSz, g_ltem1->iop->rxCtrlBlks[currIndx].primSz);
        strncpy(recvData, g_ltem1->iop->rxCtrlBlks[currIndx].primBuf, copySz + 1);
        rxCloseCtrlBlock(currIndx);

        responseSz -= copySz;
        currIndx = g_ltem1->iop->rxCtrlBlks[currIndx].next;        // step

        if (responseSz < 0)
            return iop_rx_result_truncated;

    } while(currIndx != IOP_EMPTY);

    g_ltem1->iop->rxCmdHeadIndx = IOP_EMPTY;
    return iop_rx_result_ready;
}



iop_rx_result_t iop_rxPushProtoQueued(iop_process_t socket)
{
    if (g_ltem1->iop->rxProtoHeadIndx[socket] == IOP_EMPTY)
        return iop_rx_result_nodata;

    int8_t currIndx = g_ltem1->iop->rxProtoHeadIndx[socket];
    char *recvData;
    uint16_t *recvDataSz;

    // if extended buffer in use, copy primary to start of extended buffer
    // determine which buffer (prim, aux) to return pointer to
    if (g_ltem1->iop->rxCtrlBlks[currIndx].extsnBufHead)
    {
        recvData = g_ltem1->iop->rxCtrlBlks[currIndx].extsnBufHead;
        recvDataSz = g_ltem1->iop->rxCtrlBlks[currIndx].extsnBufTail;
        memcpy(recvData, g_ltem1->iop->rxCtrlBlks[currIndx].primBuf, g_ltem1->iop->rxCtrlBlks[currIndx].primSz);
    }
    else
    {
        recvData = g_ltem1->iop->rxCtrlBlks[currIndx].primBuf;
        recvDataSz = g_ltem1->iop->rxCtrlBlks[currIndx].primSz;
    }

    // invoke protocol receiver func provided on ip_open() to push data into client
    (*g_ltem1->protocols->sockets[socket].ipReceiver_func)(recvData, recvDataSz);

    // if extended, free extended buffer space
    free(g_ltem1->iop->rxCtrlBlks[currIndx].extsnBufHead);
    rxCloseCtrlBlock(currIndx);

    //--done--
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
    uint8_t thisTxSz = MIN(g_ltem1->iop->txCtrl.remainSz, fifoAvailable);
    sc16is741a_write(g_ltem1->iop->txCtrl.chunkPtr, thisTxSz);

    // update TX ctrl with remaining work to complete requested transmit
    g_ltem1->iop->txCtrl.remainSz -= thisTxSz;
    g_ltem1->iop->txCtrl.chunkPtr = g_ltem1->iop->txCtrl.chunkPtr + thisTxSz;
    g_ltem1->iop->txCtrl.sendActive = g_ltem1->iop->txCtrl.remainSz > 0;
}



/**
 *	\brief Scan IOP Buffers structure for next available read destination slot (from BG to host).
 *
 *  \return Index of read buffer location to receive the incoming chars.
 */
static uint8_t rxOpenCtrlBlock()
{
	/* command, process URC, process IRD are all independently sharing buffer ring 
    *  reuse rx ctrl/buf if alread vacated
    */

    int bufIndx = g_ltem1->iop->rxTailIndx++;

    while (g_ltem1->iop->rxCtrlBlks[bufIndx].occupied)
    {
        bufIndx = (bufIndx == IOP_RX_CTRLBLOCK_COUNT - 1) ? 0 : ++bufIndx;

        if (bufIndx == g_ltem1->iop->rxTailIndx)
            ltem1_faultHandler("iop buffer fault, none available");
    }
    g_ltem1->iop->rxCtrlBlks[bufIndx].occupied = true;

    return g_ltem1->iop->rxTailIndx = bufIndx;
}



/**
 *	\brief Clear and dispose of IOP control stuct resources.
 *
 *	\param[in] bufferIndex The index into the buffer control struct to close.
 */
static void rxCloseCtrlBlock(uint8_t bufIndx)
{
    iop_rxCtrlBlock_t *rxCtrl = &g_ltem1->iop->rxCtrlBlks[bufIndx];

    rxCtrl->occupied = false;
    rxCtrl->process = iop_process_notAssigned;
    rxCtrl->isIrd = false;
    rxCtrl->next = IOP_EMPTY;
    memset(rxCtrl->primBuf, 0, IOP_RX_PRIMARY_BUFFER_SIZE + 1);

    if (rxCtrl->extsnBufHead != NULL)
    {
        free(rxCtrl->extsnBufHead);
        rxCtrl->extsnBufHead = NULL;
        rxCtrl->extsnBufTail = NULL;
    }
}



/**
 *	\brief Scan IOP Buffers for next (clockwise) buffer with proto contents.
 *
 *  \return Index of next buffer (to be new Head) containing protocol content.
 */
static uint8_t _rxGetNextProtoPayload(iop_process_t proto)
{
    // uint8_t bufIndx = g_ltem1->iop->rxProtoHeadIndx[proto];
    // do
    // {
    //     bufIndx = (bufIndx == IOP_RX_CTRLBLOCK_COUNT - 1) ? 0 : ++bufIndx;

    //     if (bufIndx == g_ltem1->iop->rxProtoHeadIndx[proto])          // wrapped around notfound
    //         return IOP_EMPTY;

    // } while (g_ltem1->iop->rxCtrlBlks[bufIndx].process != proto);

    // return g_ltem1->iop->rxTailIndx = bufIndx;
}



/**
 *	\brief Scan (in ISR) recv'd 1st stream chunk and prepare expansion buffer for remainder of incoming chunks.
 *
 *  \param[in\out] rxCtrlStruct receive control/buffer structure.
 */
static void rxIrdOpenExtendedBuffer(iop_rxCtrlBlock_t *rxCtrlStruct)
{
    /* note the protocol session is in invoking AT+QIRD=#, not returned in +QIRD result
    *  Example: +QIRD: 4,  where 4 is the number of chars arriving
    *     if chars arriving <= to primbuf size: all good, read will fit in primary
    *     if chars arriving > primbuf size, need to setup expandbuf (malloc)
    */
    char *lenPtr = rxCtrlStruct->primBuf + RECV_HEADERSZ_IRD;
    char *endPtr = NULL;
    uint16_t irdLen = strtol(lenPtr, &endPtr, 10);

    if (irdLen > IOP_RX_PRIMARY_BUFFER_SIZE)
    {
        uint8_t primLen = strlen(rxCtrlStruct->primBuf);
        rxCtrlStruct->extsnBufHead = calloc(irdLen, sizeof(uint8_t));
        rxCtrlStruct->extsnBufTail = rxCtrlStruct->extsnBufHead + primLen;
    }
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

    iop_rxCtrlBlock_t *rxCtrlBlock = &g_ltem1->iop->rxCtrlBlks[rxIndx];

    rxCtrlBlock->process = iop_process_command;    //default

    if (strncmp("+QIURC: \"recv", rxCtrlBlock->primBuf + 2, RECV_HEADERSZ_URC_IPRECV) == 0)
    {
        char *connIdPtr = rxCtrlBlock->primBuf + RECV_HEADERSZ_URC_IPRECV;
        char *endPtr = NULL;
        uint8_t connectId = (uint8_t)strtol(connIdPtr, &endPtr, 10);
        rxCtrlBlock->process = connectId;
    }

    if (strncmp("+QSSLURC: \"recv", rxCtrlBlock->primBuf + 2, RECV_HEADERSZ_URC_SSLRECV) == 0)
    {
        char *connIdPtr = rxCtrlBlock->primBuf + RECV_HEADERSZ_URC_SSLRECV;
        char *endPtr = NULL;
        uint8_t connectId = (uint8_t)strtol(connIdPtr, &endPtr, 10);
        rxCtrlBlock->process = connectId;
    }

    else if (strncmp("+QIRD", rxCtrlBlock->primBuf + 2, RECV_HEADERSZ_IRD) == 0)
    {
        rxIrdOpenExtendedBuffer(rxCtrlBlock);
    }

    else if (strncmp("APP RDY\r\n", rxCtrlBlock->primBuf + 2, RECV_HEADERSZ_APPRDY) == 0)
    {
        PRINTF("-appRdy ");
        g_ltem1->qbgReadyState = qbg_readyState_appReady;
        rxCtrlBlock->occupied = false;                      // release ctrl/buffer, processed here
        rxCtrlBlock->process = iop_process_notAssigned;
    }

    // catch async unsolicited state changes here
    else if (strncmp("+QIURC: ", rxCtrlBlock->primBuf + 2, RECV_HEADERSZ_URCSTATE) == 0)
    {
        // state message buffer is only 1 message
        if (g_ltem1->iop->urcStateMsg != NULL)
            ltem1_faultHandler("IOP-URC state msg buffer overflow.");

        strncpy(g_ltem1->iop->urcStateMsg + RECV_HEADERSZ_URCSTATE, rxCtrlBlock->primBuf, IOP_URC_STATEMSG_SZ);
        rxCtrlBlock->occupied = false;                      // release ctrl/buffer, processed here
        rxCtrlBlock->process = iop_process_notAssigned;
    }
    

    // update IOP control struct
    if (rxCtrlBlock->process == iop_process_command && g_ltem1->iop->rxCmdHeadIndx == IOP_EMPTY)
    {
        g_ltem1->iop->rxCmdHeadIndx = rxIndx;
    }
    else
    {
        /* code */
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
    PRINTF_INFO("ISR[");

    do
    {
        if (iirVal.IRQ_nPENDING == 1)
        {
            PRINTF_WARN("** ");
            // IRQ fired, so re-read IIR **VERIFY** nothing to service & reset source
            iirVal.reg = sc16is741a_readReg(SC16IS741A_IIR_ADDR);
        }


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
            uint8_t rxIndx;
            PRINTF_INFO("RX=%d ", iirVal.IRQ_SOURCE);
            rxLevel = sc16is741a_readReg(SC16IS741A_RXLVL_ADDR);
            //PRINTF("-lvl=%d ", rxLevel);

            rxIndx = rxOpenCtrlBlock();
            PRINTF("-ix=%d ", rxIndx);

            // read SPI into buffer
            sc16is741a_read(g_ltem1->iop->rxCtrlBlks[rxIndx].primBuf, rxLevel);
            g_ltem1->iop->rxCtrlBlks[rxIndx].primSz = rxLevel;
            PRINTF_DBG("\r%s\r", g_ltem1->iop->rxCtrlBlks[rxIndx].primBuf);

            // parse and update IOP ctrl block
            rxParseRecvContentPreamble(rxIndx);
        }


        // priority 3 -- transmit THR (threshold) : TX ready for more data
        if (iirVal.IRQ_SOURCE == 1)                                         // TX available
        {
            uint8_t txAvailable = sc16is741a_readReg(SC16IS741A_TXLVL_ADDR);
            PRINTF_INFO("TX ");
            // PRINTF("-lvl=%d ", txAvailable);

            if (g_ltem1->iop->txCtrl.remainSz > 0)
            {
                txSendNextChunk(txAvailable);
                PRINTF("-sChnk ");
            }
            else
                g_ltem1->iop->txCtrl.sendActive = false;
        }

        /* -- NOT USED --
        // priority 4 -- modem interrupt
        // priority 6 -- receive XOFF/SpecChar
        // priority 7 -- nCTS, nRTS state change:
        */

        iirVal.reg = sc16is741a_readReg(SC16IS741A_IIR_ADDR);

    } while (iirVal.IRQ_nPENDING == 0);

    PRINTF_INFO("]\r");

    gpio_pinValue_t irqPin = gpio_readPin(g_ltem1->gpio->irqPin);
    if (irqPin == gpioValue_low)
    {
        txAvailable = sc16is741a_readReg(SC16IS741A_TXLVL_ADDR);
        rxLevel = sc16is741a_readReg(SC16IS741A_RXLVL_ADDR);
        iirVal.reg = sc16is741a_readReg(SC16IS741A_IIR_ADDR);
        // PRINTF_WARN("IRQ reset stalled. nIRQ=%d, src=%d, txLvl=%d, rxLvl=%d \r", iirVal.IRQ_nPENDING, iirVal.IRQ_SOURCE, txAvailable, rxLevel);

        if (iirVal.IRQ_nPENDING == 0)
        {
            PRINTF_WARN("+");
            goto retryIsr;
        }
        ltem1_faultHandler("IRQ failed to reset.");
    }
}


#pragma endregion

