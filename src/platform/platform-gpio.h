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

// #ifdef __cplusplus
// #include <cstdint>
// #else
// #endif // __cplusplus

#include <stdint.h>
#include <stdbool.h>

//#define GPIO_NM 9

#ifdef ARDUINO

#include <Arduino.h>

typedef enum {
    gpioValue_low = LOW,
    gpioValue_high = HIGH
} gpioPinValue_t;

typedef enum {
    gpioMode_input = INPUT,
    gpioMode_output = OUTPUT,
    gpioMode_inputPullUp = INPUT_PULLUP,
    gpioMode_inputPullDown = INPUT_PULLDOWN
} gpioPinMode_t;

typedef enum {
    gpioIrqTriggerOn_low = LOW,
    gpioIrqTriggerOn_high = HIGH,
    gpioIrqTriggerOn_change = CHANGE,
    gpioIrqTriggerOn_falling = FALLING,
    gpioIrqTriggerOn_rising = RISING
} gpioIrqTrigger_t;

#endif 

// typedef uint8_t platformGpioPin;
typedef void(*platformGpioPinIrqCallback)(void);


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


void platform_openPin(uint8_t pinNum, uint8_t pinMode);
void platform_closePin(uint8_t pinNum);

uint8_t platform_readPin(uint8_t pinNum);
void platform_writePin(uint8_t pinNum, uint8_t val);

void platform_attachIsr(uint8_t pinNum, bool enabled, uint8_t triggerMode, platformGpioPinIrqCallback isrCallback);
void platform_detachIsr(uint8_t pinNum);

/* The functions below are optional
 * They are intended to be used during development to help create your attach\detach ISR functions. */
uint32_t platform_getIntFlags();
uint32_t platform_getPinInterrupt(uint32_t pin);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif  /* !__PLATFORM_GPIO_H__ */