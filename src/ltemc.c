/** ***************************************************************************
  @file 
  @brief Driver application for control and use of the LooUQ LTEm cellular modem.

  @author Greg Terrell, LooUQ Incorporated

  \loouq

  @warning Internal dependencies, changes only as directed by LooUQ staff.

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


#define SRCFILE "LTE"                       // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT
#define ENABLE_DIAGPRINT                    // expand DPRINT into debug output
//#define ENABLE_DIAGPRINT_VERBOSE            // expand DPRINT and DPRINT_V into debug output
#define ENABLE_ASSERT
#include <lqdiag.h>

#include "ltemc.h"
#include "ltemc-internal.h"

/* ------------------------------------------------------------------------------------------------
 * GLOBAL LTEm Device Objects, One LTEmX supported
 * --------------------------------------------------------------------------------------------- */
ltemDevice_t g_lqLTEM;

#define MODULERDY_TIMEOUT 8000

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))


/* BGx module initialization commands (start script)
 * ---------------------------------------------------------------------------------------------
 * used in ltemc-quectel-bg.c for module initialization, declared here for convenience
 * LTEmC requires that no-echo is there, append any ADDITIONAL global module setting command in the list.
 * Ex: Radio setup (RAT search, IoT mode, etc.) 
 * ------------------------------------------------------------------------------------------------ */
const char* const qbg_initCmds[] = 
{ 
    "ATE0\r",                                       // don't echo AT commands on serial
    "AT+QURCCFG=\"urcport\",\"uart1\"\r"            // URC events are reported to UART1
};

// makes for compile time automatic sz determination
int8_t qbg_initCmdsCnt = sizeof(qbg_initCmds)/sizeof(const char* const);            


/* Static Local Function Declarations
------------------------------------------------------------------------------------------------ */
// static bool S__initLTEmDevice();
static void S__ltemUrcHandler();
static cmdParseRslt_t S__iccidCompleteParser(ltemDevice_t *modem);



#pragma region Public Functions
/*-----------------------------------------------------------------------------------------------*/

/**
 * @brief Initialize the LTEm1 modem.
 */
void ltem_create(const ltemPinConfig_t ltem_config, yield_func yieldCallback, appEvntNotify_func eventNotifCallback)
{
    ASSERT(g_lqLTEM.atcmd == NULL);                 // prevent multiple calls, memory leak calloc()
    memset(&g_lqLTEM, 0, sizeof(ltemDevice_t));
    
	g_lqLTEM.pinConfig = ltem_config;
    #if (ARDUINO_ARCH_ESP32)
        g_lqLTEM.platformSpi = spi_createFromPins(g_lqLTEM.pinConfig.spiClkPin, g_lqLTEM.pinConfig.spiMisoPin, g_lqLTEM.pinConfig.spiMosiPin, g_lqLTEM.pinConfig.spiCsPin);
    #else
        g_lqLTEM.platformSpi = spi_createFromIndex(g_lqLTEM.pinConfig.spiIndx, g_lqLTEM.pinConfig.spiCsPin);
    #endif

    g_lqLTEM.modemSettings =  calloc(1, sizeof(modemSettings_t));
    ASSERT(g_lqLTEM.modemSettings != NULL);

    g_lqLTEM.modemInfo = calloc(1, sizeof(modemInfo_t));
    ASSERT(g_lqLTEM.modemInfo != NULL);

    g_lqLTEM.ntwkOperator =  calloc(1, sizeof(ntwkOperator_t));
    ASSERT(g_lqLTEM.operatorInfo != NULL);

    IOP_create();
    
    g_lqLTEM.atcmd = calloc(1, sizeof(atcmd_t));
    ASSERT(g_lqLTEM.atcmd != NULL);
    ATCMD_reset(true);                                          // initialize atcmd control values

    g_lqLTEM.fileCtrl = calloc(1, sizeof(fileCtrl_t));
    ASSERT(g_lqLTEM.fileCtrl != NULL);

    ntwk_create();

    g_lqLTEM.cancellationRequest = false;
    g_lqLTEM.appEvntNotifyCB = eventNotifCallback;
}



/**
 * @brief Uninitialize the LTEm device structures.
 */
void ltem_destroy()
{
	ltem_stop(false);

	gpio_pinClose(g_lqLTEM.pinConfig.irqPin);
	gpio_pinClose(g_lqLTEM.pinConfig.powerkeyPin);
	gpio_pinClose(g_lqLTEM.pinConfig.resetPin);
	gpio_pinClose(g_lqLTEM.pinConfig.statusPin);

    ip_destroy();
    free(g_lqLTEM.atcmd);
    iop_destroy();
    spi_destroy(g_lqLTEM.platformSpi);
}


/**
 * @brief Start the modem.
 */
bool ltem_start(resetAction_t resetAction)
{
    LTEM_diagCallback(">> ltem_start()");  
    g_lqLTEM.appEventNotifyEnabled = false;                                     // start may be a restart, suspend operations
    g_lqLTEM.iop->isrEnabled = false;

    if (!g_lqLTEM.hostConfigured)
    {
        // for specific platforms/MCU, need to ensure pin in default "logical" state prior to opening
        platform_writePin(g_lqLTEM.pinConfig.powerkeyPin, gpioValue_low);
        platform_writePin(g_lqLTEM.pinConfig.resetPin, gpioValue_low);
        platform_writePin(g_lqLTEM.pinConfig.spiCsPin, gpioValue_high);
        platform_writePin(g_lqLTEM.pinConfig.irqPin, gpioValue_high);

        platform_openPin(g_lqLTEM.pinConfig.powerkeyPin, gpioMode_output);		// powerKey: normal low
        platform_openPin(g_lqLTEM.pinConfig.resetPin, gpioMode_output);			// resetPin: normal low
        platform_openPin(g_lqLTEM.pinConfig.spiCsPin, gpioMode_output);			// spiCsPin: invert, normal gpioValue_high
        platform_openPin(g_lqLTEM.pinConfig.statusPin, gpioMode_input);
        platform_openPin(g_lqLTEM.pinConfig.irqPin, gpioMode_inputPullUp);
        DPRINT_V(PRNT_DEFAULT, "GPIO Configured\r\n");

        spi_start(g_lqLTEM.platformSpi);                                        // start host SPI
        DPRINT_V(PRNT_DEFAULT, "SPI Configured\r\n");
        g_lqLTEM.hostConfigured = true;
    }

    DPRINT(PRNT_CYAN, "LTEm reqst resetType=%d\r\n", resetAction);
    bool ltemWasReset = true;
    if (QBG_isPowerOn())
    {
        if (resetAction == resetAction_skipIfOn)
            ltemWasReset = false;
        else
        {
            if (resetAction == resetAction_swReset)
            {
                if (!SC16IS7xx_ping())                                          // fall back to power reset if UART not available
                    resetAction = resetAction_powerReset;
            }
            QBG_reset(resetAction);                                             // do requested reset (sw, hw, pwrCycle)
        }
    }
    else 
    {
       QBG_powerOn();                                                           // turn on BGx
    }
    DPRINT_V(PRNT_DEFAULT, "LTEm was reset=%d\r\n", ltemWasReset);

    SC16IS7xx_start();                                                          // initialize NXP SPI-UART bridge base functions: FIFO, levels, baud, framing
    DPRINT_V(PRNT_CYAN, "UART started\r\n");
    SC16IS7xx_enableIrqMode();                                                  // enable IRQ generation on SPI-UART bridge (IRQ mode)
    DPRINT_V(PRNT_CYAN, "UART set to IRQ mode\r\n");
    IOP_attachIrq();                                                            // attach I/O processor ISR to IRQ
    DPRINT_V(PRNT_CYAN, "UART IRQ attached\r\n");

    IOP_interruptCallbackISR();                                                 // force ISR to run once to sync IRQ 
    g_lqLTEM.appEventNotifyEnabled = true;                                      // through the low-level actions, re-enable notifications

    DPRINT_V(0, "LTEm prior state=%d\r\n", g_lqLTEM.deviceState);

    uint32_t appReady = 0;
    uint32_t simReady = 0;
    uint32_t startModuleReady = pMillis();                                      // wait for BGx to signal internal ready
    do
    {
        if (BBFFR_ISFOUND(bbffr_find(g_lqLTEM.iop->rxBffr, "APP RDY", 0, 0, false)))
        {
            appReady = pMillis();                                               // support timings reporting
        }
        if (BBFFR_ISFOUND(bbffr_find(g_lqLTEM.iop->rxBffr, "+CPIN: READY", 0, 0, false)))
        {
            simReady = pMillis();
        }

        if (IS_ELAPSED(startModuleReady, MODULERDY_TIMEOUT))
        {
            DPRINT_V(PRNT_WARN, "Timeout (%dms) waiting for module ready (%d/%d)!\r\n", MODULERDY_TIMEOUT, appReady, simReady);
            break;
        }
    } while (!appReady || !simReady);
    
    if (!simReady)
        ltem_notifyApp(appEvent_fault_hardFault, "SIM fault");                      // send notification, maybe app can recover
    if (!appReady)
        ltem_notifyApp(appEvent_fault_hardFault, "BGx module fault: not AppRdy");   // send notification, maybe app can recover

    if (appReady && simReady)
    {
        g_lqLTEM.deviceState = deviceState_appReady;
        DPRINT(PRNT_dCYAN, "Module ready at %dms (%d/%d)\r\n", pMillis() - startModuleReady, appReady - startModuleReady, simReady - startModuleReady);
    }
    else
        return false;
    pDelay(500);
    bbffr_reset(g_lqLTEM.iop->rxBffr);

    DPRINT_V(0, "LTEm state=%d\r\n", g_lqLTEM.deviceState);

    if (!QBG_setOptions())
    {
        ltem_notifyApp(appEvent_fault_hardFault, "BGx init cmd fault");         // send notification, maybe app can recover
        DPRINT(PRNT_DEFAULT, "\r");
    }
    else
        DPRINT_V(PRNT_CYAN, "BGx options set\r\n");

    // ntwk_setRatOptions();                                            // initialize BGx Radio Access Technology (RAT) options
    // DPRINT_V(PRNT_CYAN, "ltem_start(): rat options set");

    ntwk_applyPpdNetworkConfig();                                       // configures default PDP context for likely autostart with provider attach
    DPRINT_V(PRNT_CYAN, "ltem_start(): pdp ntwk configured\r\n");

    ntwk_awaitOperator(2);                                              // attempt to warm-up provider/PDP briefly. 
    DPRINT_V(PRNT_CYAN, "ltem_start(): provider warmed up\r\n");        // If longer duration required, leave that to application

    ltem_getModemInfo();                                                // populate modem info struct
    return true;
}


// /**
//  * @brief Static internal BGx initialization logic shared between start and reset
//  */
// bool S__initLTEmDevice()
// {
//     ASSERT(QBG_isPowerOn());
//     ASSERT(SC16IS7xx_isAvailable());

//     SC16IS7xx_start();                                                  // initialize NXP SPI-UART bridge base functions: FIFO, levels, baud, framing

//     if (g_lqLTEM.deviceState != deviceState_appReady)
//         return false;

//     if (!QBG_setOptions())
//     {
//         ltem_notifyApp(appEvent_fault_hardFault, "BGx init cmd fault"); // send notification, maybe app can recover
//         DPRINT(PRNT_DEFAULT, "\r");
//     }
//     else
//         DPRINT_V(PRNT_CYAN, "S__initLTEmDevice(): bgx options set");

//     // ntwk_setRatOptions();                                            // initialize BGx Radio Access Technology (RAT) options
//     // DPRINT_V(PRNT_CYAN, "S__initLTEmDevice(): rat options set");

//     ntwk_applyPpdNetworkConfig();                                       // configures default PDP context for likely autostart with provider attach
//     DPRINT_V(PRNT_CYAN, "S__initLTEmDevice(): pdp ntwk configured");

//     ntwk_awaitProvider(2);                                              // attempt to warm-up provider/PDP briefly. 
//     DPRINT_V(PRNT_CYAN, "S__initLTEmDevice(): provider warmed up");     // If longer duration required, leave that to application

//     return true;
// }


/**
 * @brief Powers off the modem without destroying memory objects. Modem device will require ltem_start() to reinit HW.
 */
void ltem_stop()
{
    spi_stop(g_lqLTEM.platformSpi);
    IOP_stopIrq();
    g_lqLTEM.deviceState = deviceState_powerOff;
    QBG_powerOff();
}


/**
 * @brief Performs a reset of LTEm.
 */
bool ltem_reset(bool hardReset)
{
    resetAction_t resetAction = hardReset ? resetAction_hwReset : resetAction_swReset;
    return ltem_start(resetAction);
}


/**
 * @brief Turn modem power OFF
 */
void ltem_powerOff()
{
    QBG_powerOff();  
}


void ltem_enterPcm()
{
}


/**
 * @brief Set RF priority on BG95/BG77 modules. 
 */
resultCode_t ltem_setRfPriorityMode(ltemRfPriorityMode_t mode)
{
    DPRINT_V(0, "<ltem_setRfPriorityMode()> mode=%d\r\n", mode);
    DPRINT_V(0, "<ltem_setRfPriorityMode()> module:%s\r\n", g_lqLTEM.modemInfo->model);

    if (memcmp(g_lqLTEM.modemInfo->model, "BG95", 4) == 0 || memcmp(g_lqLTEM.modemInfo->model, "BG77", 4) == 0)
    {
        DPRINT_V(0, "<ltem_setRfPriorityMode()> invoking\r\n");
        if (atcmd_tryInvoke("AT+QGPSCFG=\"priority\",%d", mode))
        {
            DPRINT_V(0, "<ltem_setRfPriorityMode()> invoked\r\n");
            resultCode_t rslt = atcmd_awaitResult();
            DPRINT_V(0, "<ltem_setRfPriorityMode()> response:%s\r\n", atcmd_getRawResponse());
            return rslt;
        }
        return resultCode__conflict;
    }
    return resultCode__preConditionFailed;                                          //  only applicable to single-RF modules
}


/**
 * @brief Get RF priority on BG95/BG77 modules. 
 */
ltemRfPriorityMode_t ltem_getRfPriorityMode()
{
    if ((memcmp(g_lqLTEM.modemInfo->model, "BG95", 4) == 0) || (memcmp(g_lqLTEM.modemInfo->model, "BG77", 4) == 0))
    {
        if (!atcmd_tryInvoke("AT+QGPSCFG=\"priority\""))
            return ltemRfPriorityMode_none;                                         // resultCode__conflict

        if (IS_SUCCESS(atcmd_awaitResult()))
        {
            uint32_t mode = strtol(atcmd_getToken(1), NULL, 10);
            DPRINT_V(0, "<ltem_getRfPriorityMode> mode=%d\r\n", mode);
            return mode;
        }
    }
    return ltemRfPriorityMode_none;
}


/**
 * @brief Get RF priority on BG95/BG77 modules. 
 */
ltemRfPriorityState_t ltem_getRfPriorityState()
{
    if (memcmp(g_lqLTEM.modemInfo->model, "BG95", 4) == 0 || memcmp(g_lqLTEM.modemInfo->model, "BG77", 4) == 0)
    {
        if (atcmd_tryInvoke("AT+QGPSCFG=\"priority\""))
        {
            if (atcmd_awaitResult() == resultCode__success)
            {
                uint32_t state = strtol(atcmd_getToken(2), NULL, 10);
                DPRINT_V(0, "<ltem_getRfPriorityState> state=%d\r\n", state);
                return state;
            }
        }
    }
    DPRINT_V(0, "<ltem_getRfPriorityState> state=0\r\n");
    return ltemRfPriorityState_unloaded;
}


/**
 * @brief Get the current UTC date and time.
 */
const char* ltem_getUtcDateTime(char format)
{
    char* dtSrc;
    uint8_t len;

    memset(&g_lqLTEM.statics.dateTimeBffr, 0, ltem__dateTimeBffrSz);            // return empty string if failure

    char* destPtr = &g_lqLTEM.statics.dateTimeBffr;
    char* dtDbg  = &g_lqLTEM.statics.dateTimeBffr;

    if (destPtr != NULL && atcmd_tryInvoke("AT+CCLK?"))
    {
        if (atcmd_awaitResult() == resultCode__success)
        {
            if ((dtSrc = memchr(atcmd_getResponseData(), '"', 12)) != NULL)         // allowance for preceeding EOL
            {
                dtSrc++;
                if (*dtSrc != '8')                                              // test for not initialized date/time, starts with 80 (aka 1980)
                {
                    DPRINT_V(0, "ltem_getUtcDateTime(): format=%c\r\n", format);

                    if (format == 'v' || format == 'V')                         // "VERBOSE" format
                    {
                        char* tzDelimPoz = memchr(dtSrc, '+', 20);              // strip UTC offset, safe stop in trailer somewhere
                        char* tzDelimNeg = memchr(dtSrc, '-', 20);              // strip UTC offset, safe stop in trailer somewhere
                        DPRINT_V(0, "ltem_getUtcDateTime(): tzDelimPoz=%p, tzDelimNeg=%p\r\n", tzDelimPoz, tzDelimNeg);

                        vTaskDelay(100);

                        if (tzDelimPoz)
                        {
                            DPRINT_V(0, "ltem_getUtcDateTime(): tmZone offset=%d\r\n", tzDelimPoz - dtSrc);
                            *tzDelimPoz = '\0';                                 // verbose displays local time, use ltem_getLocalTimezoneOffset() to get TZ
                            strcpy(destPtr, dtSrc);                             // safe c-string strcpy to dateTime
                        }
                        else if (tzDelimNeg)
                        {
                            DPRINT_V(0, "ltem_getUtcDateTime(): offset=%d\r\n", tzDelimNeg - dtSrc);
                            *tzDelimNeg = '\0';                                 // verbose displays local time, use ltem_getLocalTimezoneOffset() to get TZ
                            strcpy(destPtr, dtSrc);                             // safe c-string strcpy to dateTime
                        }
                        return &g_lqLTEM.statics.dateTimeBffr;
                    }

                    /*  process 'i'= ISO8601 or 'c'= compact ISO (2-digit year and no timezone)
                    */
                    if (format != 'c' && format != 'C')                         // not 'c'ompact format: 4 digit year
                    {
                        memcpy(destPtr, "20", 2);                               // convert to 4 digit year (ISO8601)
                        destPtr += 2;
                    }
                    memcpy(destPtr, dtSrc, 2);                                  // 2-digit year

                    DPRINT_V(0, "ltem_getUtcDateTime(): post-year: %s, len=%d\r\n", dtDbg, strlen(dtDbg));

                    destPtr += 2;
                    memcpy(destPtr, dtSrc + 3, 2);                              // month

                    DPRINT_V(0, "ltem_getUtcDateTime(): post-month: %s, len=%d\r\n", dtDbg, strlen(dtDbg));

                    destPtr += 2;
                    memcpy(destPtr, dtSrc + 6, 2);                              // day

                    DPRINT_V(0, "ltem_getUtcDateTime(): post-day: %s, len=%d\r\n", dtDbg, strlen(dtDbg));

                    destPtr += 2;
                    *destPtr = 'T';                                             // delimiter
                    destPtr += 1;

                    DPRINT_V(0, "ltem_getUtcDateTime(): post-T: %s, len=%d\r\n", dtDbg, strlen(dtDbg));

                    memcpy(destPtr, dtSrc + 9, 2);                              // hours
                    destPtr += 2;

                    DPRINT_V(0, "ltem_getUtcDateTime(): post-hours: %s, len=%d\r\n", dtDbg, strlen(dtDbg));

                    memcpy(destPtr, dtSrc + 12, 2);                       // minutes
                    destPtr += 2;

                    DPRINT_V(0, "ltem_getUtcDateTime(): post-minutes: %s, len=%d\r\n", dtDbg, strlen(dtDbg));

                    memcpy(destPtr, dtSrc + 15, 2);                       // seconds
                    destPtr += 2;

                    DPRINT_V(0, "ltem_getUtcDateTime(): post-seconds: %s, len=%d\r\n", dtDbg, strlen(dtDbg));

                    if (format != 'c' && format != 'C')                 // not 'c'ompact format: include time zone offset
                    {
                        strcat(destPtr, "Z");
                    }
               }
            }
        }
    }
    return (const char*)&g_lqLTEM.statics.dateTimeBffr;
}


/**
 * @brief Get local time zone offset.
 */
int8_t ltem_getLocalTimezoneOffset(bool precise)
{
    char dateTime[30] = {0};
    char *dtSrc;

    if (!atcmd_tryInvoke("AT+CCLK?"))
        return 0;                                                           // resultCode__conflict normally

    if (IS_SUCCESS(atcmd_awaitResult()))
    {
        if ((dtSrc = memchr(atcmd_getResponseData(), '"', 12)) != NULL)         // tolerate preceeding EOL
        {
            dtSrc++;
            if (*dtSrc != '8')                                              // test for not initialized date/time, stardtSrc with 80 (aka 1980)
            {
                char* tzDelim = memchr(dtSrc, '-', 20);                     // strip UTC offset, safe stop in trailer somewhere
                if (tzDelim != NULL)                                        // found expected - delimeter before TZ offset
                {
                    if (precise)
                        return strtol(tzDelim, NULL, 10);                   // BGx reports 15min TZ offsets (supports 30, 45 minutes TZ offset regions)
                    else
                        return strtol(tzDelim, NULL, 10) / 4;               // BGx reports 15min TZ offsets (supports 30, 45 minutes TZ offset regions)
                }
            }
        }
    }
    return 0;
}


/**
 *  @brief Get the LTEm static device identification/provisioning information.
 */
modemInfo_t* ltem_getModemInfo()
{
    if (g_lqLTEM.modemInfo->imei[0] == 0)
    {
        if (!atcmd_tryInvoke("AT+GSN"))
            return resultCode__conflict;

        if (IS_SUCCESS(atcmd_awaitResult()))
        {
            strncpy(g_lqLTEM.modemInfo->imei, atcmd_getResponseData(), ntwk__imeiSz);
        }
    }

    if (g_lqLTEM.modemInfo->fwver[0] == 0)
    {
        if (!atcmd_tryInvoke("AT+QGMR"))
            return resultCode__conflict;

        if (IS_SUCCESS(atcmd_awaitResult()))
        {
            char *eol;
            if ((eol = strstr(atcmd_getResponseData(), "\r\n")) != NULL)
            {
                uint8_t sz = eol - atcmd_getResponseData();
                memcpy(g_lqLTEM.modemInfo->fwver, atcmd_getResponseData(), MIN(sz, ntwk__dvcFwVerSz));
            }
        }
    }

    if (g_lqLTEM.modemInfo->mfg[0] == 0)
    {
        if (!atcmd_tryInvoke("ATI"))
            return resultCode__conflict;

        if (IS_SUCCESS(atcmd_awaitResult()))
        {
            char* response = atcmd_getResponseData();
            char* eol = strchr(response, '\r');
            memcpy(g_lqLTEM.modemInfo->mfg, response, eol - response);

            response = eol + 2;
            eol = strchr(response, '\r');
            memcpy(g_lqLTEM.modemInfo->model, response, eol - response);

            response = eol + 2;
            eol = strchr(response, ':');
            response = eol + 2;
            eol = strchr(response, '\r');
            memcpy(g_lqLTEM.modemInfo->fwver, response, eol - response);
        }
    }

    if (g_lqLTEM.modemInfo->iccid[0] == 0)
    {
        atcmd_ovrrdParser(S__iccidCompleteParser);
        if (!atcmd_tryInvoke("AT+ICCID"))
            return resultCode__conflict;

        if (IS_SUCCESS(atcmd_awaitResult()))
        {
            char* delimAt;
            char* responseAt = atcmd_getResponseData();
            if (strlen(responseAt) && (delimAt = memchr(responseAt, '\r', strlen(responseAt))))
            {
                memcpy(g_lqLTEM.modemInfo->iccid, responseAt, MIN(delimAt - responseAt, ntwk__iccidSz));
            }
        }
    }
    return g_lqLTEM.modemInfo;
}


/**
 *  @brief Test for SIM ready
 */
bool ltem_isSimReady()
{
    bool cpinState = false;
    if (!atcmd_tryInvoke("AT+CPIN?"))
        return false;                                                               // resultCode_conflict

    if (IS_SUCCESS(atcmd_awaitResult()))
    {
        cpinState = strstr(atcmd_getResponseData(), "+CPIN: READY") != NULL;
    }
    return strlen(g_lqLTEM.modemInfo->iccid) > 0 && cpinState;
}


/**
 *  @brief Get the signal strenght as raw value returned from BGx.
 */
uint8_t ltem_signalRaw()
{
    uint8_t signalValue = 99;

    if (ltem_getDeviceState() || !atcmd_tryInvoke("AT+CSQ"))
    {
        if (IS_SUCCESS(atcmd_awaitResult()))
        {
            char *term;
            char *lastResponse = atcmd_getResponseData();
            term = strstr(atcmd_getResponseData(), "+CSQ");
            signalValue = strtol(term + 6, NULL, 10);
        }
    }
    return signalValue;
}


/**
 *  @brief Get the signal strength reported by the LTEm device at a percent
 */
uint8_t mdmInfo_signalPercent()
{
    double csq;
    uint8_t signal = 0;
    const double csqFactor = 3.23;

    csq = (double)ltem_signalRaw();
    signal = (csq == 99) ? 0 : (uint8_t)(csq * csqFactor);
    return signal;
}

/**
 *  @brief Get the signal strenght as RSSI (db).
 */
int16_t ltem_signalRSSI()
{
    const int8_t rssiBase = -113;
    const int8_t rssiRange = 113 - 51;

    uint8_t signalPercent = mdmInfo_signalPercent();
    return (signalPercent == 0) ? rssiBase : (signalPercent * 0.01 * rssiRange) + rssiBase;
}

/**
 *  @brief Get the signal strength, as a bar count for visualizations, (like on a smartphone)
 * */
uint8_t ltem_signalBars(uint8_t displayBarCount)
{
    const int8_t barOffset = 20; // adjust point for full-bar percent (20 = full bar count at 80%)

    uint8_t barSpan = 100 / displayBarCount;
    uint8_t signalPercent = MIN(mdmInfo_signalPercent() + barOffset, 100);
    return (uint8_t)(signalPercent / barSpan);
}



/**
 * @brief Get the LTEmC software version.
 */
const char* ltem_getSwVersion()
{
    return LTEmC_VERSION;
}


/**
 * @brief Get the LTEmC software version.
 */
const char* ltem_getModuleType()
{
    return g_lqLTEM.modemInfo->model;
}


/**
 * @brief Get the operating state of the BGx module
 * @return Enum of module current state
 */
deviceState_t ltem_getDeviceState()
{
    DPRINT_V(0, "<ltem_getDeviceState()> prior state=%d\r\n", g_lqLTEM.deviceState);

    if (QBG_isPowerOn())             // ensure powered off device doesn't report otherwise
        g_lqLTEM.deviceState = MAX(g_lqLTEM.deviceState, deviceState_powerOn); 
    else
        g_lqLTEM.deviceState = deviceState_powerOff;

    DPRINT_V(0, "<ltem_getDeviceState()> new state=%d\r\n", g_lqLTEM.deviceState);
    return g_lqLTEM.deviceState;
}


/**
 * @brief Test for responsive BGx.
 */
bool ltem_ping()
{
    resultCode_t rslt;
    if (atcmd_tryInvoke("AT"))
    {
        rslt = atcmd_awaitResult();
        return rslt != resultCode__timeout;
    }
    return false;
}


/**
 * @brief Background work task runner. To be called in application Loop() periodically.
 */
void ltem_eventMgr()
{
    /* look for a new incoming URC 
     */
    int16_t potentialUrc = bbffr_find(g_lqLTEM.iop->rxBffr, "+", 0, 0, false);      // look for URC prefix char in RX buffer
    if (BBFFR_ISNOTFOUND(potentialUrc))
    {
        return;                                                                     // nope, done here
    }

    /* Invoke each stream's URC handler (if stream has one), it will service or return with a cancelled if not handled
     */
    for (size_t i = 0; i < ltem__streamCnt; i++)                                    // potential URC in rxBffr, see if a data handler will service
    {
        resultCode_t serviceRslt;
        if (g_lqLTEM.streams[i] != NULL &&  g_lqLTEM.streams[i]->urcHndlr != NULL)  // URC event handler in this stream, offer the data to the handler
        {
            serviceRslt = g_lqLTEM.streams[i]->urcHndlr();
        }
        if (serviceRslt == resultCode__cancelled)                                   // not serviced, continue looking
        {
            continue;
        }
        break;                                                                      // service attempted (might have errored), so this event is over
    }

    S__ltemUrcHandler();                                                            // always invoke system level URC validation/service
}


void ltem_addStream(streamCtrl_t *streamCtrl)
{
    DPRINT_V(PRNT_INFO, "Registering Stream\r\n");
    streamCtrl_t* prev = ltem_getStreamFromCntxt(streamCtrl->dataCntxt, streamType__ANY);

    if (prev != NULL)
        return;

    for (size_t i = 0; i < ltem__streamCnt; i++)
    {
        if (g_lqLTEM.streams[i] == NULL)
        {
            g_lqLTEM.streams[i] = streamCtrl;
            return;
        }
    }
}


void ltem_deleteStream(streamCtrl_t *streamCtrl)
{
    for (size_t i = 0; i < ltem__streamCnt; i++)
    {
        if (g_lqLTEM.streams[i]->dataCntxt == streamCtrl->dataCntxt)
        {
            ASSERT(memcmp(g_lqLTEM.streams[i], streamCtrl, sizeof(streamCtrl_t)) == 0);     // compare the common fields
            g_lqLTEM.streams[i] = NULL;
            return;
        }
    }
}


streamCtrl_t* ltem_getStreamFromCntxt(uint8_t context, streamType_t streamType)
{
    for (size_t i = 0; i < ltem__streamCnt; i++)
    {
        if (g_lqLTEM.streams[i] != NULL && g_lqLTEM.streams[i]->dataCntxt == context)
        {
            if (streamType == streamType__ANY)
            {
                return g_lqLTEM.streams[i];
            }
            else if (g_lqLTEM.streams[i]->streamType == streamType)
            {
                return g_lqLTEM.streams[i];
            }
            else if (streamType == streamType__SCKT)
            {
                if (g_lqLTEM.streams[i]->streamType == streamType_UDP ||
                    g_lqLTEM.streams[i]->streamType == streamType_TCP ||
                    g_lqLTEM.streams[i]->streamType == streamType_SSLTLS)
                {
                    return g_lqLTEM.streams[i];
                }
            }
        }
    }
    return NULL;
}

/**
 * @brief Notify host application of significant events. Application may ignore, display, save status, whatever. 
 */
void ltem_notifyApp(uint8_t notifyType, const char *notifyMsg)
{
    if (g_lqLTEM.appEventNotifyEnabled && g_lqLTEM.appEvntNotifyCB != NULL)                                       
        (g_lqLTEM.appEvntNotifyCB)(notifyType, notifyMsg);                                // if app handler registered, it may/may not return
}


/**
 * @brief Registers the address (void*) of your application event nofication callback handler.
 */
void ltem_setEventNotifCallback(appEvntNotify_func eventNotifCallback)
{
    g_lqLTEM.appEventNotifyEnabled = true;
    g_lqLTEM.appEvntNotifyCB = eventNotifCallback;
}

/**
 * @brief Registers the address (void*) of your application yield callback handler.
 */
void ltem_setYieldCallback(platform_yieldCB_func_t yieldCallback)
{
    platform_yieldCB_func = yieldCallback;
}


#pragma endregion


#pragma region LTEmC Internal Functions (ltemc-internal.h)
/*-----------------------------------------------------------------------------------------------*/

// typedef void (*appDiagCallback_func)(const char *diagPointDescription);                                 // application diagnostics callback

void LTEM_registerDiagCallback(appDiagCallback_func diagCB)
{
    //argument of type "void (*)(char *info_string)" is incompatible with parameter of type "appDiagCallback_func"C/C++(167)
    g_lqLTEM.appDiagnosticCB = diagCB;
}

void LTEM_diagCallback(const char* diagPointDescription)
{
    if (g_lqLTEM.appDiagnosticCB != NULL)                                       
        (g_lqLTEM.appDiagnosticCB)(diagPointDescription);                                // if app diag registered invoke it
}

// void LTEM_registerUrcHandler(urcHandler_func *urcHandler)
// {
//     bool registered = false;
//     for (size_t i = 0; i < ltem__urcHandlersCnt; i++)
//     {
//         if (g_lqLTEM.urcHandlers[i] == NULL)
//         {
//             registered = true;
//             g_lqLTEM.urcHandlers[i] = urcHandler;                               // add to "registered" handlers
//         }
//         else
//         {
//             if (g_lqLTEM.urcHandlers[i] == urcHandler)                          // previously registered
//                 registered = true;
//         }
//         ASSERT(registered);
//     }
// }

// uint8_t LTEM__getStreamIndx(dataCntxt_t dataCntxt)
// {
//     for (size_t indx = 0; indx < ltem__streamCnt; indx++)
//     {
//         if (g_lqLTEM.streams[indx]->dataContext == dataCntxt)
//         {
//             return indx;
//         }
//     }
// }

#pragma endregion


/*
 * --------------------------------------------------------------------------------------------- */
#pragma region Static Function Definitions


/**
 * @brief Global URC handler
 * @details Services URC events that are not specific to a stream/protocol
 */
static void S__ltemUrcHandler()                                                     
{
    /* LTEm System URCs Handled Here
     *
     * +QIURC: "pdpdeact",<contextID>                               // PDP context closed (power down, remote termination)
     * POWER 
    */

    bBuffer_t *rxBffr = g_lqLTEM.iop->rxBffr;                       // for convenience
    char parseBffr[30];

    // bool isQuectel = cbffr_find(rxBffr, "+Q", 0, 0, false) != CBFFR_NOFIND;      // look for Quectel or CCITT URC prefixes
    // bool isCCITT = cbffr_find(rxBffr, "+C", 0, 0, false) != CBFFR_NOFIND;

    /* PDP (packet network) deactivation/close
     ------------------------------------------------------------------------------------------- */
    // if (cbffr_find(rxBffr, "+QIURC: \"pdpdeact\"", 0, 0, true) >= 0)
    // {
    //     for (size_t i = 0; i < dataCntxt__cnt; i++)
    //     {
    //         if (g_lqLTEM.streams[i].dataCloseCB)
    //         {
    //             g_lqLTEM.streams[i].dataCloseCB(i);
    //         }
    //     }
    // }
 
}


/**
 * @brief Action response parser for iccid value request.
 */
static cmdParseRslt_t S__iccidCompleteParser(ltemDevice_t *modem)
{
    return atcmd_stdResponseParser("+ICCID: ", true, "", 0, 0, "\r\n\r\nOK\r\n", 20);
}



#pragma endregion
