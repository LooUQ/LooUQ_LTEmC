/******************************************************************************
 *  \file LTEmC-6-gnss.ino
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
 * Test the GNSS/GPS receiver basic functionality (on/off/get location).
 * 
 * The sketch is designed for debug output to observe results.
 *****************************************************************************/

#define _DEBUG 2                        // set to non-zero value for PRINTF debugging output, 
// debugging output options             // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if _DEBUG > 0
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG == 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #define PRINTF(c_,f_,__VA_ARGS__...) do { rtt_printf(c_, (f_), ## __VA_ARGS__); } while(0)
    #endif
#else
#define PRINTF(c_, f_, ...) 
#endif

/* specify the pin configuration
 * --------------------------------------------------------------------------------------------- */
// #define HOST_FEATHER_UXPLOR             
#define HOST_FEATHER_LTEM3F

#include <ltemc.h>
#include <lq-diagnostics.h>
#include <ltemc-gnss.h>


int loopCnt = 0;
gnssLocation_t location;
uint32_t fixWaitStart;
uint32_t secondsToFix = 0;


void setup() {
    #ifdef SERIAL_OPT
        Serial.begin(115200);
        #if (SERIAL_OPT > 0)
        while (!Serial) {}      // force wait for serial ready
        #else
        delay(5000);            // just give it some time
        #endif
    #endif

    PRINTF(dbgColor__red, "\rLTEmC Test 6: GNSS\r");
    randomSeed(analogRead(0));
    lqDiag_registerEventCallback(appNotifyCB);                      // configure ASSERTS to callback into application

    ltem_create(ltem_pinConfig, NULL, appNotifyCB);                 // create LTEmC modem, no yield CB req'd for testing
    ltem_start((resetAction_t)skipResetIfRunning);                  // ... and start it

    // turn on GNSS
    resultCode_t cmdResult = gnss_on();

    if (cmdResult == 200)
        PRINTF(dbgColor__info, "GNSS enabled\r", cmdResult);
    if (cmdResult == 504)
        PRINTF(dbgColor__warn, "GNSS was already on\r", cmdResult);

    
    fixWaitStart = pMillis();
}


void loop() {

    location = gnss_getLocation();

    if (location.statusCode == 200)
    {
        char cLat[14];
        char cLon[14];

        if (secondsToFix == 0)
            secondsToFix = (pMillis() - fixWaitStart) / 1000 + 1;       // if less than 1 second, round up

        PRINTF(dbgColor__none, "Location Information\r");
        PRINTF(dbgColor__cyan, "(double) Lat=%4.4f, Lon=%4.4f  FixSecs=%d\r", location.lat.val, location.lon.val, secondsToFix);
        PRINTF(dbgColor__cyan, "(int4d)  Lat=%d, Lon=%d  FixSecs=%d\r", (int32_t)(location.lat.val * 10000.0), (int32_t)(location.lon.val * 10000.0), secondsToFix);
    }
    else
        PRINTF(dbgColor__warn, "Location is not available (GNSS not fixed)\r");

    loopCnt ++;
    indicateLoop(loopCnt, random(1000));
}




/* test helpers
========================================================================================================================= */

void appNotifyCB(uint8_t notifType, const char *notifMsg)
{
    if (notifType > 200)
    {
        PRINTF(dbgColor__error, "LQCloud-HardFault: %s\r", notifMsg);
        while (1) {}
    }
    PRINTF(dbgColor__info, "LQCloud Info: %s\r", notifMsg);
    return;
}


void indicateFailure(char failureMsg[])
{
	PRINTF(dbgColor__error, "\r\n** %s \r\n", failureMsg);
    PRINTF(dbgColor__error, "** Test Assertion Failed. \r\n");

    bool halt = true;
    while (halt)
    {
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        pDelay(1000);
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        pDelay(100);
    }
}


void indicateLoop(int loopCnt, int waitNext) 
{
    PRINTF(dbgColor__info, "\r\nLoop=%i \r\n", loopCnt);

    for (int i = 0; i < 6; i++)
    {
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        pDelay(50);
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        pDelay(50);
    }

    PRINTF(dbgColor__dMagenta, "FreeMem=%u\r\n", getFreeMemory());
    PRINTF(dbgColor__dMagenta, "NextTest (millis)=%i\r\r", waitNext);
    pDelay(waitNext);
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

