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


#define _DEBUG 2                                    // set to non-zero value for PRINTF debugging output, 
// debugging output options                         // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if defined(_DEBUG) && _DEBUG > 0
    asm(".global _printf_float");                   // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                            // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG == 2
    #include <jlinkRtt.h>                       // PRINTF debug macro output to J-Link RTT channel
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#endif

#define SRCFILE "LTE"                               // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT
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
    "ATE0",                                         // don't echo AT commands on serial
    "AT+QURCCFG=\"urcport\",\"uart1\""              // URC events are reported to UART1
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

	g_lqLTEM.pinConfig = ltem_config;
    g_lqLTEM.spi = spi_create(g_lqLTEM.pinConfig.spiCsPin);

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
    spi_destroy(g_lqLTEM.spi);
}



/**
 *  \brief Configure RAT searching sequence
*/
void ltem_setProviderScanSeq(const char* scanSequence)
{
    /*AT+QCFG="nwscanseq"[,<scanseq>[,effect]]
    */
    if (strlen(scanSequence) > 0)
    {
        strcpy(g_lqLTEM.modemSettings->scanSequence, scanSequence);
        if (ltem_getDeviceState() == deviceState_appReady)
        {
            atcmd_tryInvoke("AT+QCFG=\"nwscanseq\",%s", scanSequence);
            atcmd_awaitResult();
        }
    }
}


/** 
 *  \brief Configure RAT(s) allowed to be searched.
*/
void ltem_setProviderScanMode(ntwkScanMode_t scanMode)
{
    /* AT+QCFG="nwscanmode"[,<scanmode>[,<effect>]]
    */
    g_lqLTEM.modemSettings->scanMode = scanMode; 
    if (ltem_getDeviceState() == deviceState_appReady)
    {
        atcmd_tryInvoke("AT+QCFG=\"nwscanmode\",%d", scanMode);
        atcmd_awaitResult();
    }
}


/** 
 *  \brief Configure the network category to be searched under LTE RAT.
 */
void ltem_setIotMode(ntwkIotMode_t iotMode)
{
    /* AT+QCFG="iotopmode",<mode>
    */
    g_lqLTEM.modemSettings->iotMode = iotMode; 
    if (ltem_getDeviceState() == deviceState_appReady)
    {
        atcmd_tryInvoke("AT+QCFG=\"iotopmode\",%d", iotMode);
        atcmd_awaitResult();
    }
}


/**
 *	\brief Build default data context configuration for modem to use on startup.
 */
void ltem_setDefaultNetwork(uint8_t pdpContextId, const char *protoType, const char *apn)
{
    ntwk_setDefaulNetworkConfig(pdpContextId, protoType, apn);
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

    spi_start(g_lqLTEM.spi);                                                // start host SPI

    bool ltemReset = true;
    if (QBG_isPowerOn())
    {
        if (resetAction == resetAction_skipIfOn)
            ltemReset = false;
        else
        {
            if (resetAction == resetAction_swReset)
            {
                if (!SC16IS7xx_isAvailable())
                    resetAction = resetAction_powerReset;
                SC16IS7xx_start();                                          // NXP SPI-UART bridge may/may not be baseline operational, initialize base: baud, framing
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

    SC16IS7xx_start();                                      // initialize NXP SPI-UART bridge base functions: FIFO, levels, baud, framing

    if (ltemReset)
    {
        if (IOP_awaitAppReady())
        {
            PRINTF(dbgColor__info, "AppRdy recv'd\r");
        }
        else
        {
            if (g_lqLTEM.deviceState == deviceState_powerOn)
            {
                PRINTF(dbgColor__warn, "AppRdy timeout\r");
                g_lqLTEM.deviceState = deviceState_appReady;        // missed it somehow
            }

        }
    }
    else
    {
        g_lqLTEM.deviceState = deviceState_appReady;        // assume device state = appReady, APP RDY sent in 1st ~10 seconds of BGx running
        PRINTF(dbgColor__info, "LTEm ON (AppRdy)\r");
    }

    IOP_attachIrq();                                        // attach I/O processor ISR to IRQ
    SC16IS7xx_enableIrqMode();                              // enable IRQ generation on SPI-UART bridge (IRQ mode)
    QBG_setOptions();                                       // initialize BGx operating settings
    NTWK_initRatOptions();                                  // initialize BGx Radio Access Technology (RAT) options
    NTWK_applyDefaulNetwork();                              // configures default PDP context for likely autostart with provider attach
    ntwk_awaitProvider(2);                                  // attempt to warm-up provider/PDP briefly. If longer duration required, leave that to application
}


/**
 *	@brief Powers off the modem without destroying memory objects. Modem device will require ltem_start() to reinit HW.
 */
void ltem_stop()
{
    spi_stop(g_lqLTEM.spi);
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
 *	@brief Get the LTEmC software version.
 */
const char *ltem_getSwVersion()
{
    return LTEmC_VERSION;
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
 *	@brief Background work task runner. To be called in application Loop() periodically.
 */
void ltem_eventMgr()
{
    /* look for a new incoming URC 
     */
    int16_t urcPossible = cbffr_find(g_lqLTEM.iop->rxBffr, "+", 0, 0, false);       // look for prefix char in URC
    if (CBFFR_NOTFOUND(urcPossible))
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
    ASSERT(ltem_getStreamFromCntxt(streamCtrl->dataCntxt, 0) == NULL);          // assert that a stream for context has not previously been added to streams table

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
        if (g_lqLTEM.streams[i]->dataCntxt == context)
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
    cBuffer_t *rxBffr = g_lqLTEM.iop->rxBffr;                           // for convenience
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
