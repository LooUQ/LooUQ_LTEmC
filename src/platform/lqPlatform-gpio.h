/******************************************************************************
 *  \file platform_gpio.h
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
#ifndef __PLATFORM_GPIO_H__
#define __PLATFORM_GPIO_H__

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif // __cplusplus

#define GPIO_NM 9

typedef struct ltemPinConfig_tag
{
    int spiCsPin;
    int irqPin;
    int statusPin;
    int powerkeyPin;
    int resetPin;
    int ringUrcPin;
    int wakePin;
} ltemPinConfig_t;


typedef enum {
    gpioValue_low = 0,
    gpioValue_high = 1
} gpioPinValue_t;


typedef enum {
    gpioMode_input = 0x0,
    gpioMode_output = 0x1,
    gpioMode_inputPullUp,
    gpioMode_inputPullDown
} gpioPinMode_t;


typedef enum {
    gpioIrqTriggerOn_low = 0,
    gpioIrqTriggerOn_high = 1,
    gpioIrqTriggerOn_change =2,
    gpioIrqTriggerOn_falling = 3,
    gpioIrqTriggerOn_rising = 4
} gpioIrqTrigger_t;


typedef uint8_t platformGpioPin;
typedef void(*platformGpioPinIrqCallback)(void);


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


void platform_openPin(uint8_t pinNum, gpioPinMode_t pinMode);
void platform_closePin(uint8_t pinNum);

gpioPinValue_t platform_readPin(uint8_t pinNum);
void platform_writePin(uint8_t pinNum, gpioPinValue_t val);

void platform_attachIsr(uint8_t pinNum, bool enabled, gpioIrqTrigger_t triggerOn, platformGpioPinIrqCallback isrCallback);
void platform_detachIsr(uint8_t pinNum);

/* The functions below are optional
 * They are intended to be used during development to help create your attach\detach ISR functions. */
uint32_t platform_getIntFlags();
uint32_t platform_getPinInterrupt(uint32_t pin);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif  /* !__PLATFORM_GPIO_H__ */