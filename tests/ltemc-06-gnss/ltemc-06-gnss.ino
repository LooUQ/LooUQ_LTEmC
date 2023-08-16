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

#define _DEBUG 1                        // set to non-zero value for PRINTF debugging output, 
// debugging output options             // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if defined(_DEBUG)
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG                  // enable serial port output using devl host platform serial
    #elif _DEBUG == 2 
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #endif
#else
#define PRINTF(c_, f_, ...)
#endif


/* specify the pin configuration 
 * --------------------------------------------------------------------------------------------- */
// #define HOST_FEATHER_UXPLOR             
// #define HOST_FEATHER_LTEM3F
// #define HOST_FEATHER_UXPLOR_L
#define HOST_ESP32_DEVMOD_BMS

#define PERIOD_FROM_SECONDS(period)  (period * 1000)
#define PERIOD_FROM_MINUTES(period)  (period * 1000 * 60)
#define ELAPSED(start, timeout) ((start == 0) ? 0 : millis() - start > timeout)

#include <ltemc.h>
#include <ltemc-gnss.h>

// test controls
uint16_t loopCnt = 0;
uint16_t cycle_interval = 2000;
uint32_t lastCycle;
resultCode_t rslt;

gnssLocation_t location;
uint32_t fixWaitStart;
uint32_t secondsToFix = 0;


void setup() {
    #ifdef SERIAL_DBG
        Serial.begin(115200);
        delay(5000);                // just give it some time
    #endif
    PRINTF(0,"\n\n*** ltemc-05-modeminfo started ***\n\n");
    //lqDiag_setNotifyCallback(applEvntNotify);                     // configure ASSERTS to callback into application

    ltem_create(ltem_pinConfig, NULL, appEvntNotify);               // create LTEmC modem, no yield req'd for testing
    ltem_start(resetAction_swReset);                                // ... and start it

    PRINTF(dbgColor__white, "LTEmC Ver: %s\r\n", ltem_getSwVersion());
    lastCycle = cycle_interval;

    // turn on GNSS
    rslt = gnss_on();
    if (rslt == 200)
        PRINTF(dbgColor__info, "GNSS enabled\r\n", rslt);
    if (rslt == 504)
        PRINTF(dbgColor__warn, "GNSS was already on\r\n", rslt);

    ltem_setRadioPriority(radioPriority_gnss);

    lastCycle = cycle_interval;
    fixWaitStart = pMillis();
}


void loop() 
{
    if (ELAPSED(lastCycle, cycle_interval))
    {
        lastCycle = millis();
        loopCnt++;


        location = gnss_getLocation();

        if (location.statusCode == 200)
        {
            char cLat[14];
            char cLon[14];

            if (secondsToFix == 0)
            {
                secondsToFix = (pMillis() - fixWaitStart) / 1000 + 1;       // if less than 1 second, round up
            }
            PRINTF(dbgColor__none, "Location Information\r\n");
            PRINTF(dbgColor__cyan, "UTC=%s   FixSecs=%d\r\n", location.utc, secondsToFix);

            PRINTF(dbgColor__cyan, "(double) Lat=%4.4f, Lon=%4.4f\r\n", location.lat.val, location.lon.val);
            PRINTF(dbgColor__cyan, "(int4d)  Lat=%d, Lon=%d\r\n", (int32_t)(location.lat.val * 10000.0), (int32_t)(location.lon.val * 10000.0));
        }
        else
            PRINTF(dbgColor__warn, "Location is not available (GNSS not fixed)\r\n");

        PRINTF(0,"\r\nLoop=%d \r\n", loopCnt);
    }
}




/* test helpers
========================================================================================================================= */

void appEvntNotify(appEvent_t eventType, const char *notifyMsg)
{
    if (eventType == appEvent_fault_assertFailed)
    if (eventType == appEvent_fault_assertFailed)
    {
        PRINTF(dbgColor__error, "LTEmC-HardFault: %s\r\n", notifyMsg);
    }
    else 
    {
        PRINTF(dbgColor__white, "LTEmC Info: %s\r\n", notifyMsg);
    }
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

