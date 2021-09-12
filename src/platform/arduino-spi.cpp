// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

/* 
* arduinoSpi.cpp provides the translation between LTEm1 platform "C" and Arduino "C++".
* Each platform function has a matching Arduino C++ free-function to wrap arduino functions.
------------------------------------------------------------------------------------------------------------------------- */

#include <Arduino.h>
//#include <Wire.h>
#include <SPI.h>

#include "lqPlatform-spi.h"

typedef struct arduino_spi_tag
{
    SPISettings nxpSettings;
    int8_t irqNumber;
} arduino_spi_t;

static arduino_spi_t arduino_spi_settings;


/**
 *	\brief Initialize and configure SPI resource.
 *
 *  \param chipSelLine [in] - GPIO for CS 
 *  \return SPI device
 */
void *spi_create(uint8_t chipSelLine)
{
    // spi_settings = SPISettings(config.dataRate, (BitOrder)config.bitOrder, config.dataMode);

    spi_t *spi = (spi_t*)malloc(sizeof(spi_t));
	if (spi == NULL)
	{
		return NULL;
	}	

    spi->dataRate = 2000000U;
    spi->dataMode = spiDataMode_0;
    spi->bitOrder = spiBitOrder_msbFirst;
    spi->csPin = chipSelLine;

    arduino_spi_settings.nxpSettings = SPISettings(spi->dataRate, (BitOrder)spi->bitOrder, spi->dataMode);
    arduino_spi_settings.irqNumber = SPI_NO_IRQ_PROTECTION;

    return spi;
}



/**
 *	\brief Start SPI facility.
 */
void spi_start(void *spi)
{
    spi_t *spiPtr = (spi_t*)spi;
    digitalWrite(spiPtr->csPin, HIGH);
    pinMode(spiPtr->csPin, OUTPUT);
    SPI.begin();
}



/**
 *	\brief Shutdown SPI facility.
 */
void spi_stop(void *spi)
{
    SPI.end();
}


/**
 *	\brief Gaurd SPI resource from recursive interrupts.
 */
void spi_usingInterrupt(void *spi, int8_t irqNumber)
{
        SPI.usingInterrupt(irqNumber);
}


/**
 *	\brief Gaurd SPI resource from recursive interrupts.
 */
void spi_notUsingInterrupt(void *spi, int8_t irqNumber)
{
    SPI.notUsingInterrupt(irqNumber);
}



/**
 *	\brief Transfer a byte to the NXP bridge.
 *
 *	\param spi [in] - The SPI device for communications.
 *  \param data [in/out] - The word to transfer to the NXP bridge.
 * 
 *  \returns A 16-bit word received during the transfer.
 */
uint8_t spi_transferByte(void *spi, uint8_t data)
{
    spi_t *spiPtr = (spi_t*)spi;

    digitalWrite(spiPtr->csPin, LOW);
    SPI.beginTransaction(arduino_spi_settings.nxpSettings);

    uint8_t result = SPI.transfer(data);

    digitalWrite(spiPtr->csPin, HIGH);
    SPI.endTransaction();
    return result;
}



/**
 *	\brief Transfer a word (16-bits) to the NXP bridge.
 *
 *	\param spi [in] - The SPI device for communications.
 *  \param data [in/out] - The word to transfer to the NXP bridge.
 * 
 *  \returns A 16-bit word received during the transfer.
 */
uint16_t spi_transferWord(void *spi, uint16_t data)
{
    union { uint16_t val; struct { uint8_t msb; uint8_t lsb; }; } t;
    spi_t *spiPtr = (spi_t*)spi;

    digitalWrite(spiPtr->csPin, LOW);
    SPI.beginTransaction(arduino_spi_settings.nxpSettings);

    t.val = data;
    if (spiPtr->bitOrder == spiBitOrder_msbFirst)
    {
        t.msb = SPI.transfer(t.msb);
        t.lsb = SPI.transfer(t.lsb);
    }
    else
    {
        t.lsb = SPI.transfer(t.lsb);
        t.msb = SPI.transfer(t.msb);
    }
    
    digitalWrite(spiPtr->csPin, HIGH);
    SPI.endTransaction();

    return t.val;
}



/**
 *	\brief Transfer a buffer to the NXP bridge.
 *
 *	\param spi [in] - The SPI device for communications.
 *  \param regAddrByte [in] - Bridge register address specifying the I/O to perform.
 *  \param buf [in/out] - The character pointer to the buffer to transfer to/from.
 *  \param xfer_len [in] - The number of characters to transfer.
 */
void spi_transferBuffer(void *spi, uint8_t regAddrByte, void* buf, size_t xfer_len)
{
    spi_t *spiPtr = (spi_t*)spi;

    digitalWrite(spiPtr->csPin, LOW);
    SPI.beginTransaction(arduino_spi_settings.nxpSettings);

    SPI.transfer(regAddrByte);
    SPI.transfer(buf, xfer_len);

    digitalWrite(spiPtr->csPin, HIGH);
    SPI.endTransaction();
}

