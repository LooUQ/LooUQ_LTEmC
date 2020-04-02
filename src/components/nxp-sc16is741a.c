//	Copyright (c) 2020 LooUQ Incorporated.

//	Licensed under The MIT License. See LICENSE in the root directory.
#include "nxp-sc16is741a.h"
#include <string.h>


#define REG_MODIFY(pDEVICE, REG_NAME, MODIFY) \
REG_NAME reg = {0}; \
sc16is741a_regRead(pDEVICE, REG_NAME##_ADDR, &reg.reg); \
MODIFY \
sc16is741a_regWrite(pDEVICE, REG_NAME##_ADDR, &reg.reg);

// BG96 default baudrate is 115200, LTEm1 raw clock is 7.378MHz (SC16IS741a pg13)
#define BAUDCLOCK_DIVISOR_DLL 0x04
#define BAUDCLOCK_DIVISOR_DLH 0x00


/**
 *	\brief SC16IS741A First SPI byte for register addressing.
 *
 *	This byte tells the SPI slave what register to access and
 *		whether this operation is a read or write.
 */
union __sc16is741a_reg_addr_byte__
{
	struct
	{
		ro8 : 1;
		rw8 CH0 : 1;
		rw8 CH1 : 1;
		rw8 A : 4;
		rw8 RnW : 1;
	};
	uint8_t reg_address;
};


/**
 *	\brief SC16IS741A SPI bytes containing address and register value.
 */
union __sc16is741a_reg_payload__
{
	struct
	{
		uint8_t reg_data;
		union __sc16is741a_reg_addr_byte__ reg_addr;
	};
	uint16_t reg_payload;
};


/**
 *	\brief Setup SC16IS741A FIFO buffers.
 *
 *	\param[in] bridge The SC16IS741A bridge.
 */
static inline void
sc16is741_setupFifoBuffers(sc16is741a_device bridge)
{	
	memset(bridge->tx_buffer, '\0', SC16IS741A_FIFO_MAX+2);
	memset(bridge->rx_buffer, '\0', SC16IS741A_FIFO_MAX+2);

	union __sc16is741a_reg_addr_byte__* tx_addr_byte = (union __sc16is741a_reg_addr_byte__*)&bridge->tx_buffer[0];
	tx_addr_byte->A = SC16IS741A_FIFO_ADDR;
	tx_addr_byte->RnW = SC16IS741A_FIFO_RnW_WRITE;
	
	union __sc16is741a_reg_addr_byte__* rx_addr_byte = (union __sc16is741a_reg_addr_byte__*) &bridge->rx_buffer[0];
	rx_addr_byte->A = SC16IS741A_FIFO_ADDR;
	rx_addr_byte->RnW = SC16IS741A_FIFO_RnW_READ;
}



/**
 *	\brief Write to a SC16IS741A bridge register.
 *
 *	\param[in] bridge The SC16IS741A bridge.
 *	\param[in] reg_addr The register address.
 *	\param[in] reg_data A pointer to the data to write to the register.
 */
void
sc16is741a_regWrite(sc16is741a_device bridge, uint8_t reg_addr, const uint8_t * reg_data)
{
	union __sc16is741a_reg_payload__ reg_payload = { 0 };
	reg_payload.reg_addr.A = reg_addr;
	reg_payload.reg_addr.RnW = 0U;
	reg_payload.reg_data = *reg_data;

	spi_transferBuffer(bridge->spi, &reg_payload.reg_payload, &reg_payload.reg_payload, sizeof(union __sc16is741a_reg_payload__));
}



/**
 *	\brief Read from a SC16IS741A bridge register.
 *
 *	\param[in] bridge The SC16IS741A bridge.
 *	\param[in] reg_addr The register address.
 *	\param[out] reg_data A pointer to the destination to read the register into.
 */
void
sc16is741a_regRead(sc16is741a_device bridge, uint8_t reg_addr, uint8_t* reg_data)
{
	union __sc16is741a_reg_payload__ reg_payload = { 0 };
	reg_payload.reg_addr.A = reg_addr;
	reg_payload.reg_addr.RnW = 1U;	

	spi_transferBuffer(bridge->spi, &reg_payload.reg_payload, &reg_payload.reg_payload, sizeof(union __sc16is741a_reg_payload__));
	*reg_data = reg_payload.reg_data;
}



/**
 *	\brief Set the SC16IS741A-BG96 uart baudrate.
 *
 *	\param[in] bridge The SC16IS741A bridge.
 */
void
sc16is741a_setUartBaudrate(sc16is741a_device bridge)
{
	sc16is741a_regWrite(bridge, SC16IS741A_LCR_ADDR, SC16IS741A_REG_SET_ENHANCED);
	sc16is741a_regWrite(bridge, SC16IS741A_DLL_ADDR, BAUDCLOCK_DIVISOR_DLL);
	sc16is741a_regWrite(bridge, SC16IS741A_DLH_ADDR, BAUDCLOCK_DIVISOR_DLH);
	sc16is741a_regWrite(bridge, SC16IS741A_LCR_ADDR, SC16IS741A_REG_SET_GENERAL);
}



/**
 *	\brief Enable flow control between SC16IS741 and BG96.
 *
 *	\param[in] bridge The SC16IS741A bridge;
 *	\param[in] enable Flow control enable/disable.
 */
static void
sc16is741a_enableFlowControl(sc16is741a_device bridge, bool enable)
{
	const uint8_t *flowSetting = enable ? 0xC0 : 0x00;
	sc16is741a_regWrite(bridge, SC16IS741A_LCR_ADDR, SC16IS741A_REG_SET_ENHANCED);
	sc16is741a_regWrite(bridge, SC16IS741A_EFR_ADDR, flowSetting);
	sc16is741a_regWrite(bridge, SC16IS741A_LCR_ADDR, SC16IS741A_REG_SET_GENERAL);
}



/**
 *	\brief Allow use of the FIFO.
 *
 *	\param[in] bridge The SC16IS741A bridge.
 *	\param[in] enable Wether to enable the FIFO.
 */
void
sc16is741a_enableFifo(sc16is741a_device bridge, bool enable)
{
	if (enable)
	{
		sc16is741_setupFifoBuffers(bridge);
	}

	REG_MODIFY(bridge, SC16IS741A_FCR, reg.FIFO_EN = enable ? 0b1U : 0b0U;
	)
}


/* Public fuctions
------------------------------------------------------------------------------------------------------------------------- */

/**
 *	\brief Initialize SC16IS741A bridge.
 *
 *	\param[in] chipSelLine The chip select line for SPI.
 *	\param[in] spiClockSpeed The SPI clock speed.
 *	\param[in] uartBaudrate The UART baud rate.
 */
sc16is741a_device
sc16is741a_init(uint8_t chipSelLine, uint32_t spiDataRate, uint32_t uartBaudrate)
{
	sc16is741a_device bridge = calloc(1, sizeof(sc16is741a_device_t));
	if (bridge == NULL)
	{
		/* [error]: could not allocate memory for bridge. */
		return NULL;
	}

	spi_config_t spi_config = 
	{
		.dataRate = spiDataRate,
		.dataMode = spi_dataMode_0,
		.bitOrder = spi_bitOrder_msbFirst,
		.csPin = chipSelLine
	};

	bridge->spi = spi_init(spi_config);
	if (bridge->spi == NULL)
	{
		/* [error]: could not initialize spi */
		free(bridge);
		return NULL;
	}

	sc16is741a_setUartBaudrate(bridge);

	return bridge;
}



/**
 *	\brief Initialize SC16IS741A bridge.
 *
 *	\param[in] chipSelLine The chip select line for SPI.
 *	\param[in] spiClockSpeed The SPI clock speed.
 *	\param[in] uartBaudrate The UART baud rate.
 */
void
sc16is741a_uninit(sc16is741a_device bridge)
{
	if (bridge == NULL)
	{
		/* [error]: bridge is null. */
		return;
	}
	platform_spi_uninit(bridge->spi);
	free(bridge);
}



/**
 *	\brief Write through the SC16IS741A bridge.
 *
 *	\param[in] bridge The SC16IS741A bridge.
 *	\param[in] src The source data to write.
 *	\param[in] src_len The length of the source.
 *	\returns The success of the operation.
 */
int
sc16is741a_write(sc16is741a_device bridge, const void* src, size_t src_len)
{
	if (src_len > SC16IS741A_FIFO_MAX)
	{
		/* TODO: watch for backing off on TX */
	}
	
	memcpy(bridge->tx_buffer + sizeof(union __sc16is741a_reg_addr_byte__), src, src_len);

	spi_transferBuffer(bridge->spi, bridge->tx_buffer, bridge->rx_buffer + 1, src_len);

	return 0U;
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
int
sc16is741a_read(sc16is741a_device bridge, void* dest, size_t dest_len, size_t* read_in)
{
	if (*read_in > SC16IS741A_FIFO_MAX)
	{
		/* TODO: watch for backing off on TX */
	}	

	spi_transferBuffer(bridge->spi, bridge->rx_buffer, bridge->rx_buffer + 1, dest_len);
	memcpy(dest, bridge->rx_buffer + sizeof(union __sc16is741a_reg_addr_byte__) + 1, *read_in);

	return 0U;
}

