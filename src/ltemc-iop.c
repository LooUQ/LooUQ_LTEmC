/** ***************************************************************************
  @file ltemc-iop.c
  @brief LTEmC low-level I/O processing.
  @warning Internal dependencies, changes should only be completed with the advise of LooUQ staff.

  @author Greg Terrell, LooUQ Incorporated

  \loouq
-------------------------------------------------------------------------------

LooUQ-LTEmC // Software driver for the LooUQ LTEm series cellular modems.
Copyright (C) 2017-2023 LooUQ Incorporated

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
Also add information on how to contact you by electronic and paper mail.

**************************************************************************** */


/**
 * @brief Known BGx Header Patterns Supported by LTEmC IOP 

    Area/Msg Prefix
     -- BG96 init
     /r/nAPP RDY/r/n      -- BG96 completed firmware initialization
     
     -- Commands
     +QPING:              -- PING response (instance and summary header)
     +QIURC: "dnsgip"     -- DNS lookup reply
    
     -- Protocols
     +QIURC: "recv",      -- "unsolicited response" proto tcp/udp
     +QIRD: #             -- "read data" response 
     +QSSLURC: "recv"     -- "unsolicited response" proto ssl tunnel
     +QHTTPGET:           -- GET response, HTTP-READ 
     CONNECT[cr][lf]      -- HTTP Read
     +QMTSTAT:            -- MQTT state change message recv'd
     +QMTRECV:            -- MQTT subscription data message recv'd
     
     -- Async Status Change Messaging
     +QIURC: "pdpdeact"   -- network pdp context timed out and deactivated

     default content type is command response
 */

#include <lq-embed.h>
#define lqLOG_LEVEL lqLOGLEVEL_DBG                                  ///< Logging detail level for this source file
//#define DISABLE_ASSERTS                                           ///< ASSERT/ASSERT_W enabled by default, can be disabled 
#define LQ_SRCFILE "IOP"                                            ///< create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT

#include "ltemc-internal.h"
#include "ltemc-iop.h"

extern ltemDevice_t g_lqLTEM;                                       ///< Global singleton LTEm object

#define MIN(x, y) (((x) < (y)) ? (x) : (y))                         ///< Returns the smaller of two numbers
#define MAX(x, y) (((x) > (y)) ? (x) : (y))                         ///< Returns the larger of two numbers

// Private Static Function Declarations
/* ------------------------------------------------------------------------------------------------ */
static inline uint8_t S_convertCharToContextId(const char cntxtChar);


/**
 *	@brief Initialize the Input/Output Process subsystem.
 */
void IOP_create()
{
    g_lqLTEM.iop = calloc(1, sizeof(iop_t));
    ASSERT(g_lqLTEM.iop != NULL);

    // TX buffering handled by source, IOP receives pointer and size

    bBuffer_t *rxBffrCtrl = calloc(1, sizeof(bBuffer_t));           // allocate space for RX buffer control struct
    if (rxBffrCtrl == NULL)
        return;
    char *rxBffr = calloc(1, ltem__bufferSz_rx);                    // allocate space for raw buffer
    if (rxBffr == NULL)
    {
        free(rxBffr);
        return;
    }
    bbffr_init(rxBffrCtrl, rxBffr, ltem__bufferSz_rx);              // initialize as a circular block buffer
    g_lqLTEM.iop->rxBffr = rxBffrCtrl;                              // add into IOP struct
}


/**
 *	@brief Complete initialization and start running IOP processes.
 */
void IOP_attachIrq()
{
    if (g_lqLTEM.iop->irqAttached == 0)
    {
        g_lqLTEM.iop->irqAttached = g_lqLTEM.pinConfig.irqPin;
        lqSpi_usingInterrupt(g_lqLTEM.platformSpi, g_lqLTEM.pinConfig.irqPin);
        lqGpio_attachIsr(g_lqLTEM.pinConfig.irqPin, true, gpioIrqTriggerOn_falling, IOP_interruptCallbackISR);
    }

    SC16IS7xx_resetFifo(SC16IS7xx_FIFO_resetActionRxTx);            // ensure FIFO state is empty, UART will not refire interrupt if pending
    g_lqLTEM.iop->txSrc = NULL;
    g_lqLTEM.iop->txPending = 0;
    g_lqLTEM.iop->isrEnabled = true;
}


/**
 *	@brief Perform a TX send operation buffered in TX buffer. This is a blocking until send is buffered.
 */
void IOP_startTx(const char *sendData, uint16_t sendSz)
{
    ASSERT(sendData != NULL && *sendData != '\0' && sendSz > 0);

    uint8_t txLevel = SC16IS7xx_readReg(SC16IS7xx_TXLVL_regAddr);       // check TX buffer status for flow, empty buffer is TX idle
    uint8_t immediateSz = 0;
    DPRINT(PRNT_RED, "\r\ntxLevel=%d >> ", SC16IS7xx_readReg(SC16IS7xx_TXLVL_regAddr));
    if (txLevel == SC16IS7xx__FIFO_bufferSz)
    {
        g_lqLTEM.iop->txSrc = sendData;
        g_lqLTEM.iop->txPending = sendSz;

        immediateSz = MIN(g_lqLTEM.iop->txPending, SC16IS7xx__FIFO_bufferSz);
        g_lqLTEM.iop->txSrc += immediateSz;
        g_lqLTEM.iop->txPending -= immediateSz;
        SC16IS7xx_write(sendData, immediateSz);
    }
    DPRINT(PRNT_RED, "txLevel=%d (sent=%d)\r\n", SC16IS7xx_readReg(SC16IS7xx_TXLVL_regAddr), immediateSz);
}


/**
 *	@brief Perform a forced TX send immediate operation. Intended for sending break type events to device.
 */
void IOP_forceTx(const char *sendData, uint16_t sendSz)
{
    ASSERT(sendSz <= SC16IS7xx__FIFO_bufferSz);
    SC16IS7xx_resetFifo(SC16IS7xx_FIFO_resetActionTx);
    pDelay(1);
    SC16IS7xx_write(sendData, SC16IS7xx__FIFO_bufferSz);
}


/**
 *	@brief Get the idle time in milliseconds since last RX I/O.
 */
uint32_t IOP_getRxIdleDuration()
{
    return lqMillis() - g_lqLTEM.iop->lastRxAt;
}


/**
 * @brief Get the current RX FIFO fill level from UART
 * 
 * @return uint8_t Number of characters in RX FIFO
 */
uint8_t IOP_getRxLevel()
{
    return SC16IS7xx_readReg(SC16IS7xx_RXLVL_regAddr);
}


/**
 * @brief Get the current TX FIFO available level from UART
 * 
 * @return uint8_t Number of spaces in TX FIFO
 */
uint8_t IOP_getTxLevel()
{
    return SC16IS7xx_readReg(SC16IS7xx_TXLVL_regAddr);
}


/**
 *	@brief Clear receive COMMAND/CORE response buffer.
 */
void IOP_resetRxBuffer()
{
    bbffr_reset(g_lqLTEM.iop->rxBffr);
}



// Static Function Definions
/*-----------------------------------------------------------------------------------------------*/

/**
 *	@brief Rapid fixed case conversion of context value returned from BGx to number.
 *
 *  @param [in] cntxChar RX data buffer to sync.
 */
static inline uint8_t S_convertCharToContextId(const char cntxtChar)
{
    return (uint8_t)cntxtChar - 0x30;
}


/**
 *	@brief ISR for NXP UART interrupt events, the NXP UART performs all serial I/O with BGx.
 */
void IOP_interruptCallbackISR()
{
    SC16IS7xx_IIR iirVal;
    uint8_t rxLevel;
    uint8_t txLevel;

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

    if (!g_lqLTEM.iop->isrEnabled)
    {
        return;
    }
    retryIsr:

    iirVal.reg = SC16IS7xx_readReg(SC16IS7xx_IIR_regAddr);
    do
    {
        g_lqLTEM.iop->isrInvokeCnt;
        uint8_t regReads = 0;
        while(iirVal.IRQ_nPENDING == 1 && regReads < 60)                               // wait for register, IRQ was signaled; safety limit at 60 in case of error gpio
        {
            iirVal.reg = SC16IS7xx_readReg(SC16IS7xx_IIR_regAddr);
            DPRINT(PRNT_dRED, "*");
            regReads++;
        }

        txLevel = SC16IS7xx_readReg(SC16IS7xx_TXLVL_regAddr);
        rxLevel = SC16IS7xx_readReg(SC16IS7xx_RXLVL_regAddr);
        DPRINT(PRNT_WHITE, "\rISR[%02X/t%d/r%d-iSrc=%d ", iirVal.reg, txLevel, rxLevel, iirVal.IRQ_SOURCE);

        // RX Error
        if (iirVal.IRQ_SOURCE == 3)                                                         // priority 1 -- receiver line status error : clear fifo of bad char
        {
            uint8_t lnStatus = SC16IS7xx_readReg(SC16IS7xx_LSR_regAddr);
            DPRINT(PRNT_ERROR, "rxERR(%02X)-lvl=%d ", lnStatus, rxLevel);
            DPRINT(PRNT_WARN, "bffrO=%d ", bbffr_getOccupied(g_lqLTEM.iop->rxBffr));
            SC16IS7xx_resetFifo(SC16IS7xx_FIFO_resetActionRxTx);                            // buffer is shot, clear to attempt recovery
        }

        // RX - read data from UART to rxBuffer
        if (iirVal.IRQ_SOURCE == 2 || iirVal.IRQ_SOURCE == 6)                               // priority 2 -- receiver RHR full (src=2), receiver time-out (src=6)
        {
            if (rxLevel > 0)
            {
                g_lqLTEM.iop->lastRxAt = lqMillis();
                char *bAddr;
                rxLevel = SC16IS7xx_readReg(SC16IS7xx_RXLVL_regAddr);

                uint16_t bWrCnt = bbffr_pushBlock(g_lqLTEM.iop->rxBffr, &bAddr, rxLevel);   // get contiguous block to write from UART
                DPRINT(PRNT_dYELLOW, "-rx(%p:%d) -Bo=%d ", bAddr, bWrCnt, bbffr_getOccupied(g_lqLTEM.iop->rxBffr));
                SC16IS7xx_read(bAddr, bWrCnt);
                bbffr_pushBlockFinalize(g_lqLTEM.iop->rxBffr, true);

                if (bWrCnt < rxLevel)                                                       // pushBlock only partially emptied UART
                {
                    rxLevel = SC16IS7xx_readReg(SC16IS7xx_RXLVL_regAddr);                   // get new rx number, push new receipts
                    bWrCnt = bbffr_pushBlock(g_lqLTEM.iop->rxBffr, &bAddr, rxLevel);
                    DPRINT(PRNT_dYELLOW, "-Wrx(%p:%d) -Bo=%d ", bAddr, bWrCnt, bbffr_getOccupied(g_lqLTEM.iop->rxBffr));
                    SC16IS7xx_read(bAddr, bWrCnt);
                    bbffr_pushBlockFinalize(g_lqLTEM.iop->rxBffr, true);
                }
                rxLevel = SC16IS7xx_readReg(SC16IS7xx_RXLVL_regAddr);
                ASSERT(rxLevel < SC16IS7xx__FIFO_bufferSz / 4);                             // bail if UART not emptying: overflow imminent
                iirVal.reg = SC16IS7xx_readReg(SC16IS7xx_IIR_regAddr);
                DPRINT(PRNT_WHITE, "--rxLvl=%d,iir=%02X ", rxLevel, iirVal.reg);
            }
        }

        // TX - write data to UART from txBuffer
        if (iirVal.IRQ_SOURCE == 1)                                                         // priority 3 -- transmit THR (threshold) : TX ready for more data
        {
            DPRINT(PRNT_dYELLOW, "-txP(%d) ", g_lqLTEM.iop->txPending);

            if (g_lqLTEM.iop->txPending > 0)
            {
                ASSERT(g_lqLTEM.iop->txPending < UINT16_MAX);
                ASSERT(g_lqLTEM.iop->txSrc != NULL);

                txLevel = SC16IS7xx_readReg(SC16IS7xx_TXLVL_regAddr);

                uint8_t blockSz = MIN(g_lqLTEM.iop->txPending, txLevel);                    // send what bridge buffer allows
                SC16IS7xx_write(g_lqLTEM.iop->txSrc, blockSz);
                g_lqLTEM.iop->txPending -= blockSz;
                g_lqLTEM.iop->txSrc += blockSz;
            }
        }

        /* -- NOT USED --
        // priority 4 -- modem interrupt
        // priority 6 -- receive XOFF/SpecChar
        // priority 7 -- nCTS, nRTS state change:
        */

        iirVal.reg = SC16IS7xx_readReg(SC16IS7xx_IIR_regAddr);

    } while (iirVal.IRQ_nPENDING == 0);

    DPRINT(PRNT_WHITE, "]\r");

    gpioPinValue_t irqPin = lqGpio_readPin(g_lqLTEM.pinConfig.irqPin);
    if (irqPin == gpioValue_low)
    {
        iirVal.reg = SC16IS7xx_readReg(SC16IS7xx_IIR_regAddr);
        txLevel = SC16IS7xx_readReg(SC16IS7xx_TXLVL_regAddr);
        rxLevel = SC16IS7xx_readReg(SC16IS7xx_RXLVL_regAddr);

        DPRINT(PRNT_YELLOW, "^IRQ: nIRQ=%d,iir=%d,txLvl=%d,rxLvl=%d^ ", iirVal.IRQ_nPENDING, iirVal.reg, txLevel, rxLevel);
        goto retryIsr;
    }
}
