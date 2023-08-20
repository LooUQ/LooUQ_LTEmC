// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

/* 
* arduinoSpi.cpp provides the translation between LTEm1 platform "C" and Arduino "C++".
* Each platform function has a matching Arduino C++ free-function to wrap arduino functions.
------------------------------------------------------------------------------------------------------------------------- */

//#define ARDUINO_ARCH_SAMD
#ifdef ARDUINO_ARCH_SAMD

#include <Arduino.h>
#include <SPI.h>
#include "platform-spi.h"
#include "lqdiag.h"


// /**
//  *	@brief Initialize and configure SPI resource.
//  *
//  *  @param [in] clkPin - GPIO pin for clock
//  *  @return Pointer to platformSpi device
//  */
// platformSpi_t* spi_createFromPins(uint8_t clkPin, uint8_t misoPin, uint8_t mosiPin, uint8_t csPin)
// {
//     ASSERT()
//     return 0;
// }


/**
 *	@brief Initialize and configure SPI resource.
 *
 *  @param [in] indx Index (0..n) of the SPI to reference for LTEmC use.
 *  @return Pointer to platformSpi device
 */
platformSpi_t* spi_createFromIndex(uint8_t indx, uint8_t csPin)
{
    platformSpi_t *platformSpi = (platformSpi_t*)calloc(1, sizeof(platformSpi_t));
	if (platformSpi == NULL)
	{
		return NULL;
	}	

    ASSERT(indx < SPI_INTERFACES_COUNT);
    switch (indx)
    {
        #if SPI_INTERFACES_COUNT > 0
        case 0:
            platformSpi->spi = &SPI;
            break;
        #endif
        #if SPI_INTERFACES_COUNT > 1
        case 1:
            platformSpi->spi = &SPI1;
            break;
        #endif
        #if SPI_INTERFACES_COUNT > 2
        case 2:
            platformSpi->spi = &SPI2;
            break;
        #endif
        #if SPI_INTERFACES_COUNT > 3
        case 3:
            platformSpi->spi = &SPI3;
            break;
        #endif
    }

    platformSpi->clkPin = 0;
    platformSpi->misoPin = 0;
    platformSpi->mosiPin = 0;

    platformSpi->csPin = csPin;
    platformSpi->dataRate = 2000000U;
    platformSpi->dataMode = spiDataMode_0;
    platformSpi->bitOrder = spiBitOrder_msbFirst;
    return platformSpi;
}


/**
 *	@brief Start SPI facility.
 */
void spi_start(platformSpi_t* platformSpi)
{
    digitalWrite(platformSpi->csPin, HIGH);
    pinMode(platformSpi->csPin, OUTPUT);

    ASSERT(platformSpi->spi != NULL);
    ((SPIClass*)platformSpi->spi)->begin();
}


/**
 *	@brief Shutdown SPI facility.
 */
void spi_stop(platformSpi_t* platformSpi)
{
    ((SPIClass*)platformSpi->spi)->end();
}


/**
 *	@brief Gaurd SPI resource from recursive interrupts.
 */
void spi_usingInterrupt(platformSpi_t* platformSpi, int8_t irqNumber)
{
        ((SPIClass*)platformSpi->spi)->usingInterrupt(irqNumber);
}


/**
 *	@brief Gaurd SPI resource from recursive interrupts.
 */
void spi_notUsingInterrupt(platformSpi_t* platformSpi, int8_t irqNumber)
{
    ((SPIClass*)platformSpi->spi)->notUsingInterrupt(irqNumber);
}



/**
 *	@brief Transfer a byte to the NXP bridge.
 *
 *	@param spi [in] - The SPI device for communications.
 *  @param data [in/out] - The word to transfer to the NXP bridge.
 * 
 *  \returns A 16-bit word received during the transfer.
 */
uint8_t spiConfig_transferByte(platformSpi_t *platformSpi, uint8_t data)
{
    digitalWrite(platformSpi->csPin, LOW);
    ((SPIClass*)platformSpi->spi)->beginTransaction(SPISettings(platformSpi->dataRate, (BitOrder)platformSpi->bitOrder, (uint8_t)platformSpi->dataMode));

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
    union { uint16_t val; struct { uint8_t msb; uint8_t lsb; }; } t;

    digitalWrite(platformSpi->csPin, LOW);
    ((SPIClass*)platformSpi->spi)->beginTransaction(SPISettings(platformSpi->dataRate, (BitOrder)platformSpi->bitOrder, (uint8_t)platformSpi->dataMode));

    t.val = data;
    if (platformSpi->bitOrder == spiBitOrder_msbFirst)
    {
        t.msb = ((SPIClass*)(platformSpi->spi))->transfer(t.msb);
        t.lsb = ((SPIClass*)(platformSpi->spi))->transfer(t.lsb);
    }
    else
    {
        t.lsb = ((SPIClass*)(platformSpi->spi))->transfer(t.lsb);
        t.msb = ((SPIClass*)(platformSpi->spi))->transfer(t.msb);
    }
   
    digitalWrite(platformSpi->csPin, HIGH);
    ((SPIClass*)(platformSpi->spi))->endTransaction();

    return t.val;
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
    ((SPIClass*)(platformSpi->spi))->beginTransaction(SPISettings(platformSpi->dataRate, (BitOrder)platformSpi->bitOrder, (uint8_t)platformSpi->dataMode));

    ((SPIClass*)(platformSpi->spi))->transfer(addressByte);
    ((SPIClass*)(platformSpi->spi))->transfer(buf, xfer_len);

    digitalWrite(platformSpi->csPin, HIGH);
    ((SPIClass*)(platformSpi->spi))->endTransaction();
}


void spi_writeBuffer(platformSpi_t* platformSpi, uint8_t addressByte, void* buf, uint16_t xfer_len)
{   
    digitalWrite(platformSpi->csPin, LOW);
    ((SPIClass*)(platformSpi->spi))->beginTransaction(SPISettings(platformSpi->dataRate, (BitOrder)platformSpi->bitOrder, (uint8_t)platformSpi->dataMode));

    ((SPIClass*)(platformSpi->spi))->transfer(addressByte);
    for (uint16_t i = 0; i < xfer_len; i++)
    {
        ((SPIClass*)(platformSpi->spi))->transfer(*((uint8_t*)buf + i));
    }

    digitalWrite(platformSpi->csPin, HIGH);
    ((SPIClass*)(platformSpi->spi))->endTransaction();
}


void spi_readBuffer(platformSpi_t* platformSpi, uint8_t addressByte, void* buf, uint16_t xfer_len)
{
    digitalWrite(platformSpi->csPin, LOW);
    ((SPIClass*)(platformSpi->spi))->beginTransaction(SPISettings(platformSpi->dataRate, (BitOrder)platformSpi->bitOrder, (uint8_t)platformSpi->dataMode));

    ((SPIClass*)(platformSpi->spi))->transfer(addressByte);
    for (uint16_t i = 0; i < xfer_len; i++)
    {
        *((uint8_t*)(buf + i)) = ((SPIClass*)(platformSpi->spi))->transfer(0xFF);
    }

    digitalWrite(platformSpi->csPin, HIGH);
    ((SPIClass*)(platformSpi->spi))->endTransaction();
}

#endif // ifdef SAMD
