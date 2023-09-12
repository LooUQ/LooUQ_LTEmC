/** ****************************************************************************
  \file 
  \brief LTEmC LTEm Device Driver for LooUQ LTEm Series Modems
  \author Greg Terrell, LooUQ Incorporated

  \loouq

--------------------------------------------------------------------------------

    This project is released under the GPL-3.0 License.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 
***************************************************************************** */

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


/* Static Function Declarations
------------------------------------------------------------------------------------------------ */
void S__initLTEmDevice(bool ltemReset);


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

    IOP_create();
    
    g_lqLTEM.atcmd = calloc(1, sizeof(atcmd_t));
    ASSERT(g_lqLTEM.atcmd != NULL);
    atcmd_reset(true);

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
void ltem_start(resetAction_t resetAction)
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

    spi_start(g_lqLTEM.platformSpi);                                        // start host SPI

    bool ltemReset = true;
    if (QBG_isPowerOn())
    {
        if (resetAction == resetAction_skipIfOn)
            ltemReset = false;
        else
        {
            if (resetAction == resetAction_swReset)
            {
                if (!SC16IS7xx_isAvailable())                               // fall back to power reset if UART not available
                    resetAction = resetAction_powerReset;
            }
            QBG_reset(resetAction);                                         // do requested reset (sw, hw, pwrCycle)
        }
    }
    else 
    {
       QBG_powerOn();                                                       // turn on BGx
    }
    S__initLTEmDevice(ltemReset);
}


/**
 *	@brief Static internal BGx initialization logic shared between start and reset
 */
void S__initLTEmDevice(bool ltemReset)
{
    ASSERT(QBG_isPowerOn());
    ASSERT(SC16IS7xx_isAvailable());

    SC16IS7xx_start();                                                  // initialize NXP SPI-UART bridge base functions: FIFO, levels, baud, framing
    if (ltemReset)
    {
        if (IOP_awaitAppReady())
        {
            DPRINT(PRNT_INFO, "AppRdy recv'd\r\n");
        }
        else
        {
            if (g_lqLTEM.deviceState == deviceState_powerOn)
            {
                DPRINT(PRNT_WARN, "AppRdy timeout\r\n");
                g_lqLTEM.deviceState = deviceState_appReady;            // missed it somehow
            }
        }
    }
    else
    {
        g_lqLTEM.deviceState = deviceState_appReady;                    // assume device state = appReady, APP RDY sent in 1st ~10 seconds of BGx running
        DPRINT(PRNT_INFO, "LTEm ON (AppRdy)\r\n");
    }

    SC16IS7xx_enableIrqMode();                                          // enable IRQ generation on SPI-UART bridge (IRQ mode)
    DPRINT_V(PRNT_CYAN, "S__initLTEmDevice(): nxp in irdmode");

    IOP_attachIrq();                                                    // attach I/O processor ISR to IRQ
    DPRINT_V(PRNT_CYAN, "S__initLTEmDevice(): irq attached");

    if (!QBG_setOptions())
    {
        ltem_notifyApp(appEvent_fault_hardFault, "BGx init cmd fault"); // send notification, maybe app can recover
        DPRINT(PRNT_DEFAULT, "\r");
    }
    else
        DPRINT_V(PRNT_CYAN, "S__initLTEmDevice(): bgx options set");

    // ntwk_setRatOptions();                                            // initialize BGx Radio Access Technology (RAT) options
    // DPRINT_V(PRNT_CYAN, "S__initLTEmDevice(): rat options set");

    ntwk_applyPpdNetworkConfig();                                       // configures default PDP context for likely autostart with provider attach
    DPRINT_V(PRNT_CYAN, "S__initLTEmDevice(): pdp ntwk configured");

    ntwk_awaitProvider(2);                                              // attempt to warm-up provider/PDP briefly. 
    DPRINT_V(PRNT_CYAN, "S__initLTEmDevice(): provider warmed up");     // If longer duration required, leave that to application
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
void ltem_reset(bool hardReset)
{
    QBG_reset(hardReset);                                           // reset module, indirectly SPI/UART (CFUNC or reset pin)
    S__initLTEmDevice(false);
}


/**
 *	@brief Set RF priority on BG95/BG77 modules. 
 */
resultCode_t ltem_setRfPriority(ltemRfPrioritySet_t priority)
{
    if (lq_strnstr(ltem_getModuleType(), "BG95", 40) ||
        lq_strnstr(ltem_getModuleType(), "BG77", 40))
    {
        if (atcmd_tryInvoke("AT+QGPSCFG=\"priority\",%d", priority))
        {
            return atcmd_awaitResult();
        }
        return resultCode__conflict;
    }
    return resultCode__preConditionFailed;
}


/**
 *	@brief Get RF priority on BG95/BG77 modules. 
 */
ltemRfPriorityState_t ltem_getRfPriority()
{
    if (lq_strnstr(ltem_getModuleType(), "BG95", 40) ||
        lq_strnstr(ltem_getModuleType(), "BG77", 40))
    {
        if (atcmd_tryInvoke("AT+QGPSCFG=\"priority\""))
        {
            if (atcmd_awaitResult() == resultCode__success)
            {
                return atcmd_getValue();
            }
        }
        return 99;
    }
    return 99;
}


/**
 *	@brief Get the current UTC date and time.
 */
void ltem_getDateTimeUtc(char *dateTime)
{
    char* ts;
    uint8_t len;
    *dateTime = '\0';                                                       // dateTime is empty c-string now

    if (dateTime != NULL && atcmd_tryInvoke("AT+CCLK?"))
    {
        if (atcmd_awaitResult() == resultCode__success)
        {
            if ((ts = memchr(atcmd_getResponse(), '"', 12)) != NULL)        // allowance for preceeding EOL
            {
                ts++;
                if (*ts != '8')                                             // test for not initialized date/time, starts with 80 (aka 1980)
                {
                    char* stop = memchr(ts, '-', 20);                       // strip UTC offset, safe stop in trailer somewhere
                    if (stop != NULL)                                       // found expected - delimeter before TZ offset
                    {
                        *stop = '\0';
                        strcpy(dateTime, ts);                               // safe strcpy to dateTime
                        return;
                    }
                }
            }
        }
    }
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
    if (strlen(g_lqLTEM.modemInfo->model) == 0)
    {
        mdminfo_ltem();
    }
    return g_lqLTEM.modemInfo->model;
}


/**
 *	@brief Performs a HW reset of LTEm1 and optionally executes start sequence.
 */
deviceState_t ltem_getDeviceState()
{
    if (QBG_isPowerOn())             // ensure powered off device doesn't report otherwise
        g_lqLTEM.deviceState = MAX(g_lqLTEM.deviceState, deviceState_powerOn); 
    else
        g_lqLTEM.deviceState = deviceState_powerOff;

    return g_lqLTEM.deviceState;
}


/**
 *	@brief Test for responsive BGx.
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
 *	@brief Background work task runner. To be called in application Loop() periodically.
 */
void ltem_eventMgr()
{
    /* look for a new incoming URC 
     */
    int16_t urcPossible = bbffr_find(g_lqLTEM.iop->rxBffr, "+", 0, 0, false);       // look for prefix char in URC
    if (BBFFR_NOTFOUND(urcPossible))
    {
        return;
    }

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

    // S__ltemUrcHandler();                                                            // always invoke system level URC validation/service
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
 *	@brief Notify host application of significant events. Application may ignore, display, save status, whatever. 
 */
void ltem_notifyApp(uint8_t notifyType, const char *notifyMsg)
{
    if (g_lqLTEM.appEvntNotifyCB != NULL)                                       
        (g_lqLTEM.appEvntNotifyCB)(notifyType, notifyMsg);                                // if app handler registered, it may/may not return
}


/**
 *	@brief Registers the address (void*) of your application event nofication callback handler.
 */
void ltem_setEventNotifCallback(appEvntNotify_func eventNotifCallback)
{
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

#pragma region Static Function Definitions

/**
 * @brief Global URC handler
 * @details Services URC events that are not specific to a stream/protocol
 */
void S__ltemUrcHandler()                                                     
{
    bBuffer_t *rxBffr = g_lqLTEM.iop->rxBffr;                           // for convenience
    char parseBffr[30];

    // bool isQuectel = cbffr_find(rxBffr, "+Q", 0, 0, false) != CBFFR_NOFIND;
    // bool isCCITT = cbffr_find(rxBffr, "+C", 0, 0, false) != CBFFR_NOFIND;

    /* LTEm System URCs Handled Here
     *
     * +QIURC: "pdpdeact",<contextID>
    */

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

#pragma endregion
