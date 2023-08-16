// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

/* 
* arduinoSpi.cpp provides the translation between LTEm1 platform "C" and Arduino "C++".
* Each platform function has a matching Arduino C++ free-function to wrap arduino functions.
------------------------------------------------------------------------------------------------------------------------- */

#ifdef ARDUINO_ARCH_ESP32

#include "platform-spi.h"
#include <Arduino.h>
#include <SPI.h>


/**
 *	@brief Initialize and configure SPI resource.
 *
 *  @param chipSelLine [in] - GPIO for CS 
 *  \return SPI device
 */
platformSpi_t* spi_create(uint8_t clkPin, uint8_t misoPin, uint8_t mosiPin, uint8_t csPin)
{
    platformSpi_t *platformSpi = (platformSpi_t*)malloc(sizeof(platformSpi_t));
	if (platformSpi == NULL)
	{
		return NULL;
	}	
    platformSpi->dataRate = 2000000U;
    platformSpi->dataMode = spiDataMode_0;
    platformSpi->bitOrder = spiBitOrder_msbFirst;
    platformSpi->clkPin = clkPin;
    platformSpi->misoPin = misoPin;
    platformSpi->mosiPin = mosiPin;
    platformSpi->csPin = csPin;
    platformSpi->spi = new SPIClass(HSPI);
    return platformSpi;
}



/**
 *	@brief Start SPI facility.
 */
void spi_start(platformSpi_t* platformSpi)
{
    digitalWrite(platformSpi->csPin, HIGH);
    pinMode(platformSpi->csPin, OUTPUT);
    ((SPIClass*)platformSpi->spi)->begin(platformSpi->clkPin, platformSpi->misoPin, platformSpi->mosiPin, platformSpi->csPin);
}



/**
 *	@brief Shutdown SPI facility.
 */
void spi_stop(platformSpi_t* spi)
{
    SPI.end();
}


/**
 *	@brief Gaurd SPI resource from recursive interrupts.
 */
void spi_usingInterrupt(platformSpi_t* spi, int8_t irqNumber)
{
    return;
}


/**
 *	@brief Gaurd SPI resource from recursive interrupts.
 */
void spi_notUsingInterrupt(platformSpi_t* spi, int8_t irqNumber)
{
    return;
}



/**
 *	@brief Transfer a byte to the NXP bridge.
 *
 *	@param spi [in] - The SPI device for communications.
 *  @param data [in/out] - The word to transfer to the NXP bridge.
 * 
 *  \returns A 16-bit word received during the transfer.
 */
uint8_t spi_transferByte(platformSpi_t* platformSpi, uint8_t data)
{
    digitalWrite(platformSpi->csPin, LOW);
    ((SPIClass*)platformSpi->spi)->beginTransaction(SPISettings(platformSpi->dataRate, platformSpi->bitOrder, platformSpi->dataMode));

    uint8_t result = ((SPIClass*)platformSpi->spi)->transfer(data);

    digitalWrite(platformSpi->csPin, HIGH);
    ((SPIClass*)platformSpi->spi)->endTransaction();
    return result;
}



/**
 *	@brief Transfer a word (16-bits) to the NXP bridge.
 *
 *	@param spi [in] - The SPI device for communications.
 *  @param data [in/out] - The word to transfer to the NXP bridge.
 * 
 *  \returns A 16-bit word received during the transfer.
 */
uint16_t spi_transferWord(platformSpi_t* platformSpi, uint16_t data)
{
    union { uint16_t val; struct { uint8_t msb; uint8_t lsb; }; } tw;

    digitalWrite(platformSpi->csPin, LOW);
    ((SPIClass*)platformSpi->spi)->beginTransaction(SPISettings(platformSpi->dataRate, platformSpi->bitOrder, platformSpi->dataMode));

    tw.val = data;
    if (platformSpi->bitOrder == spiBitOrder_msbFirst)
    {
        tw.msb = ((SPIClass*)platformSpi->spi)->transfer(tw.msb);
        tw.lsb = ((SPIClass*)platformSpi->spi)->transfer(tw.lsb);
    }
    else
    {
        tw.lsb = ((SPIClass*)platformSpi->spi)->transfer(tw.lsb);
        tw.msb = ((SPIClass*)platformSpi->spi)->transfer(tw.msb);
    }
    digitalWrite(platformSpi->csPin, HIGH);
    ((SPIClass*)platformSpi->spi)->endTransaction();

    return tw.val;
}



/**
 *	@brief Transfer a buffer to the SPI device.
 *
 *	@param spi [in] The SPI device for communications.
 *  @param addressByte [in] Optional address byte sent before buffer, can specify specifics of the I/O being initiated.
 *  @param buf [in/out] - The character pointer to the buffer to transfer to/from.
 *  @param xfer_len [in] - The number of characters to transfer.
 */
void spi_transferBuffer(platformSpi_t* platformSpi, uint8_t addressByte, void* buf, uint16_t xfer_len)
{
    digitalWrite(platformSpi->csPin, LOW);
    ((SPIClass*)platformSpi->spi)->beginTransaction(SPISettings(platformSpi->dataRate, platformSpi->bitOrder, platformSpi->dataMode));

    ((SPIClass*)platformSpi->spi)->transfer(addressByte);
    ((SPIClass*)platformSpi->spi)->transfer(buf, xfer_len);

    digitalWrite(platformSpi->csPin, HIGH);
    ((SPIClass*)platformSpi->spi)->endTransaction();
}


void spi_writeBuffer(platformSpi_t* platformSpi, uint8_t addressByte, void* buf, uint16_t xfer_len)
{
    digitalWrite(platformSpi->csPin, LOW);
    ((SPIClass*)platformSpi->spi)->beginTransaction(SPISettings(platformSpi->dataRate, platformSpi->bitOrder, platformSpi->dataMode));

    ((SPIClass*)platformSpi->spi)->transfer(addressByte);
    for (uint16_t i = 0; i < xfer_len; i++)
    {
        ((SPIClass*)platformSpi->spi)->transfer(*((uint8_t*)buf + i));
    }

    digitalWrite(platformSpi->csPin, HIGH);
    ((SPIClass*)platformSpi->spi)->endTransaction();
}


void spi_readBuffer(platformSpi_t* platformSpi, uint8_t addressByte, void* buf, uint16_t xfer_len)
{
    digitalWrite(platformSpi->csPin, LOW);
    ((SPIClass*)platformSpi->spi)->beginTransaction(SPISettings(platformSpi->dataRate, platformSpi->bitOrder, platformSpi->dataMode));

    ((SPIClass*)platformSpi->spi)->transfer(addressByte);
    for (uint16_t i = 0; i < xfer_len; i++)
    {
        *((uint8_t*)(buf + i)) = ((SPIClass*)platformSpi->spi)->transfer(0xFF);
    }

    digitalWrite(platformSpi->csPin, HIGH);
    ((SPIClass*)platformSpi->spi)->endTransaction();
}

#endif