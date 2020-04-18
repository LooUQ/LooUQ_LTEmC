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
#define SC16IS741A_EFR_ENHANCED_FUNCTIONS 0x10U

// [7:4] RX, [3:0] TX - level / 4 (buffer granularity is 4)
#define SC16IS741A_TLR_TRIGGER_LEVELS 0x22U							// **!** testing value **!**
//#define SC16IS741A_TLR_TRIGGER_LEVELS 0xFFU					    // 15 (*4) = 60 char buffer


#pragma region privateFunctions

/**
 *	\brief Initializes and enables use of the FIFO for UART communications.
 *
 *	\param[in] bridge The SC16IS741A bridge.
 *	\param[in] enable Whether to enable the FIFO.
 */
static void enableFifo(bool enableFifo)
{
	SC16IS741A_FCR regSetting;
	regSetting.FIFO_EN = enableFifo;

	if (enableFifo)
	{
        regSetting.RX_TRIGGER_LVL = RX_LVL_56CHARS;
        regSetting.TX_TRIGGER_LVL = TX_LVL_56SPACES;
	}
	sc16is741a_writeReg(SC16IS741A_FCR_ADDR, regSetting.reg);
}



// BG96 only supports flow control in transparent data mode

/**
 *	\brief Enable flow control between SC16IS741 and BG96.
 *
 *	\param[in] bridge The SC16IS741A bridge;
 *	\param[in] enable Flow control enable/disable.
 */
// static void sc16is741a_enableFlowControl(sc16is741a_device bridge, bool enable)
// {
// 	const uint8_t flowSetting = enable ? 0xC0 : 0x00;
// 	sc16is741a_writeReg(bridge, SC16IS741A_LCR_ADDR, SC16IS741A_REG_SET_ENHANCED);
// 	sc16is741a_writeReg(bridge, SC16IS741A_EFR_ADDR, flowSetting);
// 	sc16is741a_writeReg(bridge, SC16IS741A_LCR_ADDR, SC16IS741A_REG_SET_GENERAL);
// }



/**
 *	\brief Read and discard rx FIFO contents.
 *
 *	\param[in] bridge The SC16IS741A bridge.
 */
static void flushRxFifo()
{
	uint8_t flushValue;

	for (int i = 0; i < 64; i++)
 	{
		if (sc16is741a_readReg(SC16IS741A_LSR_ADDR) == 0x60U)
		{
			break;
		}
		flushValue = sc16is741a_readReg(SC16IS741A_FIFO_ADDR);
	}
}



/**
 *	\brief Start the SC16IS741A-BG96 UART.
 */
static void startUart()
{
	// set baudrate, starts clock and UART
	sc16is741a_writeReg(SC16IS741A_LCR_ADDR, SC16IS741A_REG_SET_SPECIAL);
	sc16is741a_writeReg(SC16IS741A_DLL_ADDR, BAUDCLOCK_DIVISOR_DLL);
	sc16is741a_writeReg(SC16IS741A_DLH_ADDR, BAUDCLOCK_DIVISOR_DLH);
	sc16is741a_writeReg(SC16IS741A_LCR_ADDR, SC16IS741A_REG_SET_GENERAL);

	// set byte framing on the wire:  8 data, no parity, 1 stop required by BG96
	sc16is741a_writeReg(SC16IS741A_LCR_ADDR, SC16IS741A_LCR_UART_FRAMING);

	// clear RX buffer, ready for operation
	flushRxFifo();
}


#pragma endregion

/* Public fuctions
------------------------------------------------------------------------------------------------------------------------- */

#pragma region bridgeSetup

/**
 *	\brief Initialize SC16IS741A bridge.
 *
 *	\param[in] chipSelLine The chip select line for SPI.
 *	\param[in] spiClockSpeed The SPI clock speed.
 *	\param[in] uartBaudrate The UART baud rate.
 */
// void sc16is741a_start(uint8_t chipSelLine, uint32_t spiDataRate, uint32_t uartBaudrate)
// {
// 	sc16is741a_device_t *bridge = calloc(1, sizeof(sc16is741a_device_t));
// 	if (bridge == NULL)
// 	{
// 		/* [error]: could not allocate memory for bridge. */
// 		return NULL;
// 	}

// 	spi_config_t spi_config = 
// 	{
// 		.dataRate = spiDataRate,
// 		.dataMode = spi_dataMode_0,
// 		.bitOrder = spi_bitOrder_msbFirst,
// 		.csPin = chipSelLine
// 	};

//     // defer IRQ setting
// 	bridge->spi = spi_init(spi_config);
// 	if (bridge->spi == NULL)
// 	{
// 		/* [error]: could not initialize spi */
// 		free(bridge);
// 		return NULL;
// 	}

// 	// configure NXP bridge settings
// 	sc16is741a_enableFifo(true);
//     sc16is741a_startUart();
// 	return bridge;
// }



// /**
//  *	\brief Tear-down SC16IS741A bridge.
//  *
//  *	\param[in] chipSelLine The chip select line for SPI.
//  *	\param[in] spiClockSpeed The SPI clock speed.
//  *	\param[in] uartBaudrate The UART baud rate.
//  */
// void sc16is741a_uninit()
// {
// 	if (g_ltem1->bridge == NULL)
// 	{
// 		/* [error]: bridge is null. */
// 		return;
// 	}
// 	platform_spi_uninit(g_ltem1->bridge->spi);
// 	free(g_ltem1->bridge);
// }


/**
*	\brief Configure NXP bridge HW.
*/
void sc16is741a_start()
{
	sc16is741a_enableFifo(true);
    sc16is741a_startUart();
}



/**
 *	\brief Enable IRQ servicing for communications between SC16IS741 and BG96.
 *
 *	\param[in] bridge The SC16IS741A bridge;
 *	\param[in] enable IRQ/ISR control enable/disable.
 */
void sc16is741a_enableIrqMode()
{
	// IRQ Enabled: RX chars available, TX spaces available
	SC16IS741A_IER ierSetting;
	ierSetting.RX_DATA_AVAIL_INT_EN = true;
	ierSetting.THR_EMPTY_INT_EN = true; 
	sc16is741a_writeReg(SC16IS741A_IER_ADDR, ierSetting.reg);

	// EFR[4] set(1) = (bridge) enable enhanced functions
	sc16is741a_writeReg(SC16IS741A_LCR_ADDR, SC16IS741A_REG_SET_ENHANCED);
	REG_MODIFY(SC16IS741A_EFR, SC16IS741A_EFR_reg.ENHANCED_FNS_EN = true;)
	sc16is741a_writeReg(SC16IS741A_LCR_ADDR, SC16IS741A_REG_SET_GENERAL);

	// MCR[2] set(1) = TLR enable
	REG_MODIFY(SC16IS741A_MCR, SC16IS741A_MCR_reg.TCR_TLR_EN = true;) 

	// TLR
	SC16IS741A_TLR tlrSetting;
	tlrSetting.RX_TRIGGER_LVL = 0x0F;
	tlrSetting.TX_TRIGGER_LVL = 0x0F;
	sc16is741a_writeReg(SC16IS741A_TLR_ADDR, tlrSetting.reg);
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
    reg_addr.RnW = SC16IS741A_FIFO_RnW_READ;

    spi_transferBuffer(g_ltem1->spi, reg_addr.reg_address, src, src_len);
}

#pragma endregion
