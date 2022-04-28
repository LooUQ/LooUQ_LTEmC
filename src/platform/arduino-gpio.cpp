// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

/* 
* arduinoGpio.cpp provides the translation between LTEm1 platform "C" and Arduino "C++".
* Each platform function has a matching Arduino C++ free-function to wrap arduino functions.
------------------------------------------------------------------------------------------------------------------------- */

#include "lqPlatform-gpio.h"
#include <Arduino.h>


void platform_openPin(uint8_t pNum, gpioPinMode_t pMode)
{
    // Arduino sets pinMode to input/output with option pull up/down
    pinMode(pNum, pMode);
}


void platform_closePin(uint8_t pNum)
{
    // Arduino does not require close
}


gpioPinValue_t platform_readPin(uint8_t pNum)
{
    return (gpioPinValue_t)digitalRead(pNum);
}


void platform_writePin(uint8_t pNum, gpioPinValue_t val)
{
    digitalWrite(pNum, val);
}


/* The attachIsr function requires that no interrupt be pending for the IOP modules (from the LTEmX SPI\UART chip).
 * failure to assure that constraint will likely result in the LTEmC driver locking up in the IOP interrupt service
 * routine. */
void platform_attachIsr(uint8_t pinNum, bool enabled, gpioIrqTrigger_t triggerOn, platformGpioPinIrqCallback isrCallback)
{
    EIC->INTFLAG.reg = 0x01 << g_APinDescription[pinNum].ulExtInt;
    attachInterrupt(digitalPinToInterrupt(pinNum), isrCallback, triggerOn);
}


uint32_t platform_getIntFlags()
{
    return EIC->INTFLAG.reg;
}


uint32_t platform_getPinInterrupt(uint32_t pin)
{
    return g_APinDescription[pin].ulExtInt;
}


void platform_detachIsr(uint8_t pinNum)
{
    detachInterrupt(digitalPinToInterrupt(pinNum));
}

