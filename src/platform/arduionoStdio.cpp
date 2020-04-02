
#include "platformStdio.h"
#include <Arduino.h>


void _dbg_printf(const char *fmt, ...)
{
    char buf[120];
    va_list args;
    va_start(args, fmt);

    //Serial.printf(fmt, args);
    vsnprintf(buf, sizeof(buf), fmt, args);
    Serial.write(buf);

    va_end(args);
}
