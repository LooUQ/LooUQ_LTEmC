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
platformSpi_t* spi_createFromPins(uint8_t clkPin, uint8_t misoPin, uint8_t mosiPin, uint8_t csPin)
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


void spi_transferBegin(platformSpi_t* platformSpi)
{
    if (!platformSpi->transactionActive)
    {
        platformSpi->transactionActive = true;
        digitalWrite(platformSpi->csPin, LOW);
        ((SPIClass*)platformSpi->spi)->beginTransaction(SPISettings(platformSpi->dataRate, platformSpi->bitOrder, platformSpi->dataMode));
    }
}


void spi_transferEnd(platformSpi_t* platformSpi)
{
    if (platformSpi->transactionActive)
    {
        digitalWrite(platformSpi->csPin, HIGH);
        ((SPIClass*)platformSpi->spi)->endTransaction();
        platformSpi->transactionActive = false;
    }
}


/**
 *	@brief Transfer a byte to the NXP bridge.
 *
 *	@param spi [in] - The SPI device for communications.
 *  @param data [in/out] - The word to transfer to the NXP bridge.
 * 
 *  \returns A 16-bit word received during the transfer.
 */
uint8_t spi_transferByte(platformSpi_t* platformSpi, uint8_t txData)
{
    bool priorTranState = platformSpi->transactionActive;

    if (!priorTranState)
        spi_transferBegin(platformSpi);

    uint8_t rxData = ((SPIClass*)platformSpi->spi)->transfer(txData);

    if (!priorTranState)
        spi_transferEnd(platformSpi);

    return rxData;
}



/**
 *	@brief Transfer a word (16-bits) to the NXP bridge.
 *
 *	@param spi [in] - The SPI device for communications.
 *  @param data [in/out] - The word to transfer to the NXP bridge.
 * 
 *  \returns A 16-bit word received during the transfer.
 */
uint16_t spi_transferWord(platformSpi_t* platformSpi, uint16_t txData)
{
    union { uint16_t val; struct { uint8_t msb; uint8_t lsb; }; } tw;

    bool priorTranState = platformSpi->transactionActive;

    if (!priorTranState)
        spi_transferBegin(platformSpi);

    tw.val = txData;
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

    if (!priorTranState)
        spi_transferEnd(platformSpi);

    return tw.val;
}

/**
 *	@brief Transfer a block of bytes to/from the SPI device.
 */
void spi_transferBytes(platformSpi_t* platformSpi, const uint8_t* txBuf, uint8_t* rxBuf, uint16_t xferLen)
{
    bool priorTranState = platformSpi->transactionActive;

    if (!priorTranState)
        spi_transferBegin(platformSpi);

    ((SPIClass*)platformSpi->spi)->transferBytes(txBuf, rxBuf, xferLen);

    if (!priorTranState)
        spi_transferEnd(platformSpi);
}


// /**
//  *	@brief Transfer a data block to the SPI device.
//  */
// void spi_transferBlock(platformSpi_t* platformSpi, uint8_t addressByte, const uint8_t* txBuf,  uint8_t* rxBuf, uint16_t xferLen)
// {
//     digitalWrite(platformSpi->csPin, LOW);
//     ((SPIClass*)platformSpi->spi)->beginTransaction(SPISettings(platformSpi->dataRate, platformSpi->bitOrder, platformSpi->dataMode));

//     ((SPIClass*)platformSpi->spi)->transfer(addressByte);
//     ((SPIClass*)platformSpi->spi)->transferBytes(txBuf, rxBuf, xferLen);

//     digitalWrite(platformSpi->csPin, HIGH);
//     ((SPIClass*)platformSpi->spi)->endTransaction();
// }


// void spi_writeBuffer(platformSpi_t* platformSpi, uint8_t addressByte, void* buf, uint16_t xfer_len)
// {
//     digitalWrite(platformSpi->csPin, LOW);
//     ((SPIClass*)platformSpi->spi)->beginTransaction(SPISettings(platformSpi->dataRate, platformSpi->bitOrder, platformSpi->dataMode));

//     ((SPIClass*)platformSpi->spi)->transfer(addressByte);
//     for (uint16_t i = 0; i < xfer_len; i++)
//     {
//         ((SPIClass*)platformSpi->spi)->transfer(*((uint8_t*)buf + i));
//     }

//     digitalWrite(platformSpi->csPin, HIGH);
//     ((SPIClass*)platformSpi->spi)->endTransaction();
// }


// void spi_readBuffer(platformSpi_t* platformSpi, uint8_t addressByte, void* buf, uint16_t xfer_len)
// {
//     digitalWrite(platformSpi->csPin, LOW);
//     ((SPIClass*)platformSpi->spi)->beginTransaction(SPISettings(platformSpi->dataRate, platformSpi->bitOrder, platformSpi->dataMode));

//     ((SPIClass*)platformSpi->spi)->transfer(addressByte);
//     for (uint16_t i = 0; i < xfer_len; i++)
//     {
//         *((uint8_t*)(buf + i)) = ((SPIClass*)platformSpi->spi)->transfer(0xFF);
//     }

//     digitalWrite(platformSpi->csPin, HIGH);
//     ((SPIClass*)platformSpi->spi)->endTransaction();
// }

#endif