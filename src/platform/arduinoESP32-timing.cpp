// Copyright (c) 2023 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

/* 
* arduinoTiming.cpp provides the translation between LTEm1 platform "C" and Arduino "C++".
* Each platform function has a matching Arduino C++ free-function to wrap arduino functions.
------------------------------------------------------------------------------------------------------------------------- */
#ifdef ARDUINO_ARCH_ESP32

#include "platform-timing.h"
#include <Arduino.h>


uint32_t pMillis()
{
    return millis();
}


static void espYield()              // Since ESP pattern requires a parameter, it does not match typedef, this function wraps it to create a compatible func.
{
    vTaskDelay(1);
}

// local definition of yield function (possible to override)
yield_func g_yieldCB = espYield;   // ESP32 (FreeRTOS): block task (for 1 tick), then yield immediately to an unblocked task

void pYield()
{
    g_yieldCB();                    // if not overridden, perform platform yield processing (ex: Arduino scheduler, ESPx, etc.)
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