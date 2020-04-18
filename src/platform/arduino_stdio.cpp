
#include "platform_stdio.h"
#include <Arduino.h>


#ifndef __SEGGER_RTT

void _dbg_printf(print_color_t color, const char *fmt, ...)
{
    char buf[120];
    va_list args;
    va_start(args, fmt);

    //Serial.printf(fmt, args);
    vsnprintf(buf, sizeof(buf), fmt, args);
    Serial.write(buf);

    va_end(args);
}

#endif