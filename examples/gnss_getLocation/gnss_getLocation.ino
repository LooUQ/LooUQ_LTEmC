/******************************************************************************
 *  \file gnss_getLocation.ino
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020-2024 LooUQ Incorporated.
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

#include <lq-embed.h>
#define lqLOG_LEVEL lqLOGLEVEL_DBG
//#define DISABLE_ASSERT                                    // ASSERT/_W enabled by default, can be disabled 
//#define ASSERT_ACTION_STOP                                // ASSERTS can be configured to stop at while(){}


/* specify the pin configuration 
 * --------------------------------------------------------------------------------------------- */
#ifdef ARDUINO_ARCH_ESP32
    #define HOST_ESP32_DEVMOD_BMS
#else
    #define HOST_FEATHER_UXPLOR_L
    // #define HOST_FEATHER_UXPLOR             
    // #define HOST_FEATHER_LTEM3F
#endif

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

    ltem_setRfPriorityMode(ltemRfPriorityMode_gnss);

    lastCycle = cycle_interval;
    fixWaitStart = lqMillis();
}


void loop() 
{
    if (IS_ELAPSED(lastCycle, cycle_interval))
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
                secondsToFix = (lqMillis() - fixWaitStart) / 1000 + 1;       // if less than 1 second, round up
            }
            DPRINT(PRNT_DEFAULT, "Location Information\r\n");
            DPRINT(PRNT_CYAN, "UTC=%s   FixSecs=%d\r\n", location.utc, secondsToFix);

            DPRINT(PRNT_CYAN, "(double) Lat=%4.4f, Lon=%4.4f\r\n", location.lat.val, location.lon.val);
            DPRINT(PRNT_CYAN, "(int4d)  Lat=%d, Lon=%d\r\n", (int32_t)(location.lat.val * 10000.0), (int32_t)(location.lon.val * 10000.0));
        }
        else
            DPRINT(PRNT_WARN, "Location is not available (GNSS not fixed)\r\n");


        // if (memcmp(modemInfo->model, "BG77", 4) == 0 || memcmp(modemInfo->model, "BG95", 4) == 0)
        // {
        //     DPRINT(PRNT_MAGENTA, "\r\n%s module has single RF path, RF priority is controlled by LTEmC\r\n", modemInfo->model);
        //     gnss_on();
        //     if (loopCnt % 2 == 1)
        //         DPRINT(PRNT_MAGENTA, "Setting RF Priority to WWAN (rslt=%d)\r\n", ltem_setRfPriorityMode(ltemRfPriorityMode_wwan));
        //     else
        //         DPRINT(PRNT_MAGENTA, "Setting RF Priority to GNSS (rslt=%d)\r\n", ltem_setRfPriorityMode(ltemRfPriorityMode_gnss));
        // }
        // else
        //     DPRINT(PRNT_MAGENTA, "\r\n%s module is dual RF path, RF priority is not applicable\r\n", modemInfo->model);





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
        lqGpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        lqDelay(1000);
        lqGpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        lqDelay(100);
    }
}

