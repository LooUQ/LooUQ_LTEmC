//	Copyright (c) 2020 LooUQ Incorporated.
//	Licensed under The MIT License. See LICENSE in the root directory.

#include "ltem1c.h"
//#include "nxp_sc16is741a.h"
#include <string.h>


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

	SC16IS741A_FCR fcrRegister = {0};
	fcrRegister.FIFO_EN = 1;
    fcrRegister.RX_TRIGGER_LVL = (int)RX_LVL_56CHARS;
    fcrRegister.TX_TRIGGER_LVL = (int)TX_LVL_56SPACES;
	sc16is741a_writeReg(SC16IS741A_FCR_ADDR, fcrRegister.reg);

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
 *
 *	\param[in] bridge The SC16IS741A bridge;
 *	\param[in] enable IRQ/ISR control enable/disable.
 */
void sc16is741a_enableIrqMode()
{
	// // // MCR[2] set(1) = TLR enable
	// REG_MODIFY(SC16IS741A_MCR, SC16IS741A_MCR_reg.TCR_TLR_EN = 1;) 

	// // TLR  
    // // NOTE: TLR can only be written when EFR[4] == 1 and MCR[2] == 1
    // // EFR[4] set=1 in enableFifo() previously at start of bridge init
	// SC16IS741A_TLR tlrSetting = {0};
	// tlrSetting.RX_TRIGGER_LVL = 0x0C;                           // 48 chars (12x4)
	// tlrSetting.TX_TRIGGER_LVL = 0x0C;                           // 48 spaces (12x4)
	// sc16is741a_writeReg(SC16IS741A_TLR_ADDR, tlrSetting.reg);

   	// IRQ Enabled: RX chars available, TX spaces available, UART framing error
	SC16IS741A_IER ierSetting = {0};
	ierSetting.RHR_DATA_AVAIL_INT_EN = 1;
	ierSetting.THR_EMPTY_INT_EN = 1; 
    ierSetting.RECEIVE_LINE_STAT_INT_EN = 1;
	sc16is741a_writeReg(SC16IS741A_IER_ADDR, ierSetting.reg);
}


#pragma endregion

#pragma region bridgeReadWrite


/**
 *	\brief Read from a SC16IS741A bridge register.
 *
 *	\param[in] bridge The SC16IS741A bridge.
 *	\param[in] reg_addr The register address.
 *	\param[out] reg_data A pointer to the destination to read the register into.
 */
uint8_t sc16is741a_readReg(uint8_t reg_addr)
{
	union __sc16is741a_reg_payload__ reg_payload = { 0 };
	reg_payload.reg_addr.A = reg_addr;
	reg_payload.reg_addr.RnW = SC16IS741A_FIFO_RnW_READ;	

	reg_payload.reg_payload = spi_transferWord(g_ltem1->spi, reg_payload.reg_payload);
	return reg_payload.reg_data;
}



/**
 *	\brief Write to a SC16IS741A bridge register.
 *
 *	\param[in] bridge The SC16IS741A bridge.
 *	\param[in] reg_addr The register address.
 *	\param[in] reg_data A pointer to the data to write to the register.
 */
void sc16is741a_writeReg(uint8_t reg_addr, uint8_t reg_data)
{
	union __sc16is741a_reg_payload__ reg_payload = { 0 };
	reg_payload.reg_addr.A = reg_addr;
	reg_payload.reg_addr.RnW = SC16IS741A_FIFO_RnW_WRITE;
	reg_payload.reg_data = reg_data;

	spi_transferWord(g_ltem1->spi, reg_payload.reg_payload);
}



/**
 *	\brief Reads through the SC16IS741A bridge (its RX FIFO).
 *
 *	\param[in] The SC16IS741A bridge.
 *	\param[out] dest The destination buffer.
 *	\param[in] The length of the destination buffer.
 *	\param[in/out] read_in The amount of bytes read into the destination.
 *	\returns The success of the operation.
 */
void sc16is741a_read(void* dest, size_t dest_len)
{
    union __sc16is741a_reg_addr_byte__ reg_addr = { 0 };
    reg_addr.A = SC16IS741A_FIFO_ADDR;
    reg_addr.RnW = SC16IS741A_FIFO_RnW_READ;

    spi_transferBuffer(g_ltem1->spi, reg_addr.reg_address, dest, dest_len);
}



/**
 *	\brief Write through the SC16IS741A bridge.
 *
 *	\param[in] bridge The SC16IS741A bridge.
 *	\param[in] src The source data to write.
 *	\param[in] src_len The length of the source.
 *	\returns The success of the operation.
 */
void sc16is741a_write(const void* src, size_t src_len)
{
    union __sc16is741a_reg_addr_byte__ reg_addr = { 0 };
    reg_addr.A = SC16IS741A_FIFO_ADDR;
    reg_addr.RnW = SC16IS741A_FIFO_RnW_WRITE;

    spi_transferBuffer(g_ltem1->spi, reg_addr.reg_address, src, src_len);
}



/**
 *	\brief Perform reset on bridge FIFO.
 *
 *  \param[in] resetAction What to reset TX, RX or both.
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
    // clear line status error, if set
    uint8_t lsrValue = sc16is741a_readReg(SC16IS741A_LSR_ADDR);

    for (size_t i = 0; i < rxFifoLvl; i++)
    {
        uint8_t rxDiscard = sc16is741a_readReg(SC16IS741A_FIFO_ADDR);
    }
}



void displayFifoStatus(const char *dispMsg)
{
    PRINTF_INFO("%s...\r\n", dispMsg);
    uint8_t bufFill = sc16is741a_readReg(SC16IS741A_RXLVL_ADDR);
    PRINTF_INFO("  -- RX buf level=%d\r\n", bufFill);
    bufFill = sc16is741a_readReg(SC16IS741A_TXLVL_ADDR);
    PRINTF_INFO("  -- TX buf level=%d\r\n", bufFill);
}


#pragma endregion
