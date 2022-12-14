/******************************************************************************
 *  \file ltemc.c
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020,2021 LooUQ Incorporated.
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
 *************************************************************************** */
const char *ltemcVersion = "3.0.1";
/* ************************************************************************* */

#define _DEBUG 2                        // set to non-zero value for PRINTF debugging output, 
// debugging output options             // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if defined(_DEBUG) && _DEBUG > 0
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG == 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #define PRINTF(c_,f_,__VA_ARGS__...) do { rtt_printf(c_, (f_), ## __VA_ARGS__); } while(0)
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#endif

#include "ltemc-internal.h"


#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))


/* ------------------------------------------------------------------------------------------------
 * GLOBAL LTEm Device Objects, One LTEmX supported
 * --------------------------------------------------------------------------------------------- */
ltemDevice_t g_lqLTEM;


/* BGx module initialization commands (start script)
 * ---------------------------------------------------------------------------------------------
 * used in ltemc-quectel-bg.c for module initialization, declared here for convenience
 * LTEmC requires that no-echo is there, append any ADDITIONAL global module setting command in the list.
 * Ex: Radio setup (RAT search, IoT mode, etc.) 
 * ------------------------------------------------------------------------------------------------ */
const char* const qbg_initCmds[] = 
{ 
    "ATE0",                 // don't echo AT commands on serial
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
void ltem_create(const ltemPinConfig_t ltem_config, yield_func yieldCallback, appEventCallback_func eventNotifCallback)
{
    ASSERT(g_lqLTEM.atcmd == NULL, srcfile_ltemc_ltemc_c);                    // prevent multiple calls, memory leak calloc()

	g_lqLTEM.pinConfig = ltem_config;
    g_lqLTEM.spi = spi_create(g_lqLTEM.pinConfig.spiCsPin);

    g_lqLTEM.modemSettings =  calloc(1, sizeof(modemSettings_t));
    ASSERT(g_lqLTEM.modemSettings != NULL, srcfile_ltemc_ltemc_c);

    g_lqLTEM.modemInfo = calloc(1, sizeof(modemInfo_t));
    ASSERT(g_lqLTEM.modemInfo != NULL, srcfile_ltemc_ltemc_c);

    IOP_create();
    
    g_lqLTEM.atcmd = calloc(1, sizeof(atcmd_t));
    ASSERT(g_lqLTEM.atcmd != NULL, srcfile_ltemc_ltemc_c);
    atcmd_reset(true);

    ntwk_create();

    g_lqLTEM.cancellationRequest = false;
    g_lqLTEM.appEventCB = eventNotifCallback;
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
    strcpy(g_lqLTEM.modemSettings->scanSequence, scanSequence);
    if (ltem_getDeviceState() == deviceState_appReady)
    {
        atcmd_tryInvoke("AT+QCFG=\"nwscanseq\",%s", scanSequence);
        atcmd_awaitResult();
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
    ASSERT(QBG_isPowerOn(), srcfile_ltemc_ltemc_c);
    ASSERT(SC16IS7xx_isAvailable(), srcfile_ltemc_ltemc_c);

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
        PRINTF(dbgColor__info, "AppRdy LTEm ON\r");
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
    return ltemcVersion;
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
void ltem_doWork()
{
    if (ltem_getDeviceState() != deviceState_appReady)
        ltem_notifyApp(appEvent_fault_hardFault, "LTEm I/O Error");

    for (size_t i = 0; i < sizeof(g_lqLTEM.streamWorkers) / sizeof(doWork_func); i++)  // each stream with a doWork() register it at OPEN (removed if last CLOSE)
    {
        if (g_lqLTEM.streamWorkers[i] != NULL)
            (g_lqLTEM.streamWorkers[i])();
    }
}


/**
 *	@brief Function of last resort, catastrophic failure Background work task runner. To be called in application Loop() periodically.
 */
void ltem_notifyApp(uint8_t notifyType, const char *notifyMsg)
{
    if (g_lqLTEM.appEventCB != NULL)                                       
        (g_lqLTEM.appEventCB)(notifyType, notifyMsg);                                // if app handler registered, it may/may not return
}


/**
 *	@brief Registers the address (void*) of your application event nofication callback handler.
 * 
 *  @param eventNotifCallback [in] Callback function in application code to be invoked when LTEmC is in await section.
 */
void ltem_setEventNotifCallback(appEventCallback_func eventNotifCallback)
{
    g_lqLTEM.appEventCB = eventNotifCallback;
}

/**
 *	@brief Registers the address (void*) of your application yield callback handler.
 * 
 *  @param yieldCallback [in] Callback function in application code to be invoked when LTEmC is in await section.
 */
void ltem_setYieldCallback(platform_yieldCB_func_t yieldCallback)
{
    platform_yieldCB_func = yieldCallback;
}


#pragma endregion


#pragma region LTEmC Internal Functions (ltemc-internal.h)
/*-----------------------------------------------------------------------------------------------*/

void LTEM_registerDoWorker(doWork_func *doWorker)
{
    bool found = false;
    for (size_t i = 0; i < sizeof(g_lqLTEM.streamWorkers) / sizeof(doWork_func); i++)     // wireup sckt_doWork into g_ltemc worker array
    {
        if (g_lqLTEM.streamWorkers[i] == doWorker)
        {
            found = true;
            break;
        }
    }
    if (!found)
    {
        for (size_t i = 0; i < sizeof(g_lqLTEM.streamWorkers); i++)           // not there, find and empty slot
        {
            if (g_lqLTEM.streamWorkers[i] == NULL)
            {
                g_lqLTEM.streamWorkers[i] = doWorker;
                break;
            }
        }
    }
}


#pragma endregion

#pragma region Static Function Definitions
#pragma endregion
