/** ***************************************************************************
  @file 
  @brief API for control and use of the LooUQ LTEm cellular modem.

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

#include <lq-embed.h>
#define lqLOG_LEVEL lqLOGLEVEL_DBG
//#define DISABLE_ASSERTS                                   // ASSERT/ASSERT_W enabled by default, can be disabled 
#define LQ_SRCFILE "LTE"                                    // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT

#include "ltemc.h"
#include "ltemc-internal.h"

/* ------------------------------------------------------------------------------------------------
 * GLOBAL LTEm Device Objects, One LTEmX supported
 * --------------------------------------------------------------------------------------------- */
ltemDevice_t g_lqLTEM;

#define APPRDY_TIMEOUT 8000

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
 *	@brief Initialize the LTEm1 modem.
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
    ASSERT(g_lqLTEM.ntwkOperator != NULL);

    IOP_create();
    
    g_lqLTEM.atcmd = calloc(1, sizeof(atcmd_t));
    ASSERT(g_lqLTEM.atcmd != NULL);
    atcmd_resetPostInvoke();                                        // reset to after cmd state, ready for next cmd

    g_lqLTEM.fileCtrl = calloc(1, sizeof(fileCtrl_t));
    ASSERT(g_lqLTEM.fileCtrl != NULL);

    ntwk_create();

    g_lqLTEM.cancellationRequest = false;
    g_lqLTEM.appEvntNotifyCB = eventNotifCallback;
}



/**
 *	@brief Uninitialize the LTEm device structures.
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
 *	@brief Start the modem.
 */
bool ltem_start(resetAction_t resetAction)
{
    LTEM_diagCallback(">> ltem_start()");  
    g_lqLTEM.appEventNotifyEnabled = false;                                     // start may be a restart, suspend operations
    g_lqLTEM.iop->isrEnabled = false;

    lqLOG_VRBS("(ltem_start) ready to init platform\r\n");
    if (!g_lqLTEM.hostConfigured)
    {
        // on Arduino compatible, ensure pin is in default "logical" state prior to opening
        platform_writePin(g_lqLTEM.pinConfig.powerkeyPin, gpioValue_low);
        platform_writePin(g_lqLTEM.pinConfig.resetPin, gpioValue_low);
        platform_writePin(g_lqLTEM.pinConfig.spiCsPin, gpioValue_high);
        platform_writePin(g_lqLTEM.pinConfig.irqPin, gpioValue_high);

        platform_openPin(g_lqLTEM.pinConfig.powerkeyPin, gpioMode_output);		// powerKey: normal low
        platform_openPin(g_lqLTEM.pinConfig.resetPin, gpioMode_output);			// resetPin: normal low
        platform_openPin(g_lqLTEM.pinConfig.spiCsPin, gpioMode_output);			// spiCsPin: invert, normal gpioValue_high
        platform_openPin(g_lqLTEM.pinConfig.statusPin, gpioMode_input);
        platform_openPin(g_lqLTEM.pinConfig.irqPin, gpioMode_inputPullUp);
        lqLOG_VRBS("GPIO Configured\r\n");

        spi_start(g_lqLTEM.platformSpi);                                        // start host SPI
        lqLOG_VRBS("SPI Configured\r\n");
        g_lqLTEM.hostConfigured = true;
    }

    lqLOG_INFO("LTEm reqst resetType=%d\r\n", resetAction);
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
            QBG_reset(resetAction);                                         // do requested reset (sw, hw, pwrCycle)
        }
    }
    else 
    {
       QBG_powerOn();                                                       // turn on BGx
    }
    lqLOG_VRBS("LTEm was reset=%d\r\n", ltemWasReset);

    SC16IS7xx_start();                                                      // initialize NXP SPI-UART bridge base functions: FIFO, levels, baud, framing
    lqLOG_VRBS("UART started\r\n");
    SC16IS7xx_enableIrqMode();                                              // enable IRQ generation on SPI-UART bridge (IRQ mode)
    lqLOG_VRBS("UART set to IRQ mode\r\n");
    IOP_attachIrq();                                                        // attach I/O processor ISR to IRQ
    lqLOG_VRBS("UART IRQ attached\r\n");

    IOP_interruptCallbackISR();                                             // force ISR to run once to sync IRQ 
    g_lqLTEM.appEventNotifyEnabled = true;                                  // through the low-level actions, re-enable notifications

    lqLOG_VRBS("LTEm prior state=%d\r\n", g_lqLTEM.deviceState);

    uint32_t startRdyChk = lqMillis();                                      // wait for BGx to signal internal ready
    uint32_t appRdyAt = 0;
    uint32_t simRdyAt = 0; 
    do
    {
        if (BBFFR_ISFOUND(bbffr_find(g_lqLTEM.iop->rxBffr, "APP RDY", 0, 0, false)))
            appRdyAt = lqMillis();

        if (BBFFR_ISFOUND(bbffr_find(g_lqLTEM.iop->rxBffr, "+CPIN: READY", 0, 0, false)))
            simRdyAt = lqMillis();

        if (IS_ELAPSED(startRdyChk, APPRDY_TIMEOUT))
        {
            lqLOG_VRBS("AppRdy not received! Timeout at %dms\r\n", APPRDY_TIMEOUT);
            return false;
        }
    } while (!appRdyAt || !simRdyAt);
    
    g_lqLTEM.deviceState = deviceState_ready;
    lqLOG_INFO("ModuleReady at %dms (%d/%d)\r\n", lqMillis() - startRdyChk, appRdyAt - startRdyChk, simRdyAt - startRdyChk);
    pDelay(500);
    bbffr_reset(g_lqLTEM.iop->rxBffr);                                      // clean out start messages from RX buffer


    uint8_t initTries = 0;
    while (initTries <= 1)
    {
        if (QBG_setOptions())
            lqLOG_VRBS("BGx options set\r\n");
            if (ltem_ping())
                break;
        else
        {
            ltem_notifyApp(appEvent_fault_hardFault, "BGx set options failed");         // send notification, maybe app can recover
            lqLOG_ERROR("BGx set options failed\r");
        }
        initTries++;
    }
    lqLOG_INFO("BGx start verified\r\n");

    ntwk_applyPpdNetworkConfig();                                       // configures default PDP context for likely autostart with provider attach
    lqLOG_VRBS("ltem_start(): pdp ntwk configured\r\n");

    ntwk_awaitOperator(2);                                              // attempt to warm-up provider/PDP briefly. 
    lqLOG_VRBS("ltem_start(): provider warmed up\r\n");        // If longer duration required, leave that to application

    ltem_getModemInfo();                                                // populate modem info struct
    return true;
}


/**
 *	@brief Powers off the modem without destroying memory objects. Modem device will require ltem_start() to reinit HW.
 */
void ltem_stop()
{
    spi_stop(g_lqLTEM.platformSpi);
    IOP_stopIrq();
    g_lqLTEM.deviceState = deviceState_powerOff;
    QBG_powerOff();
}


/**
 *	@brief Performs a reset of LTEm.
 */
bool ltem_reset(bool hardReset)
{
    resetAction_t resetAction = hardReset ? resetAction_hwReset : resetAction_swReset;
    return ltem_start(resetAction);
}


/**
 *	@brief Turn modem power OFF
 */
void ltem_powerOff()
{
    QBG_powerOff();  
}


void ltem_enterPcm()
{
}


/**
 *	@brief Set RF priority on BG95/BG77 modules. 
 */
resultCode_t ltem_setRfPriorityMode(ltemRfPriorityMode_t rfMode)
{
    ASSERT(rfMode == 0 || rfMode == 1);

    lqLOG_VRBS("<ltem_setRfPriorityMode()> rfMode=%d\r\n", rfMode);
    lqLOG_VRBS("<ltem_setRfPriorityMode()> module:%s\r\n", g_lqLTEM.modemInfo->model);

    //  only applicable to single-RF modules
    if (memcmp(g_lqLTEM.modemInfo->model, "BG95", 4) != 0 && memcmp(g_lqLTEM.modemInfo->model, "BG77", 4) != 0)
        return resultCode__badRequest;

    RSLT;
    uint8_t targetLoadedState = (rfMode == ltemRfPriorityMode_wwan) ? ltemRfPriorityState_wwanLoaded : ltemRfPriorityState_gnssLoaded;
    if (targetLoadedState == ltem_getRfPriorityState())
    {
        lqLOG_WARN("RF priority already at set state.\r\n");
        return resultCode__success;                                                 // already at destination
    }

    if (rfMode == ltemRfPriorityMode_gnss)                                          // test for requesting GNSS priority but GNSS not ON
    {
        bool gnssActive = false;
        if (IS_SUCCESS_RSLT(atcmd_dispatch("AT+QGPS?")))
        {
            char * response = atcmd_getRawResponse();
            char * tkn = atcmd_getToken(0);
            lqLOG_VRBS("(ltem_setRfPriorityMode) get GPS state: response=%s, tkn=%s", response, tkn);
            lqDelay(10);
            gnssActive = strtol(atcmd_getToken(0), NULL, 10) == 1;
        }
        if (!gnssActive)
            return resultCode__badRequest;
    }

    /* Pre-checks completed
     * ----------------------------------------------------------- */
    uint32_t waitStart = lqMillis();
    for (size_t i = 0; i < 10; i++)
    {
        if (IS_NOTSUCCESS_RSLT(atcmd_dispatch("AT+QGPSCFG=\"priority\",%d", rfMode)))
        {
                return rslt;
        }
        if (targetLoadedState == ltem_getRfPriorityState())                         // check for stack loaded (state)
        {
            lqLOG_DBG(0, "RF switch took %dms\r\n", lqMillis() - waitStart);
            return resultCode__success;
        }
        lqDelay(500);
    }
    return resultCode__timeout;
}


/**
 *	@brief Get RF priority on BG95/BG77 modules. 
 */
ltemRfPriorityMode_t ltem_getRfPriorityMode()
{
    if ((memcmp(g_lqLTEM.modemInfo->model, "BG95", 4) == 0) || (memcmp(g_lqLTEM.modemInfo->model, "BG77", 4) == 0))
    {
        if (atcmd_tryInvoke("AT+QGPSCFG=\"priority\""))
        {
            if (IS_SUCCESS(atcmd_awaitResult()))
            {
                uint32_t mode = strtol(atcmd_getToken(1), NULL, 10);
                lqLOG_VRBS("<ltem_getRfPriorityMode> mode=%d\r\n", mode);
                return mode;
            }
        }
    }
    return ltemRfPriorityMode_none;
}


/**
 *	@brief Get RF priority on BG95/BG77 modules. 
 */
ltemRfPriorityState_t ltem_getRfPriorityState()
{
    if (memcmp(g_lqLTEM.modemInfo->model, "BG95", 4) == 0 || memcmp(g_lqLTEM.modemInfo->model, "BG77", 4) == 0)
    {
        RSLT;
        if (IS_SUCCESS_RSLT(atcmd_dispatch("AT+QGPSCFG=\"priority\"")))
        {
            char * response = atcmd_getResponse();
            char * tkn = atcmd_getToken(2);
            lqLOG_VRBS("(ltem_getRfPriorityState) response:%s, token:%s\r\n", response, tkn);
            lqDelay(10);

            uint32_t state = strtol(atcmd_getToken(2), NULL, 10);
            lqLOG_VRBS("<ltem_getRfPriorityState> state=%d\r\n", state);
            return state;
        }
    }
    lqLOG_VRBS("<ltem_getRfPriorityState> state=0\r\n");
    return ltemRfPriorityState_unloaded;
}


/**
 *	@brief Get the current UTC date and time.
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
            if ((dtSrc = memchr(atcmd_getResponse(), '"', 12)) != NULL)         // allowance for preceeding EOL
            {
                dtSrc++;
                if (*dtSrc != '8')                                              // test for not initialized date/time, starts with 80 (aka 1980)
                {
                    lqLOG_VRBS("ltem_getUtcDateTime(): format=%c\r\n", format);

                    if (format == 'v' || format == 'V')                         // "VERBOSE" format
                    {
                        char* tzDelimPoz = memchr(dtSrc, '+', 20);              // strip UTC offset, safe stop in trailer somewhere
                        char* tzDelimNeg = memchr(dtSrc, '-', 20);              // strip UTC offset, safe stop in trailer somewhere
                        lqLOG_VRBS("ltem_getUtcDateTime(): tzDelimPoz=%p, tzDelimNeg=%p\r\n", tzDelimPoz, tzDelimNeg);

                        vTaskDelay(100);

                        if (tzDelimPoz)
                        {
                            lqLOG_VRBS("ltem_getUtcDateTime(): tzDelimPoz=%p, offset=%d\r\n", tzDelimPoz, tzDelimPoz - dtSrc);
                            *tzDelimPoz = '\0';                                 // verbose displays local time, use ltem_getLocalTimezoneOffset() to get TZ
                            strcpy(destPtr, dtSrc);                             // safe c-string strcpy to dateTime
                        }
                        else if (tzDelimNeg)
                        {
                            lqLOG_VRBS("ltem_getUtcDateTime(): tzDelimNeg=%p, offset=%d\r\n", tzDelimNeg, tzDelimNeg - dtSrc);
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
                    lqLOG_VRBS("ltem_getUtcDateTime(): post-year: %s, len=%d\r\n", dtDbg, strlen(dtDbg));

                    destPtr += 2;
                    memcpy(destPtr, dtSrc + 3, 2);                              // month

                    lqLOG_VRBS("ltem_getUtcDateTime(): post-month: %s, len=%d\r\n", dtDbg, strlen(dtDbg));

                    destPtr += 2;
                    memcpy(destPtr, dtSrc + 6, 2);                              // day

                    lqLOG_VRBS("ltem_getUtcDateTime(): post-day: %s, len=%d\r\n", dtDbg, strlen(dtDbg));

                    destPtr += 2;
                    *destPtr = 'T';                                             // delimiter
                    destPtr += 1;

                    lqLOG_VRBS("ltem_getUtcDateTime(): post-T: %s, len=%d\r\n", dtDbg, strlen(dtDbg));

                    memcpy(destPtr, dtSrc + 9, 2);                              // hours
                    destPtr += 2;

                    lqLOG_VRBS("ltem_getUtcDateTime(): post-hours: %s, len=%d\r\n", dtDbg, strlen(dtDbg));

                    memcpy(destPtr, dtSrc + 12, 2);                       // minutes
                    destPtr += 2;

                    lqLOG_VRBS("ltem_getUtcDateTime(): post-minutes: %s, len=%d\r\n", dtDbg, strlen(dtDbg));

                    memcpy(destPtr, dtSrc + 15, 2);                       // seconds
                    destPtr += 2;

                    lqLOG_VRBS("ltem_getUtcDateTime(): post-seconds: %s, len=%d\r\n", dtDbg, strlen(dtDbg));

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
 *	@brief Get local time zone offset.
 */
int8_t ltem_getLocalTimezoneOffset(bool precise)
{
    char dateTime[30] = {0};
    char *dtSrc;

    if (atcmd_tryInvoke("AT+CCLK?"))
    {
        if (IS_SUCCESS(atcmd_awaitResult()))
        {
            if ((dtSrc = memchr(atcmd_getResponse(), '"', 12)) != NULL)        // tolerate preceeding EOL
            {
                dtSrc++;
                if (*dtSrc != '8')                                             // test for not initialized date/time, stardtSrc with 80 (aka 1980)
                {
                    char* tzDelim = memchr(dtSrc, '-', 20);                    // strip UTC offset, safe stop in trailer somewhere
                    if (tzDelim != NULL)                                    // found expected - delimeter before TZ offset
                    {
                        if (precise)
                            return strtol(tzDelim, NULL, 10);               // BGx reports 15min TZ offsets (supports 30, 45 minutes TZ offset regions)
                        else
                            return strtol(tzDelim, NULL, 10) / 4;           // BGx reports 15min TZ offsets (supports 30, 45 minutes TZ offset regions)
                    }
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
    if (atcmd_awaitLock(atcmd__defaultTimeout))
    {
        if (g_lqLTEM.modemInfo->imei[0] == 0)
        {
            atcmd_invokeReuseLock("AT+GSN");
            if (atcmd_awaitResult() == resultCode__success)
            {
                strncpy(g_lqLTEM.modemInfo->imei, atcmd_getResponse(), ntwk__imeiSz);
            }
        }

        if (g_lqLTEM.modemInfo->fwver[0] == 0)
        {
            atcmd_invokeReuseLock("AT+QGMR");
            if (atcmd_awaitResult() == resultCode__success)
            {
                char *eol;
                if ((eol = strstr(atcmd_getResponse(), "\r\n")) != NULL)
                {
                    uint8_t sz = eol - atcmd_getResponse();
                    memcpy(g_lqLTEM.modemInfo->fwver, atcmd_getResponse(), MIN(sz, ntwk__dvcFwVerSz));
                }
            }
        }

        if (g_lqLTEM.modemInfo->mfg[0] == 0)
        {
            atcmd_invokeReuseLock("ATI");
            if (atcmd_awaitResult() == resultCode__success)
            {
                char* response = atcmd_getResponse();
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
            atcmd_invokeReuseLock("AT+ICCID");
            atcmd_ovrrdParser(S__iccidCompleteParser);
            if (IS_SUCCESS(atcmd_awaitResult()))
            {
                char* delimAt;
                char* responseAt = atcmd_getResponse();
                if (strlen(responseAt) && (delimAt = memchr(responseAt, '\r', strlen(responseAt))))
                {
                    memcpy(g_lqLTEM.modemInfo->iccid, responseAt, MIN(delimAt - responseAt, ntwk__iccidSz));
                }
            }
        }
        atcmd_close();
    }
    return g_lqLTEM.modemInfo;
}


/**
 *  @brief Test for SIM ready
 */
bool ltem_isSimReady()
{
    bool cpinState = false;
    if (atcmd_tryInvoke("AT+CPIN?"))
    {
        if (atcmd_awaitResult() == resultCode__success)
        {
            cpinState = strstr(atcmd_getResponse(), "+CPIN: READY") != NULL;
        }
        atcmd_close();
    }
    return strlen(g_lqLTEM.modemInfo->iccid) > 0 && cpinState;
}


/**
 *	@brief Get the LTEmC software version.
 */
const char* ltem_getSwVersion()
{
    return LTEmC_VERSION;
}


/**
 *	@brief Get the LTEmC software version.
 */
const char* ltem_getModuleType()
{
    return g_lqLTEM.modemInfo->model;
}


/**
 *	@brief Performs a HW reset of LTEm1 and optionally executes start sequence.
 */
deviceState_t ltem_getDeviceState()
{
    lqLOG_VRBS("<ltem_getDeviceState()> prior state=%d\r\n", g_lqLTEM.deviceState);

    if (QBG_isPowerOn())             // ensure powered off device doesn't report otherwise
        g_lqLTEM.deviceState = MAX(g_lqLTEM.deviceState, deviceState_powerOn); 
    else
        g_lqLTEM.deviceState = deviceState_powerOff;

    lqLOG_VRBS("<ltem_getDeviceState()> new state=%d\r\n", g_lqLTEM.deviceState);
    return g_lqLTEM.deviceState;
}


/**
 *	@brief Test for responsive and initialized BGx.
 */
bool ltem_ping()
{
    if (IS_SUCCESS(atcmd_dispatch("ATE0")))                        // get OK response (and ensure cmd echo is OFF)
    {
        return strstr(atcmd_getRawResponse(), "\r\nOK\r\n") != NULL;
    }
    return false;
}


/**
 *	@brief Background work task runner. To be called in application Loop() periodically.
 */
void ltem_eventMgr()
{
    lqLOG_VRBS("(ltem_eventMgr) Entered...\r\n");

    if (bbffr_getOccupied(g_lqLTEM.iop->rxBffr))
    {
        /* look for a new incoming URC 
        */
        int16_t urcOffset = bbffr_find(g_lqLTEM.iop->rxBffr, "+", 0, 0, false);             // look for URC prefix char in RX buffer
        if (BBFFR_ISFOUND(urcOffset))
        {
            /* Invoke each stream's URC handler (if stream has one), it will service or return with a cancelled if not handled
            */
            for (size_t i = 0; i < ltem__streamCnt; i++)                                    // potential URC in rxBffr, see if a data handler will service
            {
                // NOTE: only MQTT and SCKT (sockets) are asynchronous and have URC handlers currently

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
    }
    lqLOG_VRBS("(ltem_eventMgr) Exiting\r\n");
}


dataCntxt_t ltem_addStream(streamCtrl_t *streamCtrl)
{
    lqLOG_VRBS("Registering Stream\r\n");

    ASSERT(streamCtrl->dataCntxt < ltem__streamCnt);
    ASSERT(g_lqLTEM.streams[streamCtrl->dataCntxt] == NULL || g_lqLTEM.streams[streamCtrl->dataCntxt]->streamType == streamCtrl->streamType);

    if (g_lqLTEM.streams[streamCtrl->dataCntxt] != streamCtrl)
    {
        g_lqLTEM.streams[streamCtrl->dataCntxt] = streamCtrl;
    }
    return streamCtrl->dataCntxt;
}


void ltem_deleteStream(streamCtrl_t *streamCtrl)
{
    lqLOG_VRBS("Deregistering Stream\r\n");

    ASSERT(streamCtrl->dataCntxt < ltem__streamCnt);
    ASSERT(streamCtrl->streamType == g_lqLTEM.streams[streamCtrl->dataCntxt]->streamType);

    g_lqLTEM.streams[streamCtrl->dataCntxt] = NULL;

    // if (prev == NULL)
    //     return;
    // ASSERT(prev->streamType == streamType);
    // for (size_t i = 0; i < ltem__streamCnt; i++)
    // {
    //     if (g_lqLTEM.streams[i]->dataCntxt == streamCtrl->dataCntxt)
    //     {
    //         ASSERT(memcmp(g_lqLTEM.streams[i], streamCtrl, sizeof(streamCtrl_t)) == 0);     // compare the common fields
    //         g_lqLTEM.streams[i] = NULL;
    //         return;
    //     }
    // }
}


streamCtrl_t* ltem_findStream(uint8_t context)
{
    return g_lqLTEM.streams[context];
}


/**
 *	@brief Notify host application of significant events. Application may ignore, display, save status, whatever. 
 */
void ltem_notifyApp(uint8_t notifyType, const char *notifyMsg)
{
    if (g_lqLTEM.appEventNotifyEnabled && g_lqLTEM.appEvntNotifyCB != NULL)                                       
        (g_lqLTEM.appEvntNotifyCB)(notifyType, notifyMsg);                                // if app handler registered, it may/may not return
}


/**
 *	@brief Registers the address (void*) of your application event nofication callback handler.
 */
void ltem_setEventNotifCallback(appEvntNotify_func eventNotifCallback)
{
    g_lqLTEM.appEventNotifyEnabled = true;
    g_lqLTEM.appEvntNotifyCB = eventNotifCallback;
}

/**
 *	@brief Registers the address (void*) of your application yield callback handler.
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

    bBuffer_t *rxBffr = g_lqLTEM.iop->rxBffr;                                   // for convenience

    if (BBFFR_ISFOUND(bbffr_find(rxBffr, "+Q", 0, 0, false)))                   // Quectel URC prefix
    {
        lqLOG_INFO("Quectel URC received\r\n");

        /* PDP (packet network) deactivation/close
        ------------------------------------------------------------------------------------------- */
        if (bbffr_find(rxBffr, "+QIURC: \"pdpdeact\"", 0, 0, true) >= 0)
        {
            lqLOG_INFO("PDP deactivation reported\r\n");

            if (!ntwk_awaitOperator(0))                                             // update network operator
            {
                for (size_t i = 0; i < dataCntxt__cnt; i++)                      // future close streams
                {
                    if (g_lqLTEM.streams[i]->closeStreamCB)
                    {
                        g_lqLTEM.streams[i]->closeStreamCB(i);
                    }
                }
            }
        }
    }

    else if (BBFFR_ISFOUND(bbffr_find(rxBffr, "+C", 0, 0, false)))              // CCITT URC prefixes
    {
        lqLOG_INFO("CCITT URC received\r\n");
    }
}


/**
 *	@brief Action response parser for iccid value request.
 */
static cmdParseRslt_t S__iccidCompleteParser(ltemDevice_t *modem)
{
    return atcmd_stdResponseParser("+ICCID: ", true, "", 0, 0, "\r\n\r\nOK\r\n", 20);
}



#pragma endregion
