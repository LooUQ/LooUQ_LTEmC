/******************************************************************************
 *  \file platform_spi.h
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
#ifndef __PLATFORM_SPI_H__
#define __PLATFORM_SPI_H__

#ifdef __cplusplus
extern "C"
{
#include <cstdint>
#include <stddef.h>
#else
#include <stdint.h>
#include <stddef.h>
#endif // __cplusplus

#define SPI_DATA_RATE 2000000U
#define SPI_NO_IRQ_PROTECTION -1


typedef enum
{
    spi_bitOrder_lsbFirst = 0x0,
    spi_bitOrder_msbFirst = 0x1
} spi_bitOrder_t;

/* Arduino SPI
#define SPI_MODE0 0x02
#define SPI_MODE1 0x00
#define SPI_MODE2 0x03
#define SPI_MODE3 0x01
*/
typedef enum
{
    spi_dataMode_0 = 0x02,
    spi_dataMode_1 = 0x00,
    spi_dataMode_2 = 0x03,
    spi_dataMode_3 = 0x01
} spi_dataMode_t;


typedef struct spi_config_tag
{
    uint32_t dataRate;
    spi_dataMode_t dataMode;
    spi_bitOrder_t bitOrder;
    uint8_t csPin;
} spi_config_t;


typedef struct spi_device_tag
{
	spi_config_t *config;
} spi_device_t;

//typedef spi_device_t* spi_device;

spi_device_t *spi_create(uint8_t chipSelLine);
void spi_start(spi_device_t *spi);
void spi_stop(spi_device_t *spi);

void spi_protectFromInterrupt(spi_device_t *spi, int8_t irqNumber);

uint8_t spi_transferByte(spi_device_t *spi, uint8_t writeVal);
uint16_t spi_transferWord(spi_device_t *spi, uint16_t writeVal);

void spi_transferBuffer(spi_device_t *spi, uint8_t regAddrByte, void* buf, size_t xfer_len);


#ifdef __cplusplus
}
#endif // __cplusplus



#endif  /* !__PLATFORM_SPI_H__ */