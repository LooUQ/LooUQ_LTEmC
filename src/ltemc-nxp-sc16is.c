/******************************************************************************
 *  \file ltemc-nxp-sc16is.c
 *  \author Greg Terrell, Jensen Miller
 *  \license MIT License
 *
 *  Copyright (c) 2020,2021 LooUQ Incorporated.
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
 * NXP SC16is__ (740,741,750,760) support used in LooUQ designs
 *****************************************************************************/

#define _DEBUG 2                        // set to non-zero value for PRINTF debugging output, 
// debugging output options             // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if _DEBUG > 0
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG == 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #endif
#else
#define PRINTF(c_, f_, ...) 
#endif

#include "ltemc-nxp-sc16is.h"
#include "ltemc-internal.h"
#include "lq-platform.h"


#define REG_MODIFY(REG_NAME, MODIFY_ACTION)                 \
REG_NAME REG_NAME##_reg = {0};                              \
REG_NAME##_reg.reg = SC16IS7xx_readReg(REG_NAME##_regAddr); \
MODIFY_ACTION                                               \
SC16IS7xx_writeReg(REG_NAME##_regAddr, REG_NAME##_reg.reg);


extern ltemDevice_t g_lqLTEM;


#pragma region Public Functions
#pragma endregion
/*-----------------------------------------------------------------------------------------------*/


#pragma region LTEm Internal Functions

/* Static Local Functions Declarations
------------------------------------------------------------------------------------------------ */
void S_displayFifoStatus(const char *dispMsg);


#pragma region Bridge Initialization

/**
*	@brief Configure base NXP bridge settings: reset (opt), FIFO, trigger levels (no trig IRQ yet), baud and framing
*/
void SC16IS7xx_start()
{
    // reset bridge to a known state, possible this is restart (already powered on)
    SC16IS7xx_writeReg(SC16IS7xx_UARTRST_regAddr, SC16IS7xx__SW_resetMask);

    // Need EFR[4]=1 to enable bridge enhanced functions: TX trigger and TLR settings for IRQ 
    SC16IS7xx_writeReg(SC16IS7xx_LCR_regAddr, SC16IS7xx__REGSET_enhanced);
    REG_MODIFY(SC16IS7xx_EFR, SC16IS7xx_EFR_reg.ENHANCED_FNS_EN = 1;)
    SC16IS7xx_writeReg(SC16IS7xx_LCR_regAddr, SC16IS7xx__REGSET_general);

    /* Using FCR for trigger\interrupt generation 
     * (NXP SC16IS741A manual section 8.13) When the trigger level setting in TLR is zero, the SC16IS741A uses the trigger level setting defined in FCR. 
     */
	SC16IS7xx_FCR fcrRegister = {0};
	fcrRegister.FIFO_EN = 1;
    fcrRegister.RX_TRIGGER_LVL = (int)RX_LVL_56CHARS;
    fcrRegister.TX_TRIGGER_LVL = (int)TX_LVL_56SPACES;
	SC16IS7xx_writeReg(SC16IS7xx_FCR_regAddr, fcrRegister.reg);

	// set baudrate, starts clock and UART
	SC16IS7xx_writeReg(SC16IS7xx_LCR_regAddr, SC16IS7xx__REGSET_special);
	SC16IS7xx_writeReg(SC16IS7xx_DLL_regAddr, SC16IS7xx__DLL_baudClockDivisorLOW);
	SC16IS7xx_writeReg(SC16IS7xx_DLH_regAddr, SC16IS7xx__DLH_baudClockDivisorHIGH);
	SC16IS7xx_writeReg(SC16IS7xx_LCR_regAddr, SC16IS7xx__REGSET_general);

	// set byte framing on the wire:  8 data, no parity, 1 stop required by BGx
	SC16IS7xx_writeReg(SC16IS7xx_LCR_regAddr, SC16IS7xx__LCR_UARTframing);
}


/**
 *	@brief Enable IRQ servicing for communications between SC16IS741 and BG96.
 */
void SC16IS7xx_enableIrqMode()
{
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
bool SC16IS7xx_chkCommReady()
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

	reg_payload.reg_payload = spi_transferWord(((spi_t*)g_lqLTEM.spi), reg_payload.reg_payload);
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

	spi_transferWord(((spi_t*)g_lqLTEM.spi), reg_payload.reg_payload);
}


/**
 *	@brief Reads through the SC16IS741A bridge (its RX FIFO)
 */
void SC16IS7xx_read(void* dest, uint8_t dest_len)
{
    union __SC16IS7xx_reg_addr_byte__ reg_addr = { 0 };
    reg_addr.A = SC16IS7xx_FIFO_regAddr;
    reg_addr.RnW = SC16IS7xx__FIFO_readRnW;

    spi_transferBuffer(((spi_t*)g_lqLTEM.spi), reg_addr.reg_address, dest, dest_len);
}


/**
 *	@brief Write through the SC16IS741A bridge
 */
void SC16IS7xx_write(const void* src, uint8_t src_len)
{
    union __SC16IS7xx_reg_addr_byte__ reg_addr = { 0 };
    reg_addr.A = SC16IS7xx_FIFO_regAddr;
    reg_addr.RnW = SC16IS7xx__FIFO_writeRnW;

    spi_transferBuffer(((spi_t*)g_lqLTEM.spi), reg_addr.reg_address, src, src_len);
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
    PRINTF(dbgColor__gray, "%s...\r\n", dispMsg);
    uint8_t bufFill = SC16IS7xx_readReg(SC16IS7xx_RXLVL_regAddr);
    PRINTF(dbgColor__gray, "  -- RX buf level=%d\r\n", bufFill);
    bufFill = SC16IS7xx_readReg(SC16IS7xx_TXLVL_regAddr);
    PRINTF(dbgColor__gray, "  -- TX buf level=%d\r\n", bufFill);
}
