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

#include <Arduino.h>

#include <ltemc.h>
#include <ltemc-gnss.h>

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
    DPRINT(PRNT_RED,"\n\n*** ltemc-05-modeminfo started ***\n\n");
    //lqDiag_setNotifyCallback(applEvntNotify);                       // configure ASSERTS to callback into application

    ltem_create(ltem_pinConfig, NULL, appEvntNotify);              // create LTEmC modem, no yield req'd for testing
    ltem_start(resetAction_swReset);                                // ... and start it

    DPRINT(PRNT_WHITE, "LTEmC Ver: %s\r\n", ltem_getSwVersion());
    lastCycle = cycle_interval;
}


void loop() 
{
    ntwkOperator_t * ntwk;

    if (ELAPSED(lastCycle, cycle_interval))
    {
        lastCycle = millis();
        loopCnt++;

        modemInfo = ltem_getModemInfo();
        DPRINT(PRNT_CYAN, "\rModem Information\r\n-------------------------\r\n");
        DPRINT(PRNT_CYAN, "IMEI = %s \r\n", modemInfo->imei);
        DPRINT(PRNT_CYAN, "ICCID = %s \r\n", modemInfo->iccid);
        DPRINT(PRNT_CYAN, "Firmware = %s \r\n", modemInfo->fwver);
        DPRINT(PRNT_CYAN, "Manufacturer = %s \r\n", modemInfo->mfg);
        DPRINT(PRNT_CYAN, "Model = %s \r\n", modemInfo->model);

        DPRINT(PRNT_GREEN, "\r\nNetwork Information\r\n-------------------------\r\n");
        ntwk = ntwk_awaitOperator(0);
        DPRINT(PRNT_GREEN, "Operator = %s \r\n", ntwk->name);
        DPRINT(PRNT_GREEN, "IoT Mode = %s \r\n", ntwk->iotMode);
        DPRINT(PRNT_GREEN, "\r\nPacket Ntwk Count = %d \r\n", ntwk->pdpCntxtCnt);
        if (ntwk->pdpCntxtCnt > 0)
        {
            DPRINT(PRNT_GREEN, "-- Network #1\r\n");
            DPRINT(PRNT_GREEN, "      CntxtID = %d \r\n", ntwk->packetNetworks[0].pdpContextId);
            DPRINT(PRNT_GREEN, "     Protocol = %s (%d)\r\n", ntwk->packetNetworks[0].protoName, ntwk->packetNetworks[0].pdpProtocol);
            DPRINT(PRNT_GREEN, "      IP Addr = %s \r\n", ntwk->packetNetworks[0].ipAddress);
        }
        DPRINT(PRNT_dGREEN, "\r\nNetwork ready state=%d\r\n", ntwk_isReady(true));

        DPRINT(PRNT_INFO, "\r\n  Ntwk Info = %s \r\n", ntwk_getNetworkInfo());
        DPRINT(PRNT_INFO, " Raw Signal = %d \r\n", ltem_signalRaw());
        DPRINT(PRNT_INFO, "Signal RSSI = %d dBm \r\n", ltem_signalRSSI());

        if (memcmp(modemInfo->model, "BG77", 4) == 0 || memcmp(modemInfo->model, "BG95", 4) == 0)
        {
            DPRINT(PRNT_MAGENTA, "\r\n%s module has single RF path, RF priority is controlled by LTEmC\r\n", modemInfo->model);
            gnss_on();
            if (loopCnt % 2 == 1)
                DPRINT(PRNT_MAGENTA, "Setting RF Priority to WWAN (rslt=%d)\r\n", ltem_setRfPriorityMode(ltemRfPriorityMode_wwan));
            else
                DPRINT(PRNT_MAGENTA, "Setting RF Priority to GNSS (rslt=%d)\r\n", ltem_setRfPriorityMode(ltemRfPriorityMode_gnss));
        }
        else
            DPRINT(PRNT_MAGENTA, "\r\n%s module is dual RF path, RF priority is not applicable\r\n", modemInfo->model);


        DPRINT(0,"\r\nLoop=%d \r\n==================================================\r\n", loopCnt);
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
