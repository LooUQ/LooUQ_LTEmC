// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

/* 
* arduinoSpi.cpp provides the translation between LTEm1 platform "C" and Arduino "C++".
* Each platform function has a matching Arduino C++ free-function to wrap arduino functions.
------------------------------------------------------------------------------------------------------------------------- */

#include "lqPlatform-spi.h"
//#include <Arduino.h>
//#include <Wire.h>
#include <SPI.h>

typedef struct arduino_spi_tag
{
    SPISettings nxpSettings;
    int8_t irqNumber;
} arduino_spi_t;

static arduino_spi_t arduino_spi_settings;


/**
 *	\brief Initialize and configure SPI resource.
 */
spiDevice_t *spi_create(uint8_t chipSelLine)
{
    //spi_settings = new SPISettings(config.dataRate, (BitOrder)config.bitOrder, config.dataMode);

    spiDevice_t *spi = (spiDevice_t*)malloc(sizeof(spiDevice_t));
	if (spi == NULL)
	{
		return NULL;
	}	

    spi->config = (spiConfig_t*)malloc(sizeof(spiConfig_t));
	if (spi->config == NULL)
	{
        free(spi);
		return NULL;
	}

    spi->config->dataRate = 2000000U;
    spi->config->dataMode = spiDataMode_0;
    spi->config->bitOrder = spiBitOrder_msbFirst;
    spi->config->csPin = chipSelLine;

    arduino_spi_settings.nxpSettings = SPISettings(spi->config->dataRate, (BitOrder)spi->config->bitOrder, spi->config->dataMode);
    arduino_spi_settings.irqNumber = SPI_NO_IRQ_PROTECTION;

    return spi;
}



/**
 *	\brief Start SPI facility.
 */
void spi_start(spiDevice_t *spi)
{
    digitalWrite(spi->config->csPin, HIGH);
    pinMode(spi->config->csPin, OUTPUT);

    SPI.begin();
}



/**
 *	\brief Shutdown SPI facility.
 */
void spi_stop(spiDevice_t *spi)
{
    SPI.end();
}



/**
 *	\brief Uninitialize and deallocate memory from the SPI resource.
 */
void spi_protectFromInterrupt(spiDevice_t *spi, int8_t irqNumber)
{
    if (irqNumber == SPI_NO_IRQ_PROTECTION)
        SPI.notUsingInterrupt(irqNumber);
    else
        SPI.usingInterrupt(irqNumber);
}



/**
 *	\brief Transfer a byte to the NXP bridge.
 *
 *	\param[in] spi The SPI device for communications.
 *  \param[in\out] data The word to transfer to the NXP bridge.
 * 
 *  \returns A 16-bit word received during the transfer.
 */
uint8_t spi_transferByte(spiDevice_t *spi, uint8_t data)
{
    digitalWrite(spi->config->csPin, LOW);
    SPI.beginTransaction(arduino_spi_settings.nxpSettings);

    uint8_t result = SPI.transfer(data);

    digitalWrite(spi->config->csPin, HIGH);
    SPI.endTransaction();
    return result;
}



/**
 *	\brief Transfer a word (16-bits) to the NXP bridge.
 *
 *	\param[in] spi The SPI device for communications.
 *  \param[in\out] data The word to transfer to the NXP bridge.
 * 
 *  \returns A 16-bit word received during the transfer.
 */
uint16_t spi_transferWord(spiDevice_t *spi, uint16_t data)
{
    union { uint16_t val; struct { uint8_t msb; uint8_t lsb; }; } t;

    digitalWrite(spi->config->csPin, LOW);
    SPI.beginTransaction(arduino_spi_settings.nxpSettings);

    t.val = data;
    if (spi->config->bitOrder == spiBitOrder_msbFirst)
    {
        t.msb = SPI.transfer(t.msb);
        t.lsb = SPI.transfer(t.lsb);
    }
    else
    {
        t.lsb = SPI.transfer(t.lsb);
        t.msb = SPI.transfer(t.msb);
    }
    
    digitalWrite(spi->config->csPin, HIGH);
    SPI.endTransaction();

    return t.val;
}



/**
 *	\brief Transfer a buffer to the NXP bridge.
 *
 *	\param[in] spi The SPI device for communications.
 *  \param[in] regAddrByte Bridge register address specifying the I/O to perform.
 *  \param[in\out] buf The character pointer to the buffer to transfer to/from.
 *  \param[in] xfer_len The number of characters to transfer.
 */
void spi_transferBuffer(spiDevice_t *spi, uint8_t regAddrByte, void* buf, size_t xfer_len)
{
    digitalWrite(spi->config->csPin, LOW);
    SPI.beginTransaction(arduino_spi_settings.nxpSettings);

    SPI.transfer(regAddrByte);
    SPI.transfer(buf, xfer_len);

    digitalWrite(spi->config->csPin, HIGH);
    SPI.endTransaction();
}

