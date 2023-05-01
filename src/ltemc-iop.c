/** ****************************************************************************
  \file 
  \brief LTEmC INTERNAL low-level I/O processing functionality
  \author Greg Terrell, LooUQ Incorporated

  \loouq

  \warning The IOP processor is the low-level I/O processing code, including
  interrupt servicing. Updates should only be performed as directed by LooUQ.

--------------------------------------------------------------------------------

    This project is released under the GPL-3.0 License.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 
***************************************************************************** */


/*** Known BGx Header Patterns Supported by LTEmC IOP ***
/**
 * @details

 Area/Msg Prefix
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
 * 
 */

#pragma region Header

#define _DEBUG 0                        // set to non-zero value for PRINTF debugging output, 
// debugging output options             // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if _DEBUG > 0
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG >= 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #define PRINTF(c_,f_,__VA_ARGS__...) do { rtt_printf(c_, (f_), ## __VA_ARGS__); } while(0)
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#endif

#define SRCFILE "IOP"                           // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT

#include "ltemc-internal.h"
#include "ltemc-iop.h"

extern ltemDevice_t g_lqLTEM;

#define QBG_APPREADY_MILLISMAX 15000

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define IOP_RXCTRLBLK_ADVINDEX(INDX) INDX = (++INDX == IOP_RXCTRLBLK_COUNT) ? 0 : INDX



#pragma region Private Static Function Declarations
/* ------------------------------------------------------------------------------------------------ */

static void S_interruptCallbackISR();
static inline uint8_t S_convertCharToContextId(const char cntxtChar);

#pragma endregion // Header


#pragma region Public Functions
/*-----------------------------------------------------------------------------------------------*/
#pragma endregion // Public Functions


#pragma region LTEm Internal Functions
/*-----------------------------------------------------------------------------------------------*/

/**
 *	@brief Initialize the Input/Output Process subsystem.
 */
void IOP_create()
{
    g_lqLTEM.iop = calloc(1, sizeof(iop_t));
    ASSERT(g_lqLTEM.iop != NULL);

    cBuffer_t *txBffrCtrl = calloc(1, sizeof(cBuffer_t));           // allocate space for TX buffer control struct
    if (txBffrCtrl == NULL)
        return;
    char *txBffr = calloc(1, ltem__bufferSz_tx);                    // allocate space for raw buffer
    if (txBffr == NULL)
    {
        free(txBffr);
        return;
    }

    cBuffer_t *rxBffrCtrl = calloc(1, sizeof(cBuffer_t));           // allocate space for RX buffer control struct
    if (rxBffrCtrl == NULL)
        return;
    char *rxBffr = calloc(1, ltem__bufferSz_rx);                    // allocate space for raw buffer
    if (rxBffr == NULL)
    {
        free(rxBffr);
        return;
    }

    // TX handled with CALLOC of struct
    cbffr_init(rxBffrCtrl, rxBffr, ltem__bufferSz_rx);              // initialize as a circ-buffer
    g_lqLTEM.iop->rxBffr = rxBffrCtrl;                              // add into IOP struct
}


/**
 *	@brief Complete initialization and start running IOP processes.
 */
void IOP_attachIrq()
{
    g_lqLTEM.iop->txBffr = NULL;
    g_lqLTEM.iop->txPending = 0;
    spi_usingInterrupt(g_lqLTEM.spi, g_lqLTEM.pinConfig.irqPin);
    platform_attachIsr(g_lqLTEM.pinConfig.irqPin, true, gpioIrqTriggerOn_falling, S_interruptCallbackISR);
}


/**
 *	@brief Stop IOP services.
 */
void IOP_detachIrq()
{
    platform_detachIsr(g_lqLTEM.pinConfig.irqPin);
}


/**
 *	@brief Verify LTEm firmware has started and is ready for driver operations.
 */
bool IOP_awaitAppReady()
{
    char buf[120] = {0};
    char *head = &buf;
    uint8_t rxLevel = 0;
    uint32_t waitStart = pMillis();

    while (pMillis() - waitStart < QBG_APPREADY_MILLISMAX)                  // typical wait: 700-1450 mS
    {
        rxLevel = SC16IS7xx_readReg(SC16IS7xx_RXLVL_regAddr);
        if (rxLevel > 0)
        {
            if (((head - buf) + rxLevel) < 120)
            {
                SC16IS7xx_read(head, rxLevel);
                for (size_t i = 0; i < rxLevel; i++)                        // innoculate any prefixing NULL
                {
                    head[i] = (head[i] == '\0') ? '~' : head[i];
                }
                head += rxLevel;
                if (strstr(buf, "APP RDY"))
                {
                    PRINTF(dbgColor__white, "AppRdy @ %lums\r", pMillis() - waitStart);
                    g_lqLTEM.deviceState = deviceState_appReady;
                    return true;
                }
            }
        }
        pYield();                                                           // give application time for non-comm startup activities or watchdog
    }
    PRINTF(dbgColor__warn, "AppRdy Fault: ");
    return false;
}


/**
 *	@brief Perform a TX send operation buffered in TX buffer. This is a blocking until send is buffered.
 */
void IOP_startTx(const char *sendData, uint16_t sendSz)
{
    ASSERT(sendData != NULL && *sendData != '\0' && sendSz > 0);

    uint8_t txLevel = SC16IS7xx_readReg(SC16IS7xx_TXLVL_regAddr);       // check TX buffer status for flow, empty buffer is TX idle
    if (txLevel == SC16IS7xx__FIFO_bufferSz)
    {
        g_lqLTEM.iop->txBffr = sendData;
        g_lqLTEM.iop->txPending = sendSz;

        uint8_t immediateSz = MIN(g_lqLTEM.iop->txPending, SC16IS7xx__FIFO_bufferSz);
        g_lqLTEM.iop->txBffr += immediateSz;
        g_lqLTEM.iop->txPending -= immediateSz;
        SC16IS7xx_write(sendData, immediateSz);
    }
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
    return pMillis() - g_lqLTEM.iop->lastRxAt;
}


/**
 *	@brief Clear receive COMMAND/CORE response buffer.
 */
void IOP_resetRxBuffer()
{
    cbffr_reset(g_lqLTEM.iop->rxBffr);
}


#pragma endregion


#pragma region Static Function Definions
/*-----------------------------------------------------------------------------------------------*/

/**
 *	@brief Rapid fixed case conversion of context value returned from BGx to number.
 *  @param cntxChar [in] RX data buffer to sync.
 */
static inline uint8_t S_convertCharToContextId(const char cntxtChar)
{
    return (uint8_t)cntxtChar - 0x30;
}



/**
 *	@brief ISR for NXP UART interrupt events, the NXP UART performs all serial I/O with BGx.
 */
static void S_interruptCallbackISR()
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

    SC16IS7xx_IIR iirVal;
    uint8_t rxLevel;
    uint8_t txLevel;

    retryIsr:

    iirVal.reg = SC16IS7xx_readReg(SC16IS7xx_IIR_regAddr);
    do
    {
        uint8_t regReads = 0;
        while(iirVal.IRQ_nPENDING == 1 && regReads < 60)                               // wait for register, IRQ was signaled; safety limit at 60 in case of error gpio
        {
            iirVal.reg = SC16IS7xx_readReg(SC16IS7xx_IIR_regAddr);
            PRINTF(dbgColor__dRed, "*");
            regReads++;
        }

        txLevel = SC16IS7xx_readReg(SC16IS7xx_TXLVL_regAddr);
        rxLevel = SC16IS7xx_readReg(SC16IS7xx_RXLVL_regAddr);
        PRINTF(dbgColor__white, "\rISR[%02X/t%d/r%d-iSrc=%d ", iirVal.reg, txLevel, rxLevel, iirVal.IRQ_SOURCE);

        // RX Error
        if (iirVal.IRQ_SOURCE == 3)                                                         // priority 1 -- receiver line status error : clear fifo of bad char
        {
            uint8_t lnStatus = SC16IS7xx_readReg(SC16IS7xx_LSR_regAddr);
            PRINTF(dbgColor__error, "rxERR(%02X)-lvl=%d ", lnStatus, rxLevel);
            PRINTF(dbgColor__warn, "bffrO=%d ", cbffr_getOccupied(g_lqLTEM.iop->rxBffr));

            #if _DEBUG > 2
                PRINTF(dbgColor__yellow, " >FIFO Dump\r");
                char fifoTop;
                for (size_t i = 0; i < rxLevel; i++)
                {
                    lnStatus = SC16IS7xx_readReg(SC16IS7xx_LSR_regAddr);
                    SC16IS7xx_read(&fifoTop, 1);
                    PRINTF(dbgColor__yellow, " >%02d-%02d 0x%02X\r", i, fifoTop, lnStatus);
                }
            #elif _DEBUG > 0
                char dbg[65] = {0};
                SC16IS7xx_read(dbg, 64);
                SC16IS7xx_flushRxFifo();
            #else
                ASSERT(false);                                                              // should have caught earlier, dead now.
            #endif
        }

        // RX - read data from UART to rxBuffer
        if (iirVal.IRQ_SOURCE == 2 || iirVal.IRQ_SOURCE == 6)                               // priority 2 -- receiver RHR full (src=2), receiver time-out (src=6)
        {
            if (rxLevel > 0)
            {
                g_lqLTEM.iop->lastRxAt = pMillis();
                char *bAddr;
                rxLevel = SC16IS7xx_readReg(SC16IS7xx_RXLVL_regAddr);

                uint16_t bWrCnt = cbffr_pushBlock(g_lqLTEM.iop->rxBffr, &bAddr, rxLevel);   // get contiguous block to write from UART
                PRINTF(dbgColor__dYellow, "-rx(%p:%d) -Bo=%d ", bAddr, bWrCnt, cbffr_getOccupied(g_lqLTEM.iop->rxBffr));
                SC16IS7xx_read(bAddr, bWrCnt);
                cbffr_pushBlockFinalize(g_lqLTEM.iop->rxBffr, true);

                if (bWrCnt < rxLevel)                                                       // pushBlock only partially emptied UART
                {
                    rxLevel = SC16IS7xx_readReg(SC16IS7xx_RXLVL_regAddr);                   // get new rx number, push new receipts
                    bWrCnt = cbffr_pushBlock(g_lqLTEM.iop->rxBffr, &bAddr, rxLevel);
                    PRINTF(dbgColor__dYellow, "-Wrx(%p:%d) -Bo=%d ", bAddr, bWrCnt, cbffr_getOccupied(g_lqLTEM.iop->rxBffr));
                    SC16IS7xx_read(bAddr, bWrCnt);
                    cbffr_pushBlockFinalize(g_lqLTEM.iop->rxBffr, true);
                }
                rxLevel = SC16IS7xx_readReg(SC16IS7xx_RXLVL_regAddr);
                ASSERT(rxLevel < SC16IS7xx__FIFO_bufferSz / 4);                             // bail if UART still not empty: overflow imminent

                iirVal.reg = SC16IS7xx_readReg(SC16IS7xx_IIR_regAddr);
                rxLevel = SC16IS7xx_readReg(SC16IS7xx_RXLVL_regAddr);
                PRINTF(dbgColor__white, "--rxLvl=%d,iir=%02X ", rxLevel, iirVal.reg);
            }
        }

        // TX - write data to UART from txBuffer
        if (iirVal.IRQ_SOURCE == 1)                                                         // priority 3 -- transmit THR (threshold) : TX ready for more data
        {
            PRINTF(dbgColor__dYellow, "-txP(%d) ", g_lqLTEM.iop->txPending);

            if (g_lqLTEM.iop->txPending > 0)
            {
                ASSERT(g_lqLTEM.iop->txPending < UINT16_MAX);
                ASSERT(g_lqLTEM.iop->txBffr != NULL);

                txLevel = SC16IS7xx_readReg(SC16IS7xx_TXLVL_regAddr);

                uint8_t blockSz = MIN(g_lqLTEM.iop->txPending, txLevel);                        // send what bridge buffer allows
                SC16IS7xx_write(g_lqLTEM.iop->txBffr, blockSz);
                g_lqLTEM.iop->txPending -= blockSz;
                g_lqLTEM.iop->txBffr += blockSz;
            }
        }

        /* -- NOT USED --
        // priority 4 -- modem interrupt
        // priority 6 -- receive XOFF/SpecChar
        // priority 7 -- nCTS, nRTS state change:
        */

        iirVal.reg = SC16IS7xx_readReg(SC16IS7xx_IIR_regAddr);

    } while (iirVal.IRQ_nPENDING == 0);

    PRINTF(dbgColor__white, "]\r");

    gpioPinValue_t irqPin = platform_readPin(g_lqLTEM.pinConfig.irqPin);
    if (irqPin == gpioValue_low)
    {
        iirVal.reg = SC16IS7xx_readReg(SC16IS7xx_IIR_regAddr);
        txLevel = SC16IS7xx_readReg(SC16IS7xx_TXLVL_regAddr);
        rxLevel = SC16IS7xx_readReg(SC16IS7xx_RXLVL_regAddr);

        PRINTF(dbgColor__yellow, "^IRQ: nIRQ=%d,iir=%d,txLvl=%d,rxLvl=%d^ ", iirVal.IRQ_nPENDING, iirVal.reg, txLevel, rxLevel);
        goto retryIsr;
    }
}


#pragma endregion

