// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

/* 
* arduinoGpio.cpp provides the translation between LTEm1 platform "C" and Arduino "C++".
* Each platform function has a matching Arduino C++ free-function to wrap arduino functions.
------------------------------------------------------------------------------------------------------------------------- */

#include "platformGpio.h"
#include <Arduino.h>


void gpio_openPin(uint8_t pNum, gpio_pinMode_t pMode)
{
    // Arduino sets pinMode to input/output with option pull up/down
    pinMode(pNum, pMode);
}


void gpio_closePin(uint8_t pNum)
{
    // Arduino does not require close
}


gpio_pinValue_t gpio_readPin(uint8_t pNum)
{
    return (gpio_pinValue_t)digitalRead(pNum);
}


void gpio_writePin(uint8_t pNum, gpio_pinValue_t val)
{
    digitalWrite(pNum, val);
}

// void (*isr)(void)
//void gpio_irqAttach(uint8_t pinNum, bool enabled, gpio_irqTrigger_t triggerOn, platformGpioPinIrqCallback isrCallback);
void gpio_attachIsr(uint8_t pinNum, bool enabled, gpio_irqTrigger_t triggerOn, platformGpioPinIrqCallback isrCallback)
{
}


void gpio_detachIsr(uint8_t pinNum)
{
}

