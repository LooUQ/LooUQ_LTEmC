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

#include "iop.h"
//#include "components\nxp_sc16is741a.h"



#pragma region static local functions
/* static (local) functions
------------------------------------------------------------------------------------------------ */


/**
 *	\brief Scan IOP Buffer structure for next available read destination slot.
 */
static int8_t getReadNextBuffer()
{
	/* cmd, process URC, process IRD are all independently sharing buffer ring
	* loop once for next spot, fail if all occupied */
	int8_t next = g_readLast;
	do
	{
		next = next < _READ_BUFFERS_MAX ? ++next : 0;
		if (!g_readBuffers[next].occupied)
			return next;
	}
	while (next != g_readLast);
	return _IOP_ERROR;
}



/**
 *	\brief Inline (within ISR) to scan recv'd chunk and prepare expansion buffer for remainder of incoming chunks.
 */
void process_qird(uint8_t bufindx)
{
	// note the protocol session is in invoking AT+QIRD=#, not returned in +QIRD result
	// +QIRD: 4,  : where 4 is the required buffer
	// if <= to primbuf size, all read in
	// if > primbuf size, need expandbuf malloc'd
}

#pragma endregion


/* ------------------------------------------------------------------------------------------------
public functions
------------------------------------------------------------------------------------------------ */

/**
 *	\brief Initialize the Input/Output Process subsystem.
 */
void iop_init(ltem1_device ltem1)
{

    }
    //     .readLast = -1;
    // }
    // int8_t cmdLast = 0;
    // int8_t protoLast[IOP_PROTOCOLS_SIZE] = {0};

    // bool cmdPending = false;
    // bool recvPending = false;
    // bool qirdPending = false;
    // }
}



/**
 *	\brief Initialize the Input/Output Process subsystem.
 */
void iop_uninit()
{
	g_ltem1 = NULL;
}



/**
 *	\brief Close and dispose of IOP buffer descriptor table resources.
 *
 *	\param[in] bufferIndex The index into the buffer descriptor table to close.
 */
void iop_closeBuffer(uint8_t toclose)
{
	uint8_t current;
	uint8_t next;
	do
	{
		current = toclose;
		next = g_readBuffers[current].next;

		// advance to the last buffer in chain
		while (next < _READ_BUFFERS_END && g_readBuffers[next].occupied)
		{
			current = g_readBuffers[current].next;
			next = g_readBuffers[current].next;
		}

		g_readBuffers[current].occupied = false;
		g_readBuffers[current].next = _READ_BUFFERS_END;

		// clear character buffers
		memset(g_readBuffers[current].primaryBuf, 0, _READ_BUFFERS_PRIMARY_BUFFER_SIZE);
		if (g_readBuffers[current].expandBufHead != NULL)
			free(g_readBuffers[current].expandBufHead);

	} while (current != toclose);
}



/**
 *	\brief Scan the received data to determine the data type and process owner.
 */
void iop_classifyReadBuffers()
{
	for (size_t i = 0; i < _READ_BUFFERS_MAX; i++)
	{
		if (g_readBuffers[i].ownerProc == iop_process_notclassified)
		{
			/* classification code */
			// +QIRD not here, see process_qird()

			// +QIURC: "recv" - proto tcp/udp
			// +QPING: - cmd
			// +QIURC: "dnsgip" - cmd
			// http ???
			// +QMTSTAT: proto mqtt
			// +QMTRECV: proto mqtt

			
		}
	}
}



/**
 *	\brief ISR for NXP bridge interrupt events; primary (1st bridge chunk) read/write actions to LTEm1.
 */
void iop_irqCallback_bridge()
{
	SC16IS741A_IIR iirVal;
	iirVal.reg = sc16is741a_readReg(g_ltem1->bridge, SC16IS741A_IIR_ADDR);

	if (iirVal.IRQ_PRIORITY == IRQ_2_RCVR_RHR)
	{
		int8_t readInto = getReadNextBuffer();

		// read SPI into buffer
		spi_transferBuffer(g_ltem1->bridge->spi, SC16IS741A_FIFO_RnW_READ, g_readBuffers[readInto].primaryBuf, SC16IS741A_FIFO_XFER);

		setReadProcessInfo(readInto);		// determine data type & owner and set "last" for owner process
	}

	if (iirVal.IRQ_PRIORITY == IRQ_3_XMIT_THR)
	{
		/* code */
	}
}

