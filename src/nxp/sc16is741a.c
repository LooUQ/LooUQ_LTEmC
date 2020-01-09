//	Copyright (c) 2020 LooUQ Incorporated.

//	Licensed under The MIT License. See LICENSE in the root directory.
#include "sc16is741a.h"
#include <pal/platform_spi.h>
#include <string.h>



#define REG_MODIFY(pDEVICE, REG_NAME, MODIFY) \
REG_NAME reg = {0}; \
sc16is741a_reg_read(pDEVICE, REG_NAME##_ADDR, &reg.reg); \
MODIFY \
sc16is741a_reg_write(pDEVICE, REG_NAME##_ADDR, &reg.reg);



/**
 *	\brief
 */
typedef struct sc16is741a_device_tag
{
	sc16is741a_reg_set_t active_register_set;
	platform_spi_device spi;
	uint8_t tx_buffer[SC16IS741A_FIFO_MAX + 2];
	uint8_t rx_buffer[SC16IS741A_FIFO_MAX + 2];
} sc16is741a_device_descriptor;



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
 *	\param[in] device The SC16IS741A device.
 */
static inline void
sc16is741_setup_FIFO_buffers(sc16is741a_device device)
{	
	memset(device->tx_buffer, '\0', SC16IS741A_FIFO_MAX+2);
	memset(device->rx_buffer, '\0', SC16IS741A_FIFO_MAX+2);

	union __sc16is741a_reg_addr_byte__* tx_addr_byte = (union __sc16is741a_reg_addr_byte__*)&device->tx_buffer[0];
	tx_addr_byte->A = SC16IS741A_FIFO_ADDR;
	tx_addr_byte->RnW = SC16IS741A_FIFO_RnW_WRITE;
	
	union __sc16is741a_reg_addr_byte__* rx_addr_byte = (union __sc16is741a_reg_addr_byte__*) &device->rx_buffer[0];
	rx_addr_byte->A = SC16IS741A_FIFO_ADDR;
	rx_addr_byte->RnW = SC16IS741A_FIFO_RnW_READ;
}



/**
 *	\brief Write to a SC16IS741A device register.
 *
 *	\param[in] device The SC16IS741A device.
 *	\param[in] reg_addr The register address.
 *	\param[in] reg_data A pointer to the data to write to the register.
 */
static void
sc16is741a_reg_write(sc16is741a_device device, uint8_t reg_addr, const uint8_t * reg_data)
{
	union __sc16is741a_reg_payload__ reg_payload = { 0 };
	reg_payload.reg_addr.A = reg_addr;
	reg_payload.reg_addr.RnW = 0U;
	reg_payload.reg_data = *reg_data;

	platform_spi_transfern(device->spi, &reg_payload.reg_payload, sizeof(union __sc16is741a_reg_payload__),
		&reg_payload.reg_payload, sizeof(union __sc16is741a_reg_payload__));
}



/**
 *	\brief Read from a SC16IS741A device register.
 *
 *	\param[in] device The SC16IS741A device.
 *	\param[in] reg_addr The register address.
 *	\param[out] reg_data A pointer to the destination to read the register into.
 */
static void
sc16is741a_reg_read(sc16is741a_device device, uint8_t reg_addr, uint8_t* reg_dest)
{
	union __sc16is741a_reg_payload__ reg_payload = { 0 };
	reg_payload.reg_addr.A = reg_addr;
	reg_payload.reg_addr.RnW = 1U;	

	platform_spi_transfern(device->spi, &reg_payload.reg_payload, sizeof(union __sc16is741a_reg_payload__),
		&reg_payload.reg_payload, sizeof(union __sc16is741a_reg_payload__));
	*reg_dest = reg_payload.reg_data;
}



/**
 *	\brief Set the register set to be accessible.
 *
 *	\param[in] device The SC16IS741A device;
 *	\param[in] reg_set The register set to access.
 */
static void
sc16is741a_set_register_set(sc16is741a_device device, sc16is741a_reg_set_t reg_set)
{
	if (reg_set == device->active_register_set)
	{
		/* already set, just return */
		return;
	}

	device->active_register_set = reg_set;

	switch (reg_set)
	{
	default:
		break;
	}
}




/**
 *	\brief Initialize SC16IS741A device.
 *
 *	\param[in] chipSelLine The chip select line for SPI.
 *	\param[in] spiClockSpeed The SPI clock speed.
 *	\param[in] uartBaudrate The UART baud rate.
 */
sc16is741a_device
sc16is741a_init(uint8_t chipSelLine, uint32_t spiClockSpeed, uint32_t uartBaudrate)
{
	sc16is741a_device device = (sc16is741a_device)calloc(1, sizeof(sc16is741a_device));
	if (device == NULL)
	{
		/* [error]: could not allocate memory for device. */
		return NULL;
	}

	platform_spi_settings_t spi_settings = { 0 };
	spi_settings.clock_frequency = spiClockSpeed;
	spi_settings.mode = PLATFORM_SPI_MODE_0;
	spi_settings.byte_order = PLATFORM_SPI_MSB_FIRST;
	spi_settings.chip_select_line = chipSelLine;

	device->spi = platform_spi_init(&spi_settings);
	if (device->spi == NULL)
	{
		/* [error]: could not initialize spi */
		free(device);
		return NULL;
	}

	device->active_register_set = SC16IS741A_REG_SET_GENERAL;

	sc16is741a_set_uart_baudrate(device, uartBaudrate);

	return device;
}



/**
 *	\brief Initialize SC16IS741A device.
 *
 *	\param[in] chipSelLine The chip select line for SPI.
 *	\param[in] spiClockSpeed The SPI clock speed.
 *	\param[in] uartBaudrate The UART baud rate.
 */
void
sc16is741a_uninit(sc16is741a_device device)
{
	if (device == NULL)
	{
		/* [error]: device is null. */
		return;
	}
	platform_spi_uninit(device->spi);
	free(device);
}



/**
 *	\brief Set the SC16IS741A device uart baudrate.
 *
 *	\param[in] device The SC16IS741A device.
 *	\param[in] baudrate The baudrate.
 */
void
sc16is741a_set_uart_baudrate(sc16is741a_device device, uint16_t baudrate)
{
	sc16is741a_set_register_set(device, SC16IS741A_REG_SET_SPECIAL);
	/* TODO: write both registers individually. */
	/* sc16is741a_burst_write(device, SC16IS741A_DLL_ADDR, &baudrate, sizeof(baudrate)); */
	sc16is741a_set_register_set(device, SC16IS741A_REG_SET_GENERAL);
}



/**
 *	\brief Write through the SC16IS741A bridge.
 *
 *	\param[in] device The SC16IS741A device.
 *	\param[in] src The source data to write.
 *	\param[in] src_len The length of the source.
 *	\returns The success of the operation.
 */
int
sc16is741a_write(sc16is741a_device device, const void* src, size_t src_len)
{
	if (src_len > SC16IS741A_FIFO_MAX)
	{
		/* TODO: watch for backing off on TX */
	}
	
	memcpy(device->tx_buffer + sizeof(union __sc16is741a_reg_addr_byte__), src, src_len);

	platform_spi_transfern(device->spi, device->tx_buffer, src_len, device->rx_buffer + 1, src_len);

	return 0U;
}



/**
 *	\brief Reads through the SC16IS741A bridge (its RX FIFO).
 *
 *	\param[in] The SC16IS741A device.
 *	\param[out] dest The destination buffer.
 *	\param[in] The length of the destination buffer.
 *	\param[in/out] read_in The amount of bytes read into the destination.
 *	\returns The success of the operation.
 */
int
sc16is741a_read(sc16is741a_device device, void* dest, size_t dest_len, size_t* read_in)
{
	if (*read_in > SC16IS741A_FIFO_MAX)
	{
		/* TODO: watch for backing off on TX */
	}	

	platform_spi_transfern(device->spi, device->rx_buffer, dest_len, device->rx_buffer + 1, dest_len);

	memcpy(dest, device->rx_buffer + sizeof(union __sc16is741a_reg_addr_byte__) + 1, *read_in);

	return 0U;
}






/**
 *	\brief Allow use of the FIFO.
 *
 *	\param[in] device The SC16IS741A device.
 *	\param[in] enable Wether to enable the FIFO.
 */
void
sc16is741a_allow_FIFO(sc16is741a_device device, bool enable)
{
	if (enable)
	{
		sc16is741_setup_FIFO_buffers(device);
	}

	REG_MODIFY(device, SC16IS741A_FCR,
		reg.FIFO_EN = enable ? 0b1U : 0b0U;
	)
}