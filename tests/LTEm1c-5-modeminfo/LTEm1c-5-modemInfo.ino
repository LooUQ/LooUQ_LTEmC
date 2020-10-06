/******************************************************************************
 *  \file LTEm1c-5-modemInfo.ino
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED
 * "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
 * LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 ******************************************************************************
 * Test a collection of canned cmds that return modem information.
 *****************************************************************************/
#define HOST_FEATHER_UXPLOR
#include <platform_pins.h>
#include <ltem1c.h>

#define _DEBUG
#include "dbgprint.h"
//#define USE_SERIAL 0

const int APIN_RANDOMSEED = 0;

void setup() {
    #ifdef USE_SERIAL
        Serial.begin(115200);
        #if (USE_SERIAL)
        while (!Serial) {}
        #else
        delay(1000);
        #endif
    #endif

    PRINTF(dbgColor_none, "LTEm1c test5-modemInfo\r\n");
    gpio_openPin(LED_BUILTIN, gpioMode_output);
    
    randomSeed(analogRead(APIN_RANDOMSEED));

    ltem1_create(ltem1_pinConfig, ltem1Start_powerOn, ltem1Functionality_actions);
}


int loopCnt = 0;
modemInfo_t modemInfo;


void loop() {
    modemInfo = mdminfo_ltem1();

    PRINTF(dbgColor_cyan, "\rModem Information\r");
    PRINTF(dbgColor_cyan, "IMEI = %s \r", modemInfo.imei);
    PRINTF(dbgColor_cyan, "ICCID = %s \r", modemInfo.iccid);
    PRINTF(dbgColor_cyan, "Firmware = %s \r", modemInfo.fwver);
    PRINTF(dbgColor_cyan, "Mfg/Model = %s \r", modemInfo.mfgmodel);

    PRINTF(dbgColor_info, "\rRSSI = %d dBm \r",mdminfo_rssi());

    loopCnt ++;
    indicateLoop(loopCnt, random(1000));
}



/* test helpers
========================================================================================================================= */


void indicateLoop(int loopCnt, int waitNext) 
{
    PRINTF(dbgColor_magenta, "\r\nLoop=%i \r\n", loopCnt);

    for (int i = 0; i < 6; i++)
    {
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        timing_delay(50);
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        timing_delay(50);
    }

    PRINTF(dbgColor_gray, "FreeMem=%u\r\n", getFreeMemory());
    PRINTF(dbgColor_gray, "NextTest (millis)=%i\r\r", waitNext);
    timing_delay(waitNext);
}


void indicateFailure(char failureMsg[])
{
	PRINTF(dbgColor_error, "\r\n** %s \r\n", failureMsg);
    PRINTF(dbgColor_error, "** Test Assertion Failed. \r\n");

    uint8_t halt = 1;
    while (halt)
    {
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        timing_delay(1000);
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        timing_delay(100);
    }
}


/* Check free memory (stack-heap) 
 * - Remove if not needed for production
--------------------------------------------------------------------------------- */

#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__

int getFreeMemory() 
{
    char top;
    #ifdef __arm__
    return &top - reinterpret_cast<char*>(sbrk(0));
    #elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
    return &top - __brkval;
    #else  // __arm__
    return __brkval ? &top - __brkval : &top - __malloc_heap_start;
    #endif  // __arm__
}

