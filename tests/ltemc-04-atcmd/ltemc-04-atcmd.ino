
#define ENABLE_DIAGPRINT                    // expand DIAGPRINT into debug output
//#define ENABLE_DIAGPRINT_VERBOSE            // expand DIAGPRINT and DIAGPRINT_V into debug output
#define ENABLE_ASSERT
//#include <lqdiag.h>

/* specify the pin configuration 
 * --------------------------------------------------------------------------------------------- */
#ifdef ARDUINO_ARCH_ESP32
    #define HOST_ESP32_DEVMOD_BMS
#else
    #define HOST_FEATHER_UXPLOR_L
    // #define HOST_FEATHER_UXPLOR             
    // #define HOST_FEATHER_LTEM3F
#endif

// #define PERIOD_FROM_SECONDS(period)  (period * 1000)
// #define PERIOD_FROM_MINUTES(period)  (period * 1000 * 60)
// #define ELAPSED(start, timeout) ((start == 0) ? 0 : millis() - start > timeout)


#include <ltemc-internal.h>


// LTEmC Includes
#include <ltemc.h>


// test controls
bBuffer_t* rxBffrPtr;

uint16_t loopCnt = 0;
uint16_t cycle_interval = 2000;
uint32_t lastCycle;
uint16_t errorCnt = 0;
resultCode_t rslt;


void setup() 
{
    #ifdef DIAGPRINT_SERIAL
        Serial.begin(115200);
        delay(5000);                // just give it some time
    #endif
    DPRINT(PRNT_RED,"\n\n*** ltemc-04-atcmd started ***\n\n");
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
        DPRINT(PRNT_DEFAULT, "Invoking cmd: %s \r\n", cmdStr);

        if (atcmd_tryInvoke(cmdStr))
        {
            resultCode_t atResult = atcmd_awaitResult();
            
                char *response = atcmd_getRawResponse();
                DPRINT(PRNT_INFO, "Got %d chars\r", strlen(response));
                DPRINT(PRNT_WHITE, "Resp:");
                DPRINT(PRNT_CYAN, "%s\r", response);
                                                                                                // test response v. expected 
                char *validResponse = "\r\nQuectel\r\nBG";                                      // near beginning (depends on BGx echo)
                if (!strstr(response, validResponse))
                    indicateFailure("Expected cmd response missing... failed."); 

            if (atResult != resultCode__success)                                                // statusCode == 200 (similar to HTTP codes)
            {
                DPRINT(PRNT_ERROR, "atResult=%d \r", atResult);
                // indicateFailure("Unexpected command response... failed."); 
                indicateFailure("Invalid BGx response");
            }

            /* atcmd_close();       Not needed here since tryInvokeDefaults(). 
                                    With options and manual lock, required when done with response to close action and release action lock */
        }
        else
            DPRINT(PRNT_WARN, "Unable to get action lock.\r");


        DPRINT(0,"Loop=%d \n\n", loopCnt);
     }
}


/* test helpers
========================================================================================================================= */

void appEvntNotify(appEvent_t eventType, const char *notifyMsg)
{
    if (eventType == appEvent_fault_assertFailed)
        DPRINT(PRNT_ERROR, "LTEmC-HardFault: %s\r", notifyMsg);
    else 
        DPRINT(PRNT_WHITE, "LTEmC Info: %s\r", notifyMsg);
    return;
}


void indicateFailure(char failureMsg[])
{
	DPRINT(PRNT_ERROR, "\r\n** %s \r", failureMsg);
    DPRINT(PRNT_ERROR, "** Test Assertion Failed. \r");

    #if 1
    DPRINT(PRNT_ERROR, "** Halting Execution \r\n");
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
    //         DPRINT(PRNT_INFO, "AppRdy recv'd\r\n");
    //     }
    // }
    // else
    //     DPRINT(PRNT_dYELLOW, "AppRdy assumed\r\n");
    // SC16IS7xx_enableIrqMode();
    // IOP_attachIrq();
}
