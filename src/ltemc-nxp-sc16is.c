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

#define _DEBUG 0                        // set to non-zero value for PRINTF debugging output, 
// debugging output options             // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if defined(_DEBUG) && _DEBUG > 0
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG == 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#endif


#include "ltemc.h"
// #include <string.h>

#define REG_MODIFY(REG_NAME, MODIFY_ACTION) \
REG_NAME REG_NAME##_reg = {0}; \
REG_NAME##_reg.reg = sc16is741a_readReg(REG_NAME##_ADDR); \
MODIFY_ACTION \
sc16is741a_writeReg(REG_NAME##_ADDR, REG_NAME##_reg.reg);


// BG96 default baudrate is 115200, LTEm1 raw clock is 7.378MHz (SC16IS741a pg13)
#define BAUDCLOCK_DIVISOR_DLL 0x04U
#define BAUDCLOCK_DIVISOR_DLH 0x00U

// Bridge<>BG96 UART framing - 8 data, no parity, 1 stop (bits)
#define SC16IS741A_LCR_UART_FRAMING 0x03U
//#define SC16IS741A_EFR_ENHANCED_FUNCTIONS 0x10U

// [7:4] RX, [3:0] TX - level / 4 (buffer granularity is 4)
#define SC16IS741A_TLR_TRIGGER_LEVELS 0x22U							// **!** testing value **!**
//#define SC16IS741A_TLR_TRIGGER_LEVELS 0xFFU					    // 15 (*4) = 60 char buffer

// fcr is a RdOnly register, flush and FIFO enable are both in this register 
#define FCR_REGISTER_VALUE_BASIC_MODE 0xB7U
#define FCR_REGISTER_VALUE_IOP_FIFO_ENABLE 0xB1U
#define FCR_REGISTER_VALUE_IOP_RX_FLUSH 0x02U
#define FCR_REGISTER_VALUE_IOP_TX_FLUSH 0x04U



/* Public fuctions
------------------------------------------------------------------------------------------------------------------------- */

#pragma region bridgeSetup


/**
*	\brief Configure NXP bridge HW in basic mode (no IRQ, no level triggering).
*/
void sc16is741a_start()
{
    // start with a known state on NXP bridge, soft-reset bridge
    sc16is741a_writeReg(SC16IS741A_UARTRST_ADDR, SC16IS741A_SW_RESET_MASK);

	//enableFifo(true);
    //Need EFR[4]=1 to enable bridge enhanced functions: TX trigger and TLR settings for IRQ
    sc16is741a_writeReg(SC16IS741A_LCR_ADDR, SC16IS741A_REG_SET_ENHANCED);
    REG_MODIFY(SC16IS741A_EFR, SC16IS741A_EFR_reg.ENHANCED_FNS_EN = 1;)
    sc16is741a_writeReg(SC16IS741A_LCR_ADDR, SC16IS741A_REG_SET_GENERAL);

    /* Using FCR for trigger\interrupt generation 
            (NXP SC16IS741A manual section 8.13)
            When the trigger level setting in TLR is zero, the SC16IS741A uses the trigger level
            setting defined in FCR. If TLR has non-zero trigger level value, the trigger level defined in
            FCR is discarded. This applies to both transmit FIFO and receive FIFO trigger level
            setting.
    */
	SC16IS741A_FCR fcrRegister = {0};
	fcrRegister.FIFO_EN = 1;
    fcrRegister.RX_TRIGGER_LVL = (int)RX_LVL_56CHARS;
    fcrRegister.TX_TRIGGER_LVL = (int)TX_LVL_56SPACES;
	sc16is741a_writeReg(SC16IS741A_FCR_ADDR, fcrRegister.reg);

 	// // // MCR[2] set(1) = TLR enable
	// REG_MODIFY(SC16IS741A_MCR, SC16IS741A_MCR_reg.TCR_TLR_EN = 1;) 

	// // TLR  
    // // NOTE: TLR can only be written when EFR[4] == 1 and MCR[2] == 1
    // // EFR[4] set=1 in enableFifo() previously at start of bridge init
	// SC16IS741A_TLR tlrSetting = {0};
	// tlrSetting.RX_TRIGGER_LVL = 0x0C;                           // 48 chars (12x4)
	// tlrSetting.TX_TRIGGER_LVL = 0x0C;                           // 48 spaces (12x4)
	// sc16is741a_writeReg(SC16IS741A_TLR_ADDR, tlrSetting.reg);

    //startUart();
	// set baudrate, starts clock and UART
	sc16is741a_writeReg(SC16IS741A_LCR_ADDR, SC16IS741A_REG_SET_SPECIAL);
	sc16is741a_writeReg(SC16IS741A_DLL_ADDR, BAUDCLOCK_DIVISOR_DLL);
	sc16is741a_writeReg(SC16IS741A_DLH_ADDR, BAUDCLOCK_DIVISOR_DLH);
	sc16is741a_writeReg(SC16IS741A_LCR_ADDR, SC16IS741A_REG_SET_GENERAL);

	// set byte framing on the wire:  8 data, no parity, 1 stop required by BG96
	sc16is741a_writeReg(SC16IS741A_LCR_ADDR, SC16IS741A_LCR_UART_FRAMING);
}


/**
 *	\brief Enable IRQ servicing for communications between SC16IS741 and BG96.
 */
void sc16is741a_enableIrqMode()
{
   	// IRQ Enabled: RX chars available, TX spaces available, UART framing error
	SC16IS741A_IER ierSetting = {0};
	ierSetting.RHR_DATA_AVAIL_INT_EN = 1;
	ierSetting.THR_EMPTY_INT_EN = 1; 
    ierSetting.RECEIVE_LINE_STAT_INT_EN = 1;
	sc16is741a_writeReg(SC16IS741A_IER_ADDR, ierSetting.reg);
}


/**
 *	\brief Perform simple write\read using SC16IS741A scratchpad register. Used to test SPI communications.
 */
bool sc16is741a_chkCommReady()
{
    uint8_t wrVal = (uint8_t)(lMillis() & 0xFF);
    sc16is741a_writeReg(SC16IS741A_SPR_ADDR, wrVal);
    return sc16is741a_readReg(SC16IS741A_SPR_ADDR) == wrVal;
}

#pragma endregion



#pragma region bridgeReadWrite

/**
 *	\brief Read from a SC16IS741A bridge register.
 *
 *	\param reg_addr [in] - The register address.
 *  \return reg_data - Byte of data read from register
 */
uint8_t sc16is741a_readReg(uint8_t reg_addr)
{
	union __sc16is741a_reg_payload__ reg_payload = { 0 };
	reg_payload.reg_addr.A = reg_addr;
	reg_payload.reg_addr.RnW = SC16IS741A_FIFO_RnW_READ;	

	reg_payload.reg_payload = spi_transferWord(g_ltem->spi, reg_payload.reg_payload);
	return reg_payload.reg_data;
}



/**
 *	\brief Write to a SC16IS741A bridge register.
 *
 *	\param reg_addr [in] - The register address.
 *	\param reg_data [in] - Pointer to the data to write to the register.
 */
void sc16is741a_writeReg(uint8_t reg_addr, uint8_t reg_data)
{
	union __sc16is741a_reg_payload__ reg_payload = { 0 };
	reg_payload.reg_addr.A = reg_addr;
	reg_payload.reg_addr.RnW = SC16IS741A_FIFO_RnW_WRITE;
	reg_payload.reg_data = reg_data;

	spi_transferWord(g_ltem->spi, reg_payload.reg_payload);
}



/**
 *	\brief Reads through the SC16IS741A bridge (its RX FIFO).
 *
 *	\param dest [out] - The destination buffer.
 *	\param dest_len [in] - The length of the destination buffer.
 */
void sc16is741a_read(void* dest, uint8_t dest_len)
{
    union __sc16is741a_reg_addr_byte__ reg_addr = { 0 };
    reg_addr.A = SC16IS741A_FIFO_ADDR;
    reg_addr.RnW = SC16IS741A_FIFO_RnW_READ;

    spi_transferBuffer(g_ltem->spi, reg_addr.reg_address, dest, dest_len);
}



/**
 *	\brief Write through the SC16IS741A bridge.
 *
 *	\param src [in] - The source data to write.
 *	\param src_len [in] - The length of the source.
 */
void sc16is741a_write(const void* src, uint8_t src_len)
{
    union __sc16is741a_reg_addr_byte__ reg_addr = { 0 };
    reg_addr.A = SC16IS741A_FIFO_ADDR;
    reg_addr.RnW = SC16IS741A_FIFO_RnW_WRITE;

    spi_transferBuffer(g_ltem->spi, reg_addr.reg_address, src, src_len);
}



/**
 *	\brief Perform reset on bridge FIFO.
 *
 *  \param resetAction [in] - What to reset TX, RX or both.
 */
void sc16is741a_resetFifo(resetFifo_action_t resetAction)
{
    // fcr is a RdOnly register, flush and FIFO enable are both in this register 
    sc16is741a_writeReg(SC16IS741A_FCR_ADDR,  resetAction |= FCR_REGISTER_VALUE_IOP_FIFO_ENABLE);
}



/**
 *	\brief Flush contents of RX FIFO.
 */
void sc16is741a_flushRxFifo()
{
    uint8_t rxFifoLvl = sc16is741a_readReg(SC16IS741A_RXLVL_ADDR);
    uint8_t lsrValue = sc16is741a_readReg(SC16IS741A_LSR_ADDR);

    for (size_t i = 0; i < SC16IS741A_FIFO_BUFFER_SZ; i++)
    {
        uint8_t rxDiscard = sc16is741a_readReg(SC16IS741A_FIFO_ADDR);
    }
    rxFifoLvl = sc16is741a_readReg(SC16IS741A_RXLVL_ADDR);
    lsrValue = sc16is741a_readReg(SC16IS741A_LSR_ADDR);
}



/**
 *	\brief Debug: Show FIFO buffers fill level.
 */
void displayFifoStatus(const char *dispMsg)
{
    PRINTF(dbgColor_gray, "%s...\r\n", dispMsg);
    uint8_t bufFill = sc16is741a_readReg(SC16IS741A_RXLVL_ADDR);
    PRINTF(dbgColor_gray, "  -- RX buf level=%d\r\n", bufFill);
    bufFill = sc16is741a_readReg(SC16IS741A_TXLVL_ADDR);
    PRINTF(dbgColor_gray, "  -- TX buf level=%d\r\n", bufFill);
}


#pragma endregion
