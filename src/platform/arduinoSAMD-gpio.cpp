/** ***************************************************************************
  @file 
  @brief LTEm GPIO abstraction for SAMD (ARM Cortex+) under Arduino framework.

  @author Greg Terrell, LooUQ Incorporated

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

//#define ARDUINO_ARCH_SAMD
#ifdef ARDUINO_ARCH_SAMD

#include "platform-gpio.h"
#include <Arduino.h>


void platform_openPin(uint8_t pNum, uint8_t pMode)
{
    // Arduino sets pinMode to input/output with option pull up/down
    pinMode(pNum, pMode);
}


void platform_closePin(uint8_t pNum)
{
    // Arduino does not require close
}


uint8_t platform_readPin(uint8_t pNum)
{
    return digitalRead(pNum);
}


void platform_writePin(uint8_t pNum, uint8_t val)
{
    digitalWrite(pNum, val);
}


/* The attachIsr function requires that no interrupt be pending for the IOP modules (from the LTEmX SPI\UART chip).
 * failure to assure that constraint will likely result in the LTEmC driver locking up in the IOP interrupt service
 * routine. */
void platform_attachIsr(uint8_t pinNum, bool enabled, uint8_t triggerOn, platformGpioPinIrqCallback isrCallback)
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

#endif