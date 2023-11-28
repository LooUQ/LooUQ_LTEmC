/** ***************************************************************************
  @file 
  @brief LTEm GPIO abstraction declarations.

  @author Greg Terrell/Jensen Miller, LooUQ Incorporated

  \loouq

  @warning Internal dependencies, changes only as directed by LooUQ staff.

-------------------------------------------------------------------------------

LooUQ-LTEmC // Software driver for the LooUQ LTEm series cellular modems.
Copyright (C) 2017-2023 LooUQ Incorporated

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
Also add information on how to contact you by electronic and paper mail.

**************************************************************************** */


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