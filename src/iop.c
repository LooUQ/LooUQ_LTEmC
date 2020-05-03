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


#define RECV_HEADERSZ_URC_RECV 15
#define RECV_HEADERSZ_IRD 7

#define MIN(x, y) (((x) < (y)) ? (x) : (y))


#pragma region static local functions
/* static (local) functions
------------------------------------------------------------------------------------------------ */


/**
 *	\brief Send out next chunk (NXP bridge buffer) of data. *** CALLED FROM ISR, TX BUFFER EMPTY ***
 */
static void txSendNextChunk(uint8_t fifoAvailable)
{
    uint8_t txSz = MIN(g_ltem1->iop->txCtrl.sendSz, fifoAvailable);
    sc16is741a_write(g_ltem1->iop->txCtrl.nextChunk, txSz);

    // update TX ctrl with remaining work to complete requested transmit
    g_ltem1->iop->txCtrl.sendSz -= txSz;
    g_ltem1->iop->txCtrl.nextChunk = g_ltem1->iop->txCtrl.nextChunk + txSz;
}



/**
 *	\brief Scan IOP Buffers structure for next available read destination slot (from BG to host).
 *
 *  \return Index of read buffer location to receive the incoming chars.
 */
static uint8_t rxGetNextAvailableCtrlBlock()
{
	/* command, process URC, process IRD are all independently sharing buffer ring */

    int bufIndx = g_ltem1->iop->rxTailIndx;

    while (g_ltem1->iop->rxCtrls[bufIndx].occupied)
    {
        bufIndx = (bufIndx == IOP_RX_CTRLBLOCK_COUNT - 1) ? 0 : ++bufIndx;

        if (bufIndx == g_ltem1->iop->rxTailIndx)
            ltem1_faultHandler("iop buffer fault, none available");
    }
    return g_ltem1->iop->rxTailIndx = bufIndx;
}



/**
 *	\brief Clear and dispose of IOP control stuct resources.
 *
 *	\param[in] bufferIndex The index into the buffer control struct to close.
 */
static void rxCloseCtrlBlock(uint8_t bufIndx)
{
    iop_rxCtrlBlock_t *rxCtrl = &g_ltem1->iop->rxCtrls[bufIndx];

    rxCtrl->occupied = false;
    rxCtrl->process = 0;
    rxCtrl->isIrd = false;
    rxCtrl->next = IOP_EMPTY;
    memset(rxCtrl->primBuf, 0, IOP_RX_PRIMARY_BUFFER_SIZE);

    if (rxCtrl->expBufHead != NULL)
    {
        free(rxCtrl->expBufHead);
        rxCtrl->expBufHead = NULL;
        rxCtrl->expBufTail = NULL;
    }
}



/**
 *	\brief Scan IOP Buffers for next (clockwise) buffer with proto contents.
 *
 *  \return Index of next buffer (to be new Head) containing protocol content.
 */
static uint8_t rxGetNextProtoPayload(iop_process_t proto)
{
    uint8_t bufIndx = g_ltem1->iop->rxProtoHeadIndx[proto];
    do
    {
        bufIndx = (bufIndx == IOP_RX_CTRLBLOCK_COUNT - 1) ? 0 : ++bufIndx;

        if (bufIndx == g_ltem1->iop->rxProtoHeadIndx[proto])          // wrapped around notfound
            return IOP_EMPTY;

    } while (g_ltem1->iop->rxCtrls[bufIndx].process != proto);

    return g_ltem1->iop->rxTailIndx = bufIndx;
}



/**
 *	\brief Scan (in ISR) recv'd 1st stream chunk and prepare expansion buffer for remainder of incoming chunks.
 *
 *  \param[in\out] rxCtrlStruct receive control/buffer structure.
 */
static void rxSetupIrdBuffer(iop_rxCtrlBlock_t *rxCtrlStruct)
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
        rxCtrlStruct->expBufHead = calloc(irdLen, sizeof(uint8_t));
        rxCtrlStruct->expBufTail = rxCtrlStruct->expBufHead + primLen;
    }
}



/**
 *	\brief Scan (in ISR) the received data to determine the buffer contents type and process owner.
 *
 *  \param[in\out] rxCtrlStruct receive control/buffer structure.
 */
static void rxParseReadContentPreamble(uint8_t rxIndx)
{
    /*
		// ** Known Header Patterns	** //
        //
        // -- BG96 init
        // \r\nAPP RDY\r\n  -- BG96 completed firmware initialization
        // 
        // -- Commands
        // +QPING:          -- PING response (instance and summary header)
        // +QIURC: "dnsgip" -- DNS lookup reply
        //
        // -- Protocols
        // +QIURC: "recv",  -- "unsolicited response" proto tcp/udp
        // +QIRD: #         -- "read data" response 
        // +QSSLURC: "recv" -- "unsolicited response" proto ssl tunnel
        // +QHTTPGET:       -- GET response, HTTP-READ 
        // CONNECT<cr><lf>  -- HTTP Read
        // +QMTSTAT:        -- MQTT state change message recv'd
        // +QMTRECV:        -- MQTT subscription data message recv'd

        // default content type is command response
    */

    iop_rxCtrlBlock_t *rxCtrlBlock = &g_ltem1->iop->rxCtrls[rxIndx];

    rxCtrlBlock->occupied = true;
    rxCtrlBlock->process = iop_process_command;    //default

    if (strncmp("+QIURC: \"r", rxCtrlBlock->primBuf, RECV_HEADERSZ_URC_RECV) == 0)
    {
        char *connIdPtr = rxCtrlBlock->primBuf + RECV_HEADERSZ_URC_RECV;
        char *endPtr = NULL;
        uint8_t connectId = (uint8_t)strtol(connIdPtr, &endPtr, 10);
        rxCtrlBlock->process = connectId;
    }

    else if (strncmp("+QIRD: ", rxCtrlBlock->primBuf, RECV_HEADERSZ_IRD) == 0)
    {
        rxSetupIrdBuffer(rxCtrlBlock);
    }

    else if (strncmp("\r\nAPP RDY\r\n", rxCtrlBlock->primBuf, RECV_HEADERSZ_IRD) == 0)
    {
        PRINTF("-appRdy ");
        g_ltem1->bg96ReadyState = bg96_readyState_appReady;
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

    uint8_t rxPass = 0;
    SC16IS741A_IIR iirVal;
    uint8_t rxFilled;
    uint8_t txAvailable;

    iirVal.reg = sc16is741a_readReg(SC16IS741A_IIR_ADDR);
    // rxFilled = sc16is741a_readReg(SC16IS741A_RXLVL_ADDR);
    // txAvailable = sc16is741a_readReg(SC16IS741A_TXLVL_ADDR);

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
            PRINTF_INFO("-RXErr ");
            sc16is741a_flushRxFifo();

            // // priority serviced, re-read IIR to see if lower priority needs serviced
            // iirVal.reg = sc16is741a_readReg(SC16IS741A_IIR_ADDR);
        }

        // priority 2 -- receiver time-out (src=6), receiver RHR full (src=2) 
        // Service Action: read RXLVL, read FIFO to empty
        if (iirVal.IRQ_SOURCE == 2 || iirVal.IRQ_SOURCE == 6)
        {
            uint8_t rxIndx;
            PRINTF_INFO("-RX=%d ", iirVal.IRQ_SOURCE);
            rxPass++;
            rxFilled = sc16is741a_readReg(SC16IS741A_RXLVL_ADDR);
            //PRINTF("-lvl=%d ", rxFilled);

            // 1st pass always get new IOP buffer and init rxIndx,
            // append if still room for continuation receive (totalBufSz - already used > rxPending)
            //                                       2nd+ pass rxIndx still at original buffer \/
            if (rxPass == 1 || (IOP_RX_PRIMARY_BUFFER_SIZE - g_ltem1->iop->rxCtrls[rxIndx].primSz > rxFilled))
            {
                rxIndx = rxGetNextAvailableCtrlBlock();
            }
            PRINTF("-indx=%d ", rxIndx);

            // read SPI into buffer
            sc16is741a_read(g_ltem1->iop->rxCtrls[rxIndx].primBuf, rxFilled);
            g_ltem1->iop->rxCtrls[rxIndx].primSz = rxFilled;

            // parse and update IOP ctrl block
            rxParseReadContentPreamble(rxIndx);

            // // priority serviced, read IIR to see if additional servicing required
            // iirVal.reg = sc16is741a_readReg(SC16IS741A_IIR_ADDR);
            // PRINTF("-rxExitSrc=%d ", iirVal.IRQ_SOURCE);
        }

        // priority 3 -- transmit THR (threshold) : TX ready for more data
        if (iirVal.IRQ_SOURCE == 1)                                         // TX available
        {
            uint8_t txAvailable = sc16is741a_readReg(SC16IS741A_TXLVL_ADDR);
            PRINTF_INFO("-TX ");
            PRINTF("-lvl=%d ", txAvailable);

            if (g_ltem1->iop->txCtrl.sendSz > 0)
                txSendNextChunk(txAvailable);
            else
                g_ltem1->iop->txCtrl.sendActive = false;

            // // priority serviced, re-read IIR to see if lower priority needs serviced
            // iirVal.reg = sc16is741a_readReg(SC16IS741A_IIR_ADDR);
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
        rxFilled = sc16is741a_readReg(SC16IS741A_RXLVL_ADDR);
        iirVal.reg = sc16is741a_readReg(SC16IS741A_IIR_ADDR);
        PRINTF_WARN("IRQ reset stalled. nIRQ=%d, src=%d, txLvl=%d, rxLvl=%d \r", iirVal.IRQ_nPENDING, iirVal.IRQ_SOURCE, txAvailable, rxFilled);

        if (iirVal.IRQ_nPENDING == 0)
        {
            PRINTF_WARN("+");
            goto retryIsr;
        }
        ltem1_faultHandler("IRQ failed to reset.");
    }
}


#pragma endregion


/* ------------------------------------------------------------------------------------------------
public functions
------------------------------------------------------------------------------------------------ */


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
    iop->txCtrl.sendSz = 0;

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
    return g_ltem1->bg96ReadyState == bg96_readyState_appReady && !g_ltem1->iop->txCtrl.sendActive;
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
    if (!iop_txClearToSend())
        ltem1_faultHandler("iop-clearToSend failed in send");
    if (sendSz > IOP_TX_BUFFER_SZ)
        ltem1_faultHandler("iop-send data max size exceeded");

    g_ltem1->iop->txCtrl.sendActive = true;

    // copy sender's data into iop tx buffer
    memcpy(g_ltem1->iop->txCtrl.txBuf, sendData, sendSz);
    sc16is741a_write(g_ltem1->iop->txCtrl.txBuf, MIN(sendSz, SC16IS741A_FIFO_BUFFER_SZ));

    if (sendSz <= SC16IS741A_FIFO_BUFFER_SZ)
    {
        g_ltem1->iop->txCtrl.sendSz = 0;
        return true;
    }
    else
    {
        g_ltem1->iop->txCtrl.sendSz -= SC16IS741A_FIFO_BUFFER_SZ;
        g_ltem1->iop->txCtrl.nextChunk = g_ltem1->iop->txCtrl.txBuf + SC16IS741A_FIFO_BUFFER_SZ;
    }
    return false;
}



/**
 *	\brief Dequeue received data.
 */
iop_rx_result_t iop_rxGetQueued(iop_process_t process, char *recvData, size_t responseSz)
{
    if (process == iop_process_command)                             // gather and return command response
    {
        if (g_ltem1->iop->rxCmdHeadIndx == IOP_EMPTY)
            return iop_rx_result_nodata;

        int8_t currIndx = g_ltem1->iop->rxCmdHeadIndx;
        int8_t nextIndx;
        do
        {
            size_t copySz = MIN(responseSz, g_ltem1->iop->rxCtrls[currIndx].primSz);
            strncpy(recvData, g_ltem1->iop->rxCtrls[currIndx].primBuf, copySz);
            rxCloseCtrlBlock(currIndx);

            responseSz -= copySz;
            currIndx = g_ltem1->iop->rxCtrls[currIndx].next;        // step

            if (responseSz < 0)
                return iop_rx_result_truncated;

        } while(currIndx != IOP_EMPTY);

        g_ltem1->iop->rxCmdHeadIndx = IOP_EMPTY;
        return iop_rx_result_ready;
    }

    else    // iop_process_protocol[0-5]
    {
        /* TBD protocol code */
    }
    return true;
}



// /**
//  *	\brief Close and dispose of IOP buffer descriptor table resources.
//  *
//  *	\param[in] bufferIndex The index into the buffer descriptor table to close.
//  */
// void iop_closeRxCtrl(uint8_t bufIndx)
// {
//     /* command buffers extend vertically thru forward-linked-list rdBuf chain, 
//      * protocol buffers extend horizontally thru expandbuf (malloc)
//     */
//     iop_rx_ctrlStruct_t *current;
//     int8_t currIndx;

//     if (g_ltem1->iop->rxCtrls[currIndx].process == iop_process_command)
//         g_ltem1->iop->cmdHeadIndx = IOP_EMPTY;
        
//     // start at top, find last, clear; work backwards to top
//     // when top == last: done
//     do      
//     {
//         currIndx = bufIndx;

//         // advance along linked chain to the last buffer
//         while (g_ltem1->iop->rxCtrls[currIndx].next != IOP_EMPTY)
//         {
//             currIndx = g_ltem1->iop->rxCtrls[currIndx].next;   // incr ptr
//         }
//         current = &g_ltem1->iop->rxCtrls[currIndx];


//     } while (currIndx != bufIndx);

// }

