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



#pragma region static local functions
/* static (local) functions
------------------------------------------------------------------------------------------------ */

#define INCREMENT_RD_BUFFER_PTR(BP) BP = (BP == (IOP_RD_BUFFERS_COUNT - 1)) ? 0 : ++BP

/**
 *	\brief Scan IOP Buffers structure for next available read destination slot (from BG to host).
 *
 *  \return Index of read buffer location to receive the incoming chars.
 */
static uint8_t getNextReadDest()
{
	/* command, process URC, process IRD are all independently sharing buffer ring */

    int bufIndx = g_ltem1->iop->readLast;
    while (g_ltem1->iop->rdBufs[bufIndx].occupied)
    {
        INCREMENT_RD_BUFFER_PTR(bufIndx);
        if (bufIndx == g_ltem1->iop->readLast)
            ltem1_faultHandler("iop buffer fault, none available");
    }
    return g_ltem1->iop->readLast = bufIndx;
}



/**
 *	\brief Scan IOP Buffers for next (clockwise) buffer with proto contents.
 *
 *  \return Index of next buffer (to be new Head) containing protocol content.
 */
static uint8_t getNextProtoPayload(iop_processes_t proto)
{
    int bufIndx = g_ltem1->iop->protoHead[proto];
    do
    {
        INCREMENT_RD_BUFFER_PTR(bufIndx);

        if (bufIndx == g_ltem1->iop->protoHead[proto])          // wrapped around notfound
            return IOP_RD_BUFFER_NOTFOUND;

    } while (g_ltem1->iop->rdBufs[bufIndx].process != proto);

    return g_ltem1->iop->readLast = bufIndx;
}



/**
 *	\brief Scan (in ISR) recv'd 1st stream chunk and prepare expansion buffer for remainder of incoming chunks.
 */
static void setupIrdBuffer(uint8_t bufIndx)
{
    /* note the protocol session is in invoking AT+QIRD=#, not returned in +QIRD result
    *  Example: +QIRD: 4,  where 4 is the number of chars arriving
    *     if chars arriving <= to primbuf size: all good, read will fit in primary
    *     if chars arriving > primbuf size, need to setup expandbuf (malloc)
    */
    char *lenPtr = g_ltem1->iop->rdBufs[bufIndx].primBuf + RECV_HEADERSZ_IRD;
    char *endPtr = NULL;
    uint16_t irdLen = strtol(lenPtr, &endPtr, 10);

    if (irdLen > IOP_RD_BUFFERS_PRIMARY_BUFFER_SIZE)
    {
        uint8_t primLen = strlen(g_ltem1->iop->rdBufs[bufIndx].primBuf);
        g_ltem1->iop->rdBufs[bufIndx].expBufHead = calloc(irdLen, sizeof(uint8_t));
        g_ltem1->iop->rdBufs[bufIndx].expBufTail = g_ltem1->iop->rdBufs[bufIndx].expBufHead + primLen;
    }
}



/**
 *	\brief Scan (in ISR) the received data to determine the buffer contents type and process owner.
 */
static void parseReadContentPreamble(uint8_t bufIndx)
{
    /*
		// ** Known Header Patterns	** //
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

    if (!g_ltem1->iop->rdBufs[bufIndx].occupied)
        ltem1_faultHandler("call to parse empty buffer");

    g_ltem1->iop->rdBufs[bufIndx].process = iop_process_command;    //default

    if (strncmp("+QIURC: \"r", g_ltem1->iop->rdBufs[bufIndx].primBuf, RECV_HEADERSZ_URC_RECV))
    {
        char *connIdPtr = g_ltem1->iop->rdBufs[bufIndx].primBuf + RECV_HEADERSZ_URC_RECV;
        char *endPtr = NULL;
        uint8_t connectId = (uint8_t)strtol(connIdPtr, &endPtr, 10);
        g_ltem1->iop->rdBufs[bufIndx].process = connectId;
    }

    else if (strncmp("+QIRD: ", g_ltem1->iop->rdBufs[bufIndx].primBuf, RECV_HEADERSZ_IRD))
    {
        setupIrdBuffer(bufIndx);
    }
}



/**
 *	\brief ISR for NXP bridge interrupt events; primary (1st bridge chunk) read/write actions to LTEm1.
 */
static void interruptCallbackISR()
{
    /*
    * IIR servicing:
    *   read (RHR) : buffer full (need to empty), timeout (chars recv'd, buffer not full but no more coming)
    *   write (THR): buffer emptied sufficiently to send more chars
    */

	SC16IS741A_IIR iirVal;
	iirVal.reg = sc16is741a_readReg(SC16IS741A_IIR_ADDR);

	if (iirVal.IRQ_PRIORITY == IRQ_2_RCVR_RHR)
	{
		uint8_t readIndx = getNextReadDest();
        uint8_t charCnt = sc16is741a_readReg(SC16IS741A_RXLVL_ADDR);

		// read SPI into buffer
        sc16is741a_read(g_ltem1->iop->rdBufs[readIndx].primBuf, charCnt);

        // likely need to retry if slow cmd response
        // need to wait until header parsing has sufficient chars, longest header is 131 uSec to serial in

        // set IOP ctrl block
        parseReadContentPreamble(readIndx);
	}

	if (iirVal.IRQ_PRIORITY == IRQ_3_XMIT_THR)
	{
		/* code */
	}
}


#pragma endregion


/* ------------------------------------------------------------------------------------------------
public functions
------------------------------------------------------------------------------------------------ */

/**
 *	\brief Initialize the Input/Output Process subsystem.
 */
iop_status_t *iop_create()
{
    iop_status_t *iop = malloc(sizeof(iop_status_t));
	if (iop == NULL)
	{
        ltem1_faultHandler("iop-could not alloc iop status struct");
	}
    return iop;
}



void iop_start()
{
    // attach ISR and enable NXP interrupt mode
    gpio_attachIsr(g_ltem1->gpio->irqPin, true, gpioIrqTriggerOn_falling, interruptCallbackISR);
    spi_protectFromInterrupt(g_ltem1->spi, g_ltem1->gpio->irqPin);
    sc16is741a_enableIrqMode();
}



/**
 *	\brief Initialize the Input/Output Process subsystem.
 */
void iop_destroy(void *iop)
{
	free(iop);
}



/**
 *	\brief Close and dispose of IOP buffer descriptor table resources.
 *
 *	\param[in] bufferIndex The index into the buffer descriptor table to close.
 */
void iop_closeRdBuffer(uint8_t bufIndx)
{
    /* command buffers extend vertically thru forward-linked-list rdBuf chain, 
     * protocol buffers extend horizontally thru expandbuf (malloc)
    */
    uint8_t curr;
    uint8_t next;

    // start at top, find last, clear; work backwards to top
    // when top == last: done
    do      
    {
        curr = bufIndx;
        next = g_ltem1->iop->rdBufs[curr].next;

        // advance along linked chain to the last buffer
        while (next != IOP_RD_BUFFER_EMPTY)
        {
            curr = g_ltem1->iop->rdBufs[curr].next;
            next = g_ltem1->iop->rdBufs[curr].next;
        }

        // clear buffer
        g_ltem1->iop->rdBufs[curr].occupied = false;
        g_ltem1->iop->rdBufs[curr].process = 0;
        g_ltem1->iop->rdBufs[curr].isIrd = false;
        g_ltem1->iop->rdBufs[curr].next = IOP_RD_BUFFER_EMPTY;
        memset(g_ltem1->iop->rdBufs[curr].primBuf, 0, IOP_RD_BUFFERS_PRIMARY_BUFFER_SIZE);

        if (g_ltem1->iop->rdBufs[curr].expBufHead != NULL)
        {
            free(g_ltem1->iop->rdBufs[curr].expBufHead);
            g_ltem1->iop->rdBufs[curr].expBufHead = NULL;
            g_ltem1->iop->rdBufs[curr].expBufTail = NULL;
        }
    } while (curr != bufIndx);
}

