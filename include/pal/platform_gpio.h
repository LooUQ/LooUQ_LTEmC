/******************************************************************************
 *  \file platform_gpio.h
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
#ifndef __PLATFORM_GPIO_H__
#define __PLATFORM_GPIO_H__

#ifdef __cplusplus
extern "C"
{
#include <cstdint>
#else
#include <stdint.h>
#endif // __cplusplus


typedef enum
{
    PLATFORM_GPIO_PIN_DIR_INPUT,
    PLATFORM_GPIO_PIN_DIR_OUTPUT,
    PLATFORM_GPIO_PIN_DIR_INPUT_PULLUP,
    PLATFORM_GPIO_PIN_DIR_INPUT_PULLDOWN
} platform_gpio_pin_dir_t;


typedef enum
{
    PLATFORM_GPIO_PIN_VAL_LOW,
    PLATFORM_GPIO_PIN_VAL_HIGH
} platform_gpio_pin_val;


typedef enum
{
    PLATFORM_GPIO_PIN_INT_LOW,
    PLATFORM_GPIO_PIN_INT_HIGH,
    PLATFORM_GPIO_PIN_INT_RISING,
    PLATFORM_GPIO_PIN_INT_FALLING,
    PLATFORM_GPIO_PIN_INT_BOTH
} platform_gpio_pin_interrupt_t;


typedef struct platform_gpio_pin_tag* platform_gpio_pin;

typedef void(*platform_gpio_pin_int_callback)(platform_gpio_pin);

platform_gpio_pin platform_gpio_pin_open(int pin_num, platform_gpio_pin_dir_t pin_dir);
void platform_gpio_pin_close(platform_gpio_pin pin);

void platform_gpio_pin_set_dir(platform_gpio_pin pin, platform_gpio_pin_dir_t pin_dir);

void platform_gpio_pin_write(platform_gpio_pin pin, platform_gpio_pin_val pin_val);
platform_gpio_pin_val platform_gpio_pin_read(platform_gpio_pin pin);

void platform_gpio_pin_allow_interrupt(platform_gpio_pin pin, bool enable, platform_gpio_pin_interrupt_t int_type, platform_gpio_pin_int_callback callback);



#ifdef __cplusplus
}
#endif // __cplusplus



#endif  /* !__PLATFORM_GPIO_H__ */