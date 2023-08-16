
#define ENABLE_DIAGPRINT                    // expand DIAGPRINT into debug output
#define ENABLE_DIAGPRINT_VERBOSE            // expand DIAGPRINT and DIAGPRINT_V into debug output
#define ENABLE_ASSERT
//#include <jlinkRtt.h>                     // Use J-Link RTT channel for debug output (not platform serial)
#include <lqdiag.h>

/* specify the pin configuration 
 * --------------------------------------------------------------------------------------------- */
// #define HOST_FEATHER_UXPLOR             
// #define HOST_FEATHER_LTEM3F
// #define HOST_FEATHER_UXPLOR_L
#define HOST_ESP32_DEVMOD_BMS

#define PERIOD_FROM_SECONDS(period)  (period * 1000)
#define PERIOD_FROM_MINUTES(period)  (period * 1000 * 60)
#define ELAPSED(start, timeout) ((start == 0) ? 0 : millis() - start > timeout)

// LTEmC Includes
#include <ltemc.h>


// test controls
uint16_t loopCnt = 0;
uint16_t cycle_interval = 2000;
uint32_t lastCycle;
resultCode_t rslt;


void setup() 
{
    #ifdef DIAGPRINT_SERIAL
        Serial.begin(115200);
        delay(5000);                // just give it some time
    #endif
    DIAGPRINT(PRNT_DEFAULT,"\n\n*** ltemc-04-atcmd started ***\n\n");

    ltem_create(ltem_pinConfig, NULL, appEvntNotify);                   // create LTEmC modem
    ltem_start(resetAction_swReset);                                    // ... and start it

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
        DIAGPRINT(PRNT_DEFAULT, "Invoking cmd: %s \r\n", cmdStr);

        if (atcmd_tryInvoke(cmdStr))
        {
            resultCode_t atResult = atcmd_awaitResult();
            
                char *response = atcmd_getRawResponse();
                DIAGPRINT(PRNT_INFO, "Got %d chars\r", strlen(response));
                DIAGPRINT(PRNT_WHITE, "Resp:");
                DIAGPRINT(PRNT_CYAN, "%s\r", response);
                                                                                                // test response v. expected 
                char *validResponse = "\r\nQuectel\r\nBG";                                      // near beginning (depends on BGx echo)
                if (!strstr(response, validResponse))
                    indicateFailure("Expected cmd response missing... failed."); 

            if (atResult != resultCode__success)                                                // statusCode == 200 (similar to HTTP codes)
            {
                DIAGPRINT(PRNT_ERROR, "atResult=%d \r", atResult);
                // indicateFailure("Unexpected command response... failed."); 
                indicateFailure("Invalid BGx response");
            }

            /* atcmd_close();       Not needed here since tryInvokeDefaults(). 
                                    With options and manual lock, required when done with response to close action and release action lock */
        }
        else
            DIAGPRINT(PRNT_WARN, "Unable to get action lock.\r");


        DIAGPRINT(0,"Loop=%d \n\n", loopCnt);
     }
}


/* test helpers
========================================================================================================================= */

void appEvntNotify(appEvent_t eventType, const char *notifyMsg)
{
    if (eventType == appEvent_fault_assertFailed)
        DIAGPRINT(PRNT_ERROR, "LTEmC-HardFault: %s\r", notifyMsg);
    else 
        DIAGPRINT(PRNT_WHITE, "LTEmC Info: %s\r", notifyMsg);
    return;
}

void indicateFailure(char failureMsg[])
{
	DIAGPRINT(PRNT_ERROR, "\r\n** %s \r", failureMsg);
    DIAGPRINT(PRNT_ERROR, "** Test Assertion Failed. \r");

    #if 1
    DIAGPRINT(PRNT_ERROR, "** Halting Execution \r\n");
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


