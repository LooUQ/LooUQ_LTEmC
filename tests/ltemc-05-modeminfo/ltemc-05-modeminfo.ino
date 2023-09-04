/******************************************************************************
 *  \file LTEmC-5-modemInfo.ino
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
 * Test 5: Query the BGx for basic identity information.
 * 
 * The sketch is designed for debug output to observe results.
 *****************************************************************************/

#define ENABLE_DIAGPRINT                    // expand DIAGPRINT into debug output
//#define ENABLE_DIAGPRINT_VERBOSE            // expand DIAGPRINT and DIAGPRINT_V into debug output
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


// test controls
uint16_t loopCnt = 0;
uint16_t cycle_interval = 2000;
uint32_t lastCycle;
resultCode_t rslt;

modemInfo_t *modemInfo;


void setup() 
{
    #ifdef DIAGPRINT_SERIAL
        Serial.begin(115200);
        delay(5000);                // just give it some time
    #endif
    DPRINT(PRNT_DEFAULT,"\n\n*** ltemc-05-modeminfo started ***\n\n");
    //lqDiag_setNotifyCallback(applEvntNotify);                       // configure ASSERTS to callback into application

    ltem_create(ltem_pinConfig, NULL, appEvntNotify);              // create LTEmC modem, no yield req'd for testing
    ltem_start(resetAction_swReset);                                // ... and start it

    DPRINT(PRNT_WHITE, "LTEmC Ver: %s\r\n", ltem_getSwVersion());
    lastCycle = cycle_interval;
}


void loop() 
{
    if (ELAPSED(lastCycle, cycle_interval))
    {
        lastCycle = millis();
        loopCnt++;

        modemInfo = mdminfo_ltem();
        DPRINT(PRNT_CYAN, "\rModem Information\r\n");
        DPRINT(PRNT_CYAN, "IMEI = %s \r\n", modemInfo->imei);
        DPRINT(PRNT_CYAN, "ICCID = %s \r\n", modemInfo->iccid);
        DPRINT(PRNT_CYAN, "Firmware = %s \r\n", modemInfo->fwver);
        DPRINT(PRNT_CYAN, "Mfg/Model = %s \r\n", modemInfo->model);

        DPRINT(PRNT_INFO, "\rRSSI = %d dBm \r\n",mdminfo_signalRSSI());
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

    uint8_t halt = 1;
    while (halt)
    {
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        pDelay(1000);
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        pDelay(100);
    }
}
