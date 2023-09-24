// Copyright (c) 2023 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

/* 
* arduinoTiming.cpp provides the translation between LTEm1 platform "C" and Arduino "C++".
* Each platform function has a matching Arduino C++ free-function to wrap arduino functions.
------------------------------------------------------------------------------------------------------------------------- */

#ifdef ARDUINO_ARCH_ESP32

#include "platform-timing.h"
#include <Arduino.h>

platform_yieldCB_func_t platform_yieldCB_func;

uint32_t pMillis()
{
    return millis();
}


void pYield()
{
    if (platform_yieldCB_func)          // allow for device application yield processing
        platform_yieldCB_func();
    else
        vTaskDelay(1);                  // ESP32: give core to next task
}


void pDelay(uint32_t delay_ms)
{
    for (size_t i = 0; i < delay_ms; i++)
    {
        pYield();
        delay(1);
    }
}


bool pElapsed(uint32_t timerStart, uint32_t timerTimeout)
{
    return (timerStart == 0) ? 0 : millis() - timerStart > timerTimeout;
}

#endif