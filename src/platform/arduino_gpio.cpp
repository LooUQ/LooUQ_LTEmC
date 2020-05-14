// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

/* 
* arduinoGpio.cpp provides the translation between LTEm1 platform "C" and Arduino "C++".
* Each platform function has a matching Arduino C++ free-function to wrap arduino functions.
------------------------------------------------------------------------------------------------------------------------- */

#include "platform_gpio.h"
#include <Arduino.h>


void gpio_openPin(uint8_t pNum, gpioPinMode_t pMode)
{
    // Arduino sets pinMode to input/output with option pull up/down
    pinMode(pNum, pMode);
}


void gpio_closePin(uint8_t pNum)
{
    // Arduino does not require close
}


gpioPinValue_t gpio_readPin(uint8_t pNum)
{
    return (gpioPinValue_t)digitalRead(pNum);
}


void gpio_writePin(uint8_t pNum, gpioPinValue_t val)
{
    digitalWrite(pNum, val);
}


void gpio_attachIsr(uint8_t pinNum, bool enabled, gpioIrqTrigger_t triggerOn, platformGpioPinIrqCallback isrCallback)
{
    attachInterrupt(digitalPinToInterrupt(pinNum), isrCallback, triggerOn);
}


void gpio_detachIsr(uint8_t pinNum)
{
    detachInterrupt(digitalPinToInterrupt(pinNum));
}

