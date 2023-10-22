/******************************************************************************
 *  \file lqPlatform-spi.h
 *  \author Jensen Miller, Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED
 * "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
 * LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *****************************************************************************/
#ifndef __LQPLATFORM_SPI_H__
#define __LQPLATFORM_SPI_H__

#include <stdint.h>

#define SPI_NO_IRQ_PROTECTION -1


typedef enum
{
    spiBitOrder_lsbFirst = 0x0,
    spiBitOrder_msbFirst = 0x1
} spiBitOrder_t;


typedef enum
{
    spiDataMode_0 = 0x02,
    spiDataMode_1 = 0x00,
    spiDataMode_2 = 0x03,
    spiDataMode_3 = 0x01
} spiDataMode_t;


typedef struct platformSpi_tag
{
    uint32_t dataRate;
    spiDataMode_t dataMode;
    spiBitOrder_t bitOrder;
    uint8_t clkPin;
    uint8_t misoPin;
    uint8_t mosiPin;
    uint8_t csPin;
    void* spi;
    bool transactionActive;
} platformSpi_t;


#ifdef __cplusplus
extern "C"
{
#endif

platformSpi_t* spi_createFromPins(uint8_t clkPin, uint8_t misoPin, uint8_t mosiPin, uint8_t csPin);
platformSpi_t* spi_createFromIndex(uint8_t indx, uint8_t csPin);

void spi_start(platformSpi_t* platformSpi);
void spi_stop(platformSpi_t* platformSpi);

void spi_usingInterrupt(platformSpi_t* platformSpi, int8_t irqNumber);
void spi_notUsingInterrupt(platformSpi_t* platformSpi, int8_t irqNumber);

void spi_transferBegin(platformSpi_t* platformSpi);
void spi_transferEnd(platformSpi_t* platformSpi);

uint8_t spi_transferByte(platformSpi_t* platformSpi, uint8_t txData);
uint16_t spi_transferWord(platformSpi_t* platformSpi, uint16_t txData);

/**
 *	@brief Transfer a block of bytes to/from the SPI device.
 *	@param [in] spi The SPI device for communications.
 *  @param [in] txBuf Pointer to buffer sourcing (transfer from)
 *  @param [out] rxBuf Pointer to buffer receiving (transfer to)
 *  @param [in] xferLen The number of characters to transfer.
 */
void spi_transferBytes(platformSpi_t* platformSpi, const uint8_t* txBuf, uint8_t* rxBuf, uint16_t xferLen);


#ifdef __cplusplus
}
#endif // __cplusplus

#endif  /* !__LQPLATFORM_SPI_H__ */
