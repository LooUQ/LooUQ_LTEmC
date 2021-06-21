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

#ifdef __cplusplus
#include <cstdint>
#include <stddef.h>
#else
#include <stdint.h>
#include <stddef.h>
#endif

#define SPI_NO_IRQ_PROTECTION -1


typedef enum
{
    spiBitOrder_lsbFirst = 0x0,
    spiBitOrder_msbFirst = 0x1
} spiBitOrder_t;


/* Arduino SPI
#define SPI_MODE0 0x02
#define SPI_MODE1 0x00
#define SPI_MODE2 0x03
#define SPI_MODE3 0x01
*/

typedef enum
{
    spiDataMode_0 = 0x02,
    spiDataMode_1 = 0x00,
    spiDataMode_2 = 0x03,
    spiDataMode_3 = 0x01
} spiDataMode_t;


typedef struct spiConfig_tag
{
    uint32_t dataRate;
    spiDataMode_t dataMode;
    spiBitOrder_t bitOrder;
    uint8_t csPin;
} spiConfig_t;


typedef struct spiDevice_tag
{
    spiConfig_t *config;
} spiDevice_t;

//typedef spi_device_t* spi_device;


#ifdef __cplusplus
extern "C"
{
#endif


spiDevice_t *spi_create(uint8_t chipSelLine);
void spi_start(spiDevice_t *spi);
void spi_stop(spiDevice_t *spi);

void spi_protectFromInterrupt(spiDevice_t *spi, int8_t irqNumber);

uint8_t spi_transferByte(spiDevice_t *spi, uint8_t writeVal);
uint16_t spi_transferWord(spiDevice_t *spi, uint16_t writeVal);

void spi_transferBuffer(spiDevice_t *spi, uint8_t regAddrByte, void* buf, size_t xfer_len);


#ifdef __cplusplus
}
#endif // __cplusplus

#endif  /* !__LQPLATFORM_SPI_H__ */
