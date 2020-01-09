/******************************************************************************
 *  \file platform_spi.h
 *  \author Jensen Miller
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
#else
#include <stdint.h>
#endif // __cplusplus


typedef enum
{
    PLATFORM_SPI_MSB_FIRST,
    PLATFORM_SPI_LSB_FIRST
} platform_spi_byte_order_t;



typedef enum
{
    PLATFORM_SPI_MODE_0,
    PLATFORM_SPI_MODE_1,
    PLATFORM_SPI_MODE_2,
    PLATFORM_SPI_MODE_3
} platform_spi_mode_t;



typedef struct
{
    uint32_t clock_frequency;
    uint8_t chip_select_line;
    platform_spi_byte_order_t byte_order;
    platform_spi_mode_t mode;
} platform_spi_settings_t;


typedef struct platform_spi_device_tag* platform_spi_device;


platform_spi_device platform_spi_init(platform_spi_settings_t* spiSettings);
void platform_spi_uninit(platform_spi_device spiDevice);

uint8_t platform_spi_transfer(platform_spi_device spiDevice, uint8_t byteOfData);
int platform_spi_transfern(platform_spi_device spiDevice, const void* src, size_t src_len, void* dest, size_t dest_len);


#ifdef __cplusplus
}
#endif // __cplusplus



#endif  /* !__PLATFORM_SPI_H__ */