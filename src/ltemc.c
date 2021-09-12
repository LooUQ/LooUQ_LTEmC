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
 ******************************************************************************
 *
 *****************************************************************************/


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

/* ------------------------------------------------------------------------------------------------
 * GLOBAL LTEm1 Device Objects, One LTEmX supported
 * --------------------------------------------------------------------------------------------- */
// ltemDevice_t *g_ltem;

// ltemPinConfig_t g_ltemPinConfig;         ///< GPIO pin configuration for required GPIO and SPI interfacing.
// spiDevice_t *g_ltemSpiPtr;               ///< SPI device (methods signatures compatible with Arduino).
// qbgReadyState_t g_ltemQbgReadyState;     ///< Ready state of the BGx module
// appNotifyFunc_t g_ltemAppNotifyCB;       ///< Notification callback to application
// uint8_t g_ltemPdpContext;                ///< The primary packet data protocol (PDP) context with the network carrier for application transfers.
// volatile iop_t *g_ltemIopPtr;            ///< IOP subsystem controls.
// atcmd_t *g_ltemAtcmdPtr;                 ///< Action subsystem controls.
// modemInfo_t *g_ltemModemInfoPtr;         ///< Data structure holding persistent information about application modem state.
// network_t *g_ltemNetworkPtr;             ///< Data structure representing the cellular network.
// bool g_ltemCancellationRequest;          ///< For RTOS implementations, token to request cancellation of long running task/action.

ltemDevice_t g_ltem;

/* Static Function Declarations
------------------------------------------------------------------------------------------------ */


#pragma region Public Functions
/*-----------------------------------------------------------------------------------------------*/

/**
 *	\brief Initialize the LTEm1 modem.
 *
 *	\param ltem_config [in] - The LTE modem gpio pin configuration.
 *  \param appNotifyCB [in] - If supplied (not NULL), this function will be invoked for significant LTEm1 events.
 */
void ltem_create(const ltemPinConfig_t ltem_config, appNotifyFunc_t appNotifyCB)
{
	// g_ltem = calloc(1, sizeof(ltemDevice_t));
	// if (g_ltem == NULL)
	// {
    //     if (appNotifyCB != NULL)                                       
    //         appNotifyCB(ltemNotifType_memoryAllocFault, "ltem1-could not alloc ltem1 object");
	// }

	g_ltem.pinConfig = ltem_config;
    g_ltem.spi = spi_create(g_ltem.pinConfig.spiCsPin);
    //g_ltem->spi = createSpiConfig(g_ltem->gpio->spiCsPin, LTEM1_SPI_DATARATE, BG96_BAUDRATE_DEFAULT);

    g_ltem.modemInfo = calloc(1, sizeof(modemInfo_t));
    ASSERT(g_ltem.modemInfo != NULL, srcfile_ltemc_c);
    g_ltem.pdpContext = 1;

    IOP_create();
    g_ltem.atcmd = calloc(1, sizeof(atcmd_t));
    atcmd_setOptions(atcmd__setLockModeAuto, ATCMD__defaultTimeoutMS, atcmd_okResultParser);
    atcmd_reset(true);
    g_ltem.cancellationRequest = false;

    ntwk_create();
    g_ltem.appNotifyCB = appNotifyCB;
}



/**
 *	\brief Power on and start the modem.
 */
void ltem_start()
{
  	// on Arduino compatible, ensure pin is in default "logical" state prior to opening
	gpio_writePin(g_ltem.pinConfig.powerkeyPin, gpioValue_low);
	gpio_writePin(g_ltem.pinConfig.resetPin, gpioValue_low);
	gpio_writePin(g_ltem.pinConfig.spiCsPin, gpioValue_high);
	gpio_writePin(g_ltem.pinConfig.irqPin, gpioValue_high);

	gpio_openPin(g_ltem.pinConfig.powerkeyPin, gpioMode_output);		// powerKey: normal low
	gpio_openPin(g_ltem.pinConfig.resetPin, gpioMode_output);			// resetPin: normal low
	gpio_openPin(g_ltem.pinConfig.spiCsPin, gpioMode_output);			// spiCsPin: invert, normal gpioValue_high
	gpio_openPin(g_ltem.pinConfig.statusPin, gpioMode_input);
	gpio_openPin(g_ltem.pinConfig.irqPin, gpioMode_inputPullUp);

    bool poweredPrevious = qbg_isPowerOn();
    qbg_powerOn();

    if (qbg_isPowerOn())
    {
        spi_start(g_ltem.spi);
        SC16IS741A_start();                                     // start (resets previously powered on) NXP SPI-UART bridge
        IOP_start();
        if (!poweredPrevious)
            IOP_awaitAppReady();                                // wait for BGx to signal out firmware ready
        qbg_setOptions();                                       // initialize BGx operating settings
    }
    g_ltem.qbgReadyState = qbg_readyState_appReady;             // if already "ON", assume running
}


/**
 *	\brief Powers off the modem without destroying memory objects. Modem device will require ltem_start() to reinit HW.
 */
void ltem_stop()
{
    spi_stop(g_ltem.spi);
    IOP_stopIrq();
    g_ltem.qbgReadyState = qbg_readyState_powerOff;
    qbg_powerOff();
}


/**
 *	\brief Performs a HW reset of LTEm1.
 */
void ltem_reset()
{
    ltem_stop();
    ltem_start();
}


/**
 *	\brief Check the BGx for hardware ready (status pin).
 *
 *  \return True is status HIGH, hardware ready.
 */
bool ltem_chkHwReady()
{
	return gpio_readPin(g_ltem.pinConfig.statusPin);
}



/**
 *	\brief Performs a HW reset of LTEm1 and optionally executes start sequence.
 */
qbgReadyState_t ltem_getReadyState()
{
    return g_ltem.qbgReadyState;
}



/**
 *	\brief Uninitialize the LTE modem.
 */
void ltem_destroy()
{
	ltem_stop();

	gpio_pinClose(g_ltem.pinConfig.irqPin);
	gpio_pinClose(g_ltem.pinConfig.powerkeyPin);
	gpio_pinClose(g_ltem.pinConfig.resetPin);
	gpio_pinClose(g_ltem.pinConfig.statusPin);

    ip_destroy();
    free(g_ltem.atcmd);
    iop_destroy();
    spi_destroy(g_ltem.spi);
}



/**
 *	\brief Background work task runner. To be called in application Loop() periodically.
 */
void ltem_doWork()
{
    if (!ltem_chkHwReady())
        ltem_notifyApp(lqNotifType_lqDevice_hwFault, "LTEm1 I/O Error");

    for (size_t i = 0; i < sizeof(g_ltem.streamWorkers) / sizeof(moduleDoWorkFunc_t); i++)  // each stream with a doWork() register it at OPEN (removed if last CLOSE)
    {
        if (g_ltem.streamWorkers[i] != NULL)
            (g_ltem.streamWorkers[i])();
    }
}


/**
 *	\brief Function of last resort, catastrophic failure Background work task runner. To be called in application Loop() periodically.
 * 
 *  \param notifyType [in] - Enum of broad notification categories.
 *  \param notifyMsg [in] - Message from origination about the issue being reported.
 */
void ltem_notifyApp(uint8_t notifyType, const char *notifyMsg)
{
    if (g_ltem.appNotifyCB != NULL)                                       
        (g_ltem.appNotifyCB)(notifyType, notifyMsg);                                // if app handler registered, it may/may not return
}



/**
 *	\brief Registers the address (void*) of your application yield callback handler.
 * 
 *  \param yieldCb_func [in] Callback function in application code to be invoked when LTEmC is in await section.
 */
void ltem_setYieldCb(platform_yieldCB_func_t yieldCb_func)
{
    platform_yieldCB_func = yieldCb_func;
}

#pragma endregion

#pragma region LTEmC Internal Functions
/*-----------------------------------------------------------------------------------------------*/

void LTEM_registerDoWorker(moduleDoWorkFunc_t *doWorker)
{
    bool found = false;
    for (size_t i = 0; i < sizeof(g_ltem.streamWorkers) / sizeof(moduleDoWorkFunc_t); i++)     // wireup sckt_doWork into g_ltemc worker array
    {
        if (g_ltem.streamWorkers[i] == doWorker)
        {
            found = true;
            break;
        }
    }
    if (!found)
    {
        for (size_t i = 0; i < sizeof(g_ltem.streamWorkers); i++)           // not there, find and empty slot
        {
            if (g_ltem.streamWorkers[i] == NULL)
            {
                g_ltem.streamWorkers[i] = doWorker;
                break;
            }
        }
    }
}


#pragma endregion


#pragma region Static Function Definitions

/**
 *	\brief Initialize the modems IO.
 *
 *	\param[in] modem The LTE modem.
 */
static void S_initIo()
{
}

#pragma endregion
