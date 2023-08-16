/** ****************************************************************************
  \file 
  \brief LTEmC INTERNAL SPI/UART support
  \author Greg Terrell, Jensen Miller LooUQ Incorporated

  \loouq

  \warning This source unit is low-level processing code. Updates should only 
  be performed as directed by LooUQ.
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

#define SRCFILE "NXP"                       // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT
#define ENABLE_DIAGPRINT                    // expand DIAGPRINT into debug output
#define ENABLE_DIAGPRINT_VERBOSE            // expand DIAGPRINT and DIAGPRINT_V into debug output
#define ENABLE_ASSERT
//#include <jlinkRtt.h>                     // Use J-Link RTT channel for debug output (not platform serial)
#include <lqdiag.h>

#include "ltemc-internal.h"
#include "ltemc-platform.h"
#include "ltemc-nxp-sc16is.h"

extern ltemDevice_t g_lqLTEM;


#define REG_MODIFY(REG_NAME, MODIFY_ACTION)                 \
REG_NAME REG_NAME##_reg = {0};                              \
REG_NAME##_reg.reg = SC16IS7xx_readReg(REG_NAME##_regAddr); \
MODIFY_ACTION                                               \
SC16IS7xx_writeReg(REG_NAME##_regAddr, REG_NAME##_reg.reg);



#pragma region Public Functions
#pragma endregion
/*-----------------------------------------------------------------------------------------------*/


#pragma region LTEm Internal Functions

/* Static Local Functions Declarations
------------------------------------------------------------------------------------------------ */
void S_displayFifoStatus(const char *dispMsg);


#pragma region Bridge Initialization

/**
*	@brief Configure base NXP bridge settings: reset, FIFO (polled mode), baud and framing
*/
void SC16IS7xx_start()
{
    // reset bridge to a known state, possible this is restart (already powered on)
    SC16IS7xx_writeReg(SC16IS7xx_UARTRST_regAddr, SC16IS7xx__SW_resetMask);

    // Need EFR[4]=1 to enable bridge enhanced functions: TX trigger and TLR settings for IRQ 
    SC16IS7xx_writeReg(SC16IS7xx_LCR_regAddr, SC16IS7xx__LCR_REGSET_enhanced);
    REG_MODIFY(SC16IS7xx_EFR, SC16IS7xx_EFR_reg.ENHANCED_FNS_EN = 1;)                           // enable enhanced functions (TX trigger for now)
    SC16IS7xx_writeReg(SC16IS7xx_LCR_regAddr, SC16IS7xx__LCR_REGSET_general);

	SC16IS7xx_FCR fcrRegister = {0};
	fcrRegister.FIFO_EN = 1;
    fcrRegister.RX_TRIGGER_LVL = (int)RX_LVL_56CHARS;
    fcrRegister.TX_TRIGGER_LVL = (int)TX_LVL_56SPACES;
	SC16IS7xx_writeReg(SC16IS7xx_FCR_regAddr, fcrRegister.reg);

	// set baudrate => starts clock and UART
	SC16IS7xx_writeReg(SC16IS7xx_LCR_regAddr, SC16IS7xx__LCR_REGSET_special);
	SC16IS7xx_writeReg(SC16IS7xx_DLL_regAddr, SC16IS7xx__DLL_baudClockDivisorLOW);
	SC16IS7xx_writeReg(SC16IS7xx_DLH_regAddr, SC16IS7xx__DLH_baudClockDivisorHIGH);
	SC16IS7xx_writeReg(SC16IS7xx_LCR_regAddr, SC16IS7xx__LCR_REGSET_general);

	// set byte framing on the wire:  8 data, no parity, 1 stop required by BGx
	SC16IS7xx_writeReg(SC16IS7xx_LCR_regAddr, SC16IS7xx__LCR_UARTframing);
}


/**
 *	@brief Enable IRQ servicing for communications between SC16IS741 and BG9x.
 */
void SC16IS7xx_enableIrqMode()
{
    // // Need EFR[4]=1 and MCR[2]=1 TLR settings for IRQ 
    // SC16IS7xx_writeReg(SC16IS7xx_LCR_regAddr, SC16IS7xx__LCR_REGSET_enhanced);
	// SC16IS7xx_EFR efrRegister = {0};
	// efrRegister.ENHANCED_FNS_EN = 1;
    // SC16IS7xx_writeReg(SC16IS7xx_EFR_regAddr, efrRegister.reg);
    // // REG_MODIFY(SC16IS7xx_EFR, SC16IS7xx_EFR_reg.ENHANCED_FNS_EN = 1;)
    // SC16IS7xx_writeReg(SC16IS7xx_LCR_regAddr, SC16IS7xx__LCR_REGSET_general);
    // REG_MODIFY(SC16IS7xx_MCR, SC16IS7xx_MCR_reg.TCR_TLR_EN = 1;)


    // // /* reg field * 4 = trigger level, RX[7:4] / TX[3:0]
    // //  * 0x0=disabled, 0x1=4, 0x2=8, 0x3=12, 0x4=16, 0x5=20, 0x6=24, 0x7=28, 0x8=32, 0x9=36, 0xA=40, 0xB=44, 0xC=48, 0xD=52, 0xE=56, 0xF=60
    // // */

    // // // turn off flow-control
    // SC16IS7xx_writeReg(SC16IS7xx_TCR_regAddr, 0xF0);

    // // Set TLR (trigger levels)
    // SC16IS7xx_writeReg(SC16IS7xx_TLR_regAddr, 0xDF);                // RX=0xD (52 chars), TX=0xF (60 spaces)
    // // EFR[4]=1 (enhanced functions) and MCR[2]=1 (TCR/TLR enable) remain set

   	// IRQ to enable: RX chars available, TX spaces available, UART framing error : reg = 0x07
	SC16IS7xx_IER ierSetting = {0};
	ierSetting.RHR_DATA_AVAIL_INT_EN = 1;
	ierSetting.THR_EMPTY_INT_EN = 1; 
    ierSetting.RECEIVE_LINE_STAT_INT_EN = 1;
	SC16IS7xx_writeReg(SC16IS7xx_IER_regAddr, ierSetting.reg);
}


/**
 *	@brief Read interrupt enable register, check IER for IRQ enabled (register is cleared at reset)
 */
bool SC16IS7xx_isAvailable()
{
    uint8_t wrVal = (uint8_t)(pMillis() & 0xFF);
    SC16IS7xx_writeReg(SC16IS7xx_SPR_regAddr, wrVal);
    return SC16IS7xx_readReg(SC16IS7xx_SPR_regAddr) == wrVal;
    // return SC16IS7xx_readReg(SC16IS7xx_IER_regAddr) == 0x07;
}

#pragma endregion
/* ----------------------------------------------------------------------------------------------------------------- */


#pragma region bridgeReadWrite

/**
 *	@brief Read from a SC16IS741A bridge register.
 */
uint8_t SC16IS7xx_readReg(uint8_t reg_addr)
{
	union __SC16IS7xx_reg_payload__ reg_payload = { 0 };
	reg_payload.reg_addr.A = reg_addr;
	reg_payload.reg_addr.RnW = SC16IS7xx__FIFO_readRnW;

	reg_payload.reg_payload = spi_transferWord(g_lqLTEM.platformSpi, reg_payload.reg_payload);
	return reg_payload.reg_data;
}


/**
 *	@brief Write to a SC16IS741A bridge register
 */
void SC16IS7xx_writeReg(uint8_t reg_addr, uint8_t reg_data)
{
	union __SC16IS7xx_reg_payload__ reg_payload = { 0 };
	reg_payload.reg_addr.A = reg_addr;
	reg_payload.reg_addr.RnW = SC16IS7xx__FIFO_writeRnW;
	reg_payload.reg_data = reg_data;

	spi_transferWord(g_lqLTEM.platformSpi, reg_payload.reg_payload);
}


/**
 *	@brief Reads through the SC16IS741A bridge (its RX FIFO)
 */
void SC16IS7xx_read(void* dest, uint8_t dest_len)
{
    union __SC16IS7xx_reg_addr_byte__ reg_addr = { 0 };
    reg_addr.A = SC16IS7xx_FIFO_regAddr;
    reg_addr.RnW = SC16IS7xx__FIFO_readRnW;

    spi_transferBuffer(g_lqLTEM.platformSpi, reg_addr.reg_address, dest, dest_len);
}


/**
 *	@brief Write through the SC16IS741A bridge
 */
void SC16IS7xx_write(const void* src, uint8_t src_len)
{
    union __SC16IS7xx_reg_addr_byte__ reg_addr = { 0 };
    reg_addr.A = SC16IS7xx_FIFO_regAddr;
    reg_addr.RnW = SC16IS7xx__FIFO_writeRnW;

    spi_transferBuffer(g_lqLTEM.platformSpi, reg_addr.reg_address, src, src_len);
}


/**
 *	@brief Perform reset on bridge FIFO
 */
void SC16IS7xx_resetFifo(sc16IS7xx_FifoResetAction_t resetAction)
{
    // fcr is a RdOnly register, flush and FIFO enable are both in this register 
    SC16IS7xx_writeReg(SC16IS7xx_FCR_regAddr,  resetAction |= SC16IS7xx__FCR_IOP_FIFO_ENABLE);
}


/**
 *	@brief Perform reset on bridge FIFO
 */
void SC16IS7xx_sendBreak()
{
    uint8_t lcrReg = SC16IS7xx_readReg(SC16IS7xx_LCR_regAddr);

    SC16IS7xx_writeReg(SC16IS7xx_LCR_regAddr,  lcrReg |= SC16IS7xx__LCR_break);
    pDelay(2);
    SC16IS7xx_writeReg(SC16IS7xx_LCR_regAddr,  lcrReg &= ~SC16IS7xx__LCR_break);
}


/**
 *	@brief Flush contents of RX FIFO
 */
void SC16IS7xx_flushRxFifo()
{
    uint8_t rxFifoLvl = SC16IS7xx_readReg(SC16IS7xx_RXLVL_regAddr);
    uint8_t lsrValue = SC16IS7xx_readReg(SC16IS7xx_LSR_regAddr);

    for (size_t i = 0; i < SC16IS7xx__FIFO_bufferSz; i++)
    {
        uint8_t rxDiscard = SC16IS7xx_readReg(SC16IS7xx_FIFO_regAddr);
    }
    rxFifoLvl = SC16IS7xx_readReg(SC16IS7xx_RXLVL_regAddr);
    lsrValue = SC16IS7xx_readReg(SC16IS7xx_LSR_regAddr);
}


#pragma endregion
/* ----------------------------------------------------------------------------------------------------------------- */


/**
 *	@brief DEBUG: Show FIFO buffers fill level
 */
void SC16IS74__displayFifoStatus(const char *dispMsg)
{
    DIAGPRINT(PRNT_GRAY, "%s...\r\n", dispMsg);
    uint8_t bufFill = SC16IS7xx_readReg(SC16IS7xx_RXLVL_regAddr);
    DIAGPRINT(PRNT_GRAY, "  -- RX buf level=%d\r\n", bufFill);
    bufFill = SC16IS7xx_readReg(SC16IS7xx_TXLVL_regAddr);
    DIAGPRINT(PRNT_GRAY, "  -- TX buf level=%d\r\n", bufFill);
}
