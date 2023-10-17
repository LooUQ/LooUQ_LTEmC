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

#define ENABLE_DIAGPRINT                    // expand DPRINT into debug output
//#define ENABLE_DIAGPRINT_VERBOSE            // expand DPRINT and DPRINT_V into debug output
#define ENABLE_ASSERT
#include <lqdiag.h>


/* specify the pin configuration 
 * --------------------------------------------------------------------------------------------- */
#ifdef ARDUINO_ARCH_ESP32
    #define HOST_ESP32_DEVMOD_BMS
#else
    #define HOST_FEATHER_UXPLOR_L
    // #define HOST_FEATHER_UXPLOR             
    // #define HOST_FEATHER_LTEM3F
#endif

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
    #ifdef DIAGPRINT_SERIAL
        Serial.begin(115200);
        delay(5000);                // just give it some time
    #endif
    DPRINT(PRNT_DEFAULT,"\n\n*** ltemc-05-gnss started ***\n\n");
    //lqDiag_setNotifyCallback(applEvntNotify);                     // configure ASSERTS to callback into application

    ltem_create(ltem_pinConfig, NULL, appEvntNotify);               // create LTEmC modem, no yield req'd for testing
    ltem_start(resetAction_swReset);                                // ... and start it

    DPRINT(PRNT_WHITE, "LTEmC Ver: %s\r\n", ltem_getSwVersion());
    lastCycle = cycle_interval;

    // turn on GNSS
    rslt = gnss_on();
    if (rslt == 200)
        DPRINT(PRNT_INFO, "GNSS enabled\r\n", rslt);
    if (rslt == 504)
        DPRINT(PRNT_WARN, "GNSS was already on\r\n", rslt);

    char tkBffr[8];
    uint32_t rfSwStart = pMillis();

    ltem_setRfPriority(ltemRfPrioritySet_gnss);
    DPRINT(PRNT_CYAN, "RF priority switch, elapsed=%d\r\n", pMillis() - rfSwStart);

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
            DPRINT(PRNT_DEFAULT, "Location Information\r\n");
            DPRINT(PRNT_CYAN, "UTC=%s   FixSecs=%d\r\n", location.utc, secondsToFix);

            DPRINT(PRNT_CYAN, "(double) Lat=%4.4f, Lon=%4.4f\r\n", location.lat.val, location.lon.val);
            DPRINT(PRNT_CYAN, "(int4d)  Lat=%d, Lon=%d\r\n", (int32_t)(location.lat.val * 10000.0), (int32_t)(location.lon.val * 10000.0));
        }
        else
            DPRINT(PRNT_WARN, "Location is not available (GNSS not fixed)\r\n");

        DPRINT(0,"\r\nLoop=%d \r\n", loopCnt);
    }
}




/* test helpers
========================================================================================================================= */

void appEvntNotify(appEvent_t eventType, const char *notifyMsg)
{
    if (eventType == appEvent_fault_assertFailed)
    if (eventType == appEvent_fault_assertFailed)
    {
        DPRINT(PRNT_ERROR, "LTEmC-HardFault: %s\r\n", notifyMsg);
    }
    else 
    {
        DPRINT(PRNT_WHITE, "LTEmC Info: %s\r\n", notifyMsg);
    }
    return;
}


void indicateFailure(char failureMsg[])
{
	DPRINT(PRNT_ERROR, "\r\n** %s \r\n", failureMsg);
    DPRINT(PRNT_ERROR, "** Test Assertion Failed. \r\n");

    bool halt = true;
    while (halt)
    {
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        pDelay(1000);
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        pDelay(100);
    }
}

