// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.


#include "lqPlatform-debug.h"
#include <Arduino.h>


void dbg_print(const char *msg, ...)
{
    asm(".global _printf_float");

    //Serial.println("*** preparing print expansion");

    char buf[DBGBUFFER_SZ] = {0};
    va_list args;
    va_start(args, msg);
    vsnprintf(buf, sizeof(buf), msg, args);
    va_end(args);

    //Serial.println("*** print expansion completed");

    // Arduino Serial.print()
    Serial.print(buf);
}

