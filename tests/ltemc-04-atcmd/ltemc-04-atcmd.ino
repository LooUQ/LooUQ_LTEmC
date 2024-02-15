/** ***************************************************************************
  @file ltemc-04-atcmd.ino
  @brief ATCMD processor example for LooUQ LTEm series modems with LTEmC.

  @author Greg Terrell, LooUQ Incorporated

  \loouq
-------------------------------------------------------------------------------

LooUQ-LTEmC // Example/test application for using LooUQ LTEm series cellular modems.
Copyright (C) 2017-2023 LooUQ Incorporated

Copyright <YEAR> <COPYRIGHT HOLDER>

Permission is hereby granted, free of charge, to any person obtaining a copy of  
this software and associated documentation files (the “Software”), to deal in the 
Software without restriction, including without limitation the rights to use, copy, 
modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, 
and to permit persons to whom the Software is furnished to do so, subject to the 
following conditions:

The above copyright notice and this permission notice shall be included in all copies 
or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT 
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF 
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE 
OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************** */

#include <lq-embed.h>
#define lqLOG_LEVEL lqLOGLEVEL_DBG
//#define DISABLE_ASSERTS                                   // ASSERT/ASSERT_W enabled by default, can be disabled 
//#define LQ_SRCFILE "INO"                                    // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT


/* specify the pin configuration 
 * --------------------------------------------------------------------------------------------- */
#ifdef ARDUINO_ARCH_ESP32
    #define HOST_ESP32_DEVMOD_BMS
#else
    #define HOST_FEATHER_UXPLOR_L
    // #define HOST_FEATHER_UXPLOR             
    // #define HOST_FEATHER_LTEM3F
#endif

// LTEmC Includes
#include <ltemc.h>
#include <ltemc-internal.h>

// test controls
bBuffer_t* rxBffrPtr;

uint16_t loopCnt = 0;
uint16_t cycle_interval = 2000;
uint32_t lastCycle;
uint16_t errorCnt = 0;
resultCode_t rslt;


void setup() 
{
    #if defined(lqLOG_SERIAL)
        Serial.begin(115200);
        delay(5000);                                            // just give it some time
    #endif

    lqDelay(10);
    lqLOG_NOTICE("\n\n*** ltemc-04-atcmd started ***\n\n");
    randomSeed(analogRead(0));

    // lqDiag_setNotifyCallback(appEvntNotify);                 // configure LTEMC ASSERTS to callback into application

    ltem_create(ltem_pinConfig, NULL, appEvntNotify);           // create LTEmC modem
    ltem_start(resetAction_swReset);                            // ... and start it

    lastCycle = cycle_interval;
}


void loop() 
{
    if (ELAPSED(lastCycle, cycle_interval))
    {
        lastCycle = millis();
        loopCnt++;

        /* BG96 test pattern: get mfg\model
        *
        *  ATI
        * 
        *   Quectel
        *   BG96
        *   Revision: BG96MAR04A02M1G
        * 
        *   OK
        */

        uint8_t regValue = 0;
        char cmdStr[] = "ATI";
        // char cmdStr[] = "AT+CPIN?";
        // char cmdStr[] = "AT+QCCID";
        lqLOG_DBG(lqcDEFAULT, "Invoking cmd: %s \r\n", cmdStr);

        resultCode_t rslt = atcmd_dispatch(cmdStr);
        if (IS_SUCCESS(rslt))
        {
            char *response = atcmd_getRawResponse();
            lqLOG_DBG(lqcINFO, "Got %d chars\r", strlen(response));
            lqLOG_DBG(lqcWHITE, "Resp:");
            lqLOG_DBG(lqcCYAN, "%s\r", response);
                                                                                            // test response v. expected 
            char *validResponse = "\r\nQuectel\r\nBG";                                      // near beginning (depends on BGx echo)
            if (!strstr(response, validResponse))
                indicateFailure("Expected cmd response missing... failed."); 
        }
        else
        {
            lqLOG_DBG(lqcERROR, "atResult=%d \r", rslt);
            indicateFailure("Invalid BGx response");
        }
        lqLOG_INFO("Loop=%d \n\n", loopCnt);
     }
}


/* test helpers
========================================================================================================================= */

void appEvntNotify(appEvent_t eventType, const char *notifyMsg)
{
    if (eventType == appEvent_fault_assertFailed)
        lqLOG_DBG(lqcERROR, "LTEmC-HardFault: %s\r", notifyMsg);
    else 
        lqLOG_DBG(lqcWHITE, "LTEmC Info: %s\r", notifyMsg);
    return;
}


void indicateFailure(char failureMsg[])
{
	lqLOG_DBG(lqcERROR, "\r\n** %s \r", failureMsg);
    lqLOG_DBG(lqcERROR, "** Test Assertion Failed. \r");

    #if 1
    lqLOG_DBG(lqcERROR, "** Halting Execution \r\n");
    bool halt = true;
    while (halt)
    {
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        pDelay(1000);
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        pDelay(100);
    }
    #endif
}


void startLTEm()
{
    // // initialize the HOST side of the LTEm interface
	// // ensure pin is in default "logical" state prior to opening
	// platform_writePin(g_lqLTEM.pinConfig.powerkeyPin, gpioValue_low);
	// platform_writePin(g_lqLTEM.pinConfig.resetPin, gpioValue_low);
	// platform_writePin(g_lqLTEM.pinConfig.spiCsPin, gpioValue_high);

	// platform_openPin(g_lqLTEM.pinConfig.powerkeyPin, gpioMode_output);		// powerKey: normal low
	// platform_openPin(g_lqLTEM.pinConfig.resetPin, gpioMode_output);			// resetPin: normal low
	// platform_openPin(g_lqLTEM.pinConfig.spiCsPin, gpioMode_output);			// spiCsPin: invert, normal gpioValue_high

	// platform_openPin(g_lqLTEM.pinConfig.statusPin, gpioMode_input);
	// platform_openPin(g_lqLTEM.pinConfig.irqPin, gpioMode_inputPullUp);

    // spi_start(g_lqLTEM.platformSpi);

    // QBG_reset(resetAction_powerReset);                                      // force power cycle here, limited initial state conditioning
    // SC16IS7xx_start();                                                      // start (resets previously powered on) NXP SPI-UART bridge

    // if (g_lqLTEM.deviceState != deviceState_appReady)
    // {
    //     if (IOP_awaitAppReady())
    //     {
    //         lqLOG_DBG(lqINFO, "AppRdy recv'd\r\n");
    //     }
    // }
    // else
    //     lqLOG_DBG(lqDARKYELLOW, "AppRdy assumed\r\n");
    // SC16IS7xx_enableIrqMode();
    // IOP_attachIrq();
}
