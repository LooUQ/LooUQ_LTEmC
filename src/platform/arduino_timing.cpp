// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

/* 
* arduinoTiming.cpp provides the translation between LTEm1 platform "C" and Arduino "C++".
* Each platform function has a matching Arduino C++ free-function to wrap arduino functions.
------------------------------------------------------------------------------------------------------------------------- */

#include "platform_timing.h"
#include <Arduino.h>


void timing_delay(uint32_t intervalMillis)
{
    uint32_t start = millis();
    yield();
    while (millis() - start < intervalMillis) {}
}


uint32_t timing_millis()
{
    return millis();
}


void timing_yield()
{
    yield();
}