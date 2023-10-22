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
 *	@brief Begin a SPI transfer transaction.
 *	@param [in] platformSpi The SPI device for communications.
 */
void spi_transferBegin(platformSpi_t *platformSpi)
{
    if (!platformSpi->transactionActive)
    {
        platformSpi->transactionActive = true;
        digitalWrite(platformSpi->csPin, LOW);
        ((SPIClass*)platformSpi->spi)->beginTransaction(SPISettings(platformSpi->dataRate, (BitOrder)platformSpi->bitOrder, (uint8_t)platformSpi->dataMode));
    }
}


/**
 *	@brief End a SPI transfer transaction.
 *	@param [in] platformSpi The SPI device for communications.
 */
void spi_transferEnd(platformSpi_t *platformSpi)
{
    if (platformSpi->transactionActive)
    {
        platformSpi->transactionActive = false;
        digitalWrite(platformSpi->csPin, HIGH);
        ((SPIClass*)platformSpi->spi)->endTransaction();
    }
}


/**
 *	@brief Transfer a byte to the NXP bridge.
 *	@param spi [in] - The SPI device for communications.
 *  @param data [in/out] - The word to transfer to the NXP bridge.
 *  @return A 16-bit word received during the transfer.
 */
uint8_t spi_transferByte(platformSpi_t *platformSpi, uint8_t txData)
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
 *	@param spi [in] - The SPI device for communications.
 *  @param data [in/out] - The word to transfer to the NXP bridge.
 *  @return A 16-bit word received during the transfer.
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
        tw.msb = ((SPIClass*)(platformSpi->spi))->transfer(tw.msb);
        tw.lsb = ((SPIClass*)(platformSpi->spi))->transfer(tw.lsb);
    }
    else
    {
        tw.lsb = ((SPIClass*)(platformSpi->spi))->transfer(tw.lsb);
        tw.msb = ((SPIClass*)(platformSpi->spi))->transfer(tw.msb);
    }
   
    if (!priorTranState)
        spi_transferEnd(platformSpi);

    return tw.val;
}


/**
 *	@brief Transfer a buffer to/from the SPI device.
 */
void spi_transferBytes(platformSpi_t* platformSpi, const uint8_t* txBuf, uint8_t* rxBuf, uint16_t xferLen)
{
    bool priorTranState = platformSpi->transactionActive;

    if (!priorTranState)
        spi_transferBegin(platformSpi);

    uint16_t bffrPtr = 0;
    for (size_t i=0; i < xferLen; i++) {
        uint8_t txData = (txBuf != NULL) ? txBuf[i] : 0;
        uint8_t rxData = ((SPIClass*)(platformSpi->spi))->transfer(txData);
        if (rxBuf != NULL)
        {
            *rxBuf = rxData;
            rxBuf++;
        }
    }

    if (!priorTranState)
        spi_transferEnd(platformSpi);
}


// /**
//  *	@brief Transfer a block of data to/from the SPI device.
//  */
// void spi_transferBlock(platformSpi_t* platformSpi, uint8_t addressByte, const uint8_t* txBuf, uint8_t* rxBuf, uint16_t xferLen)
// {
//     digitalWrite(platformSpi->csPin, LOW);
//     ((SPIClass*)(platformSpi->spi))->beginTransaction(SPISettings(platformSpi->dataRate, (BitOrder)platformSpi->bitOrder, (uint8_t)platformSpi->dataMode));

//     ((SPIClass*)(platformSpi->spi))->transfer(addressByte);

//     uint16_t bffrPtr = 0;
//     for (size_t i=0; i < xferLen; i++) {
//         uint8_t tx = (txBuf != NULL) ? txBuf[i] : 0;
//         uint8_t rx = ((SPIClass*)(platformSpi->spi))->transfer(tx);
//         if (rxBuf != NULL)
//         {
//             *rxBuf = rx;
//             rxBuf++;
//         }
//     }
//     // ((SPIClass*)(platformSpi->spi))->transfer(buf, xfer_len);

//     digitalWrite(platformSpi->csPin, HIGH);
//     ((SPIClass*)(platformSpi->spi))->endTransaction();
// }


// void spi_writeBuffer(platformSpi_t* platformSpi, uint8_t addressByte, void* buf, uint16_t xfer_len)
// {   
//     digitalWrite(platformSpi->csPin, LOW);
//     ((SPIClass*)(platformSpi->spi))->beginTransaction(SPISettings(platformSpi->dataRate, (BitOrder)platformSpi->bitOrder, (uint8_t)platformSpi->dataMode));

//     ((SPIClass*)(platformSpi->spi))->transfer(addressByte);
//     for (uint16_t i = 0; i < xfer_len; i++)
//     {
//         ((SPIClass*)(platformSpi->spi))->transfer(*((uint8_t*)buf + i));
//     }

//     digitalWrite(platformSpi->csPin, HIGH);
//     ((SPIClass*)(platformSpi->spi))->endTransaction();
// }


// void spi_readBuffer(platformSpi_t* platformSpi, uint8_t addressByte, void* buf, uint16_t xfer_len)
// {
//     digitalWrite(platformSpi->csPin, LOW);
//     ((SPIClass*)(platformSpi->spi))->beginTransaction(SPISettings(platformSpi->dataRate, (BitOrder)platformSpi->bitOrder, (uint8_t)platformSpi->dataMode));

//     ((SPIClass*)(platformSpi->spi))->transfer(addressByte);
//     for (uint16_t i = 0; i < xfer_len; i++)
//     {
//         *((uint8_t*)(buf + i)) = ((SPIClass*)(platformSpi->spi))->transfer(0xFF);
//     }

//     digitalWrite(platformSpi->csPin, HIGH);
//     ((SPIClass*)(platformSpi->spi))->endTransaction();
// }

#endif // ifdef SAMD
