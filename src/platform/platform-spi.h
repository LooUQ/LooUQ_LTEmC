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


/* Arduino SPI
#define spi_MODE0 0x02
#define spi_MODE1 0x00
#define spi_MODE2 0x03
#define spi_MODE3 0x01
*/

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

uint8_t spi_transferByte(platformSpi_t* platformSpi, uint8_t writeVal);
uint16_t spi_transferWord(platformSpi_t* platformSpi, uint16_t writeVal);

void spi_transferBuffer(platformSpi_t* platformSpi, uint8_t addressByte, void* buf, uint16_t xfer_len);

void spi_writeBuffer(platformSpi_t* platformSpi, uint8_t addressByte, void* buf, uint16_t xfer_len);
void* spi_readBuffer(platformSpi_t* platformSpi, uint8_t addressByte, uint16_t xfer_len);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif  /* !__LQPLATFORM_SPI_H__ */
