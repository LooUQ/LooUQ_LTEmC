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
#if defined(_DEBUG) && _DEBUG > 0
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG == 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#endif


// define options for how to assemble this build
#define HOST_FEATHER_UXPLOR             // specify the pin configuration

#include <ltemc.h>


void setup() {
    #ifdef SERIAL_OPT
        Serial.begin(115200);
        #if (SERIAL_OPT > 0)
        while (!Serial) {}      // force wait for serial ready
        #else
        delay(5000);            // just give it some time
        #endif
    #endif

    PRINTF(DBGCOLOR_white, "\rLTEm1c Test6: gnss\r");
    gpio_openPin(LED_BUILTIN, gpioMode_output);
    
    randomSeed(analogRead(0));

    ltem_create(ltem_pinConfig, appNotifyCB);
    ltem_start(pdpProtocol_none);

    // turn on GNSS
    resultCode_t cmdResult = gnss_on();
    PRINTF(DBGCOLOR_info, "GNSS On result=%d (504 is already on)\r", cmdResult);
}


int loopCnt = 0;
gnssLocation_t location;

void loop() {

    location = gnss_getLocation();

    if (location.statusCode == 200)
    {
        char cLat[14];
        char cLon[14];
        
        PRINTF(DBGCOLOR_none, "Location Information\r");
        PRINTF(DBGCOLOR_cyan, "Lat=%4.4f, Lon=%4.4f \r", location.lat.val, location.lon.val);
    }
    else
        PRINTF(DBGCOLOR_warn, "Location is not available (GNSS not fixed)\r");

    loopCnt ++;
    indicateLoop(loopCnt, random(1000));
}




/* test helpers
========================================================================================================================= */

void appNotifyCB(uint8_t notifType, const char *notifMsg)
{
    if (notifType > 200)
    {
        PRINTF(DBGCOLOR_error, "LQCloud-HardFault: %s\r", notifMsg);
        while (1) {}
    }
    PRINTF(DBGCOLOR_info, "LQCloud Info: %s\r", notifMsg);
    return;
}


void indicateFailure(char failureMsg[])
{
	PRINTF(DBGCOLOR_error, "\r\n** %s \r\n", failureMsg);
    PRINTF(DBGCOLOR_error, "** Test Assertion Failed. \r\n");

    bool halt = true;
    while (halt)
    {
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        lDelay(1000);
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        lDelay(100);
    }
}


void indicateLoop(int loopCnt, int waitNext) 
{
    PRINTF(DBGCOLOR_info, "\r\nLoop=%i \r\n", loopCnt);

    for (int i = 0; i < 6; i++)
    {
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        lDelay(50);
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        lDelay(50);
    }

    PRINTF(DBGCOLOR_dMagenta, "FreeMem=%u\r\n", getFreeMemory());
    PRINTF(DBGCOLOR_dMagenta, "NextTest (millis)=%i\r\r", waitNext);
    lDelay(waitNext);
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

