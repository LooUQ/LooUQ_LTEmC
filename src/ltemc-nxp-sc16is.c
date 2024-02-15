/** ***************************************************************************
  @file ltemc-nxp-sc16is.c
  @brief UART control and transfer functions/services.

  @author Greg Terrell/Jensen Miller, LooUQ Incorporated

  \loouq

  @warning Internal dependencies, changes only as directed by LooUQ staff.

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
// https://www.nxp.com/docs/en/data-sheet/SC16IS740_750_760.pdf


#include <lq-embed.h>
#define LOG_LEVEL lqLOGLEVEL_DBG
//#define DISABLE_ASSERTS                                   // ASSERT/ASSERT_W enabled by default, can be disabled 
#define LQ_SRCFILE "NXP"                                    // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT

#include "ltemc-internal.h"
#include "ltemc-platform.h"
#include "ltemc-nxp-sc16is.h"

extern ltemDevice_t g_lqLTEM;

#define REG_MODIFY(REG_NAME, MODIFY_ACTION)                     \
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
    REG_MODIFY(SC16IS7xx_EFR, SC16IS7xx_EFR_reg.ENHANCED_FNS_EN = 1;) // enable enhanced functions (TX trigger for now)
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

    // flush RX buffer, RX must be previously empty to fire on new recv
    SC16IS7xx_writeReg(SC16IS7xx_FCR_regAddr, SC16IS7xx__FCR_IOP_RX_FLUSH);
}

/**
 *	@brief Write/read UART scratchpad register
 */
bool SC16IS7xx_ping()
{
    uint8_t wrVal = (uint8_t)(pMillis() & 0xFF);
    SC16IS7xx_writeReg(SC16IS7xx_SPR_regAddr, wrVal);
    return SC16IS7xx_readReg(SC16IS7xx_SPR_regAddr) == wrVal;
}


/**
 *	@brief Ping UART for a limited period of time until SPI sync'd between host and UART.
 */
bool SC16IS7xx_awaitReady()
{
    for (size_t i = 0; i < AWAIT_READY_TRIES; i++)
    {
        if (SC16IS7xx_ping())
        {
            SC16IS7xx_resetUART();
            return true;
        }
        pDelay(10);
        lqLOG_VRBS("_awaitReady() try: %d\r\n", i + 2);
    }
    return false;
}


#pragma endregion
/* ----------------------------------------------------------------------------------------------------------------- */

#pragma region bridgeReadWrite

/**
 *	@brief Read from a SC16IS741A bridge register.
 */
uint8_t SC16IS7xx_readReg(uint8_t reg_addr)
{
    union __SC16IS7xx_reg_payload__ reg_payload = {0};
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
    union __SC16IS7xx_reg_payload__ reg_payload = {0};
    reg_payload.reg_addr.A = reg_addr;
    reg_payload.reg_addr.RnW = SC16IS7xx__FIFO_writeRnW;
    reg_payload.reg_data = reg_data;

    spi_transferWord(g_lqLTEM.platformSpi, reg_payload.reg_payload);
}

/**
 *	@brief Reads through the SC16IS741A bridge (its RX FIFO)
 */
void SC16IS7xx_read(uint8_t * rxData, uint32_t size)
{
    union __SC16IS7xx_reg_addr_byte__ reg_addr = {0};
    reg_addr.A = SC16IS7xx_FIFO_regAddr;
    reg_addr.RnW = SC16IS7xx__FIFO_readRnW;

    //void spi_transferBuffer(platformSpi_t* platformSpi, uint8_t addressByte, const uint8_t * txData,  uint8_t * rxData, uint32_t size);

    spi_transferBuffer(g_lqLTEM.platformSpi, reg_addr.reg_address, NULL, rxData, size);
}


/**
 *	@brief Write through the SC16IS741A bridge
 */
void SC16IS7xx_write(const uint8_t * txData, uint32_t size)
{
    union __SC16IS7xx_reg_addr_byte__ reg_addr = {0};
    reg_addr.A = SC16IS7xx_FIFO_regAddr;
    reg_addr.RnW = SC16IS7xx__FIFO_writeRnW;

    spi_transferBuffer(g_lqLTEM.platformSpi, reg_addr.reg_address, txData, NULL, size);
}

/**
 *	@brief Perform reset on bridge FIFO
 */
void SC16IS7xx_resetFifo(sc16IS7xx_FifoResetAction_t resetAction)
{
    // fcr is a RdOnly register, flush and FIFO enable are both in this register
    SC16IS7xx_writeReg(SC16IS7xx_FCR_regAddr, resetAction |= SC16IS7xx__FCR_IOP_FIFO_ENABLE);
}

/**
 *	@brief Send serial break signal
 */
void SC16IS7xx_sendBreak()
{
    uint8_t lcrReg = SC16IS7xx_readReg(SC16IS7xx_LCR_regAddr);

    SC16IS7xx_writeReg(SC16IS7xx_LCR_regAddr, lcrReg |= SC16IS7xx__LCR_break);
    pDelay(2);
    SC16IS7xx_writeReg(SC16IS7xx_LCR_regAddr, lcrReg &= ~SC16IS7xx__LCR_break);
}


/**
 *	@brief Send Ctrl-Z 
 */
void SC16IS7xx_sendCtrlZ()
{
    SC16IS7xx_write("\026", 1);
}

/*
TOTAL HACK to see if flush possible
 */

void SC16IS7xx_flushTx(char flushChar, uint16_t sendCnt)
{
    char buffer[2] = {0}; 
    buffer[0] = flushChar;

    for (uint16_t i = 0; i < sendCnt; i++)
    {
        SC16IS7xx_write(&buffer, 1);
        pDelay(5);
    }
}


// /**
//  *	@brief Flush contents of RX FIFO
//  */
// void SC16IS7xx_flushRxFifo()
// {
//     uint8_t rxFifoLvl = SC16IS7xx_readReg(SC16IS7xx_RXLVL_regAddr);
//     uint8_t lsrValue = SC16IS7xx_readReg(SC16IS7xx_LSR_regAddr);

//     for (size_t i = 0; i < SC16IS7xx__FIFO_bufferSz; i++)
//     {
//         uint8_t rxDiscard = SC16IS7xx_readReg(SC16IS7xx_FIFO_regAddr);
//     }
//     rxFifoLvl = SC16IS7xx_readReg(SC16IS7xx_RXLVL_regAddr);
//     lsrValue = SC16IS7xx_readReg(SC16IS7xx_LSR_regAddr);
// }

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
