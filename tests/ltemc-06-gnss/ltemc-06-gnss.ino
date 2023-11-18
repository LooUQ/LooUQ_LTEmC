/** ***************************************************************************
  @file 
  @brief LTEm example/test for GNSS location services.

  @author Greg Terrell, LooUQ Incorporated

  \loouq
-------------------------------------------------------------------------------

LooUQ-LTEmC // Software driver for the LooUQ LTEm series cellular modems.
Copyright (C) 2017-2023 LooUQ Incorporated

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
Also add information on how to contact you by electronic and paper mail.

**************************************************************************** */


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

    ltem_setRfPriorityMode(ltemRfPriorityMode_gnss);

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

