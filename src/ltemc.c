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

/* ------------------------------------------------------------------------------------------------
 * GLOBAL LTEm1 Device Objects, One LTEmX supported
 * --------------------------------------------------------------------------------------------- */
/* ltemDevice_t *g_ltem;

   ltemPinConfig_t g_ltemPinConfig;         ///< GPIO pin configuration for required GPIO and SPI interfacing.
   spiDevice_t *g_ltemSpiPtr;               ///< SPI device (methods signatures compatible with Arduino).
   qbgReadyState_t g_ltemQbgReadyState;     ///< Ready state of the BGx module
   appNotifyFunc_t g_ltemAppNotifyCB;       ///< Notification callback to application
   uint8_t g_ltemPdpContext;                ///< The primary packet data protocol (PDP) context with the network carrier for application transfers.
   volatile iop_t *g_ltemIopPtr;            ///< IOP subsystem controls.
   atcmd_t *g_ltemAtcmdPtr;                 ///< Action subsystem controls.
   modemInfo_t *g_ltemModemInfoPtr;         ///< Data structure holding persistent information about application modem state.
   network_t *g_ltemNetworkPtr;             ///< Data structure representing the cellular network.
   bool g_ltemCancellationRequest;          ///< For RTOS implementations, token to request cancellation of long running task/action.
*/

// global device object
ltemDevice_t g_ltem;


/* Static Function Declarations
------------------------------------------------------------------------------------------------ */
void S__initDevice(bool foundOn);


#pragma region Public Functions
/*-----------------------------------------------------------------------------------------------*/

/**
 *	@brief Get the LTEmC software version.
 */
const char *ltem_ltemcVersion()
{
    return ltemcVersion;
}


/**
 *	@brief Initialize the LTEm1 modem.
 */
void ltem_create(const ltemPinConfig_t ltem_config, yield_func yieldCallback, appEventCallback_func eventNotifCallback)
{
    ASSERT(g_ltem.modemInfo == NULL, srcfile_ltemc_ltemc_c);

	g_ltem.pinConfig = ltem_config;
    g_ltem.spi = spi_create(g_ltem.pinConfig.spiCsPin);

    g_ltem.modemInfo = calloc(1, sizeof(modemInfo_t));
    ASSERT(g_ltem.modemInfo != NULL, srcfile_ltemc_ltemc_c);
    g_ltem.pdpContext = 1;

    IOP_create();
    g_ltem.atcmd = calloc(1, sizeof(atcmd_t));
    atcmd_setOptions(atcmd__defaultTimeoutMS, atcmd_okResultParser);
    ATCMD_reset(true);
    g_ltem.cancellationRequest = false;

    ntwk_create();
    g_ltem.appEventCB = eventNotifCallback;
}



/**
 *	@brief Uninitialize the LTEm device structures.
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
 *	@brief Start the modem.
 */
bool ltem_start(bool forceReset)
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

    spi_start(g_ltem.spi);                                              // start host SPI

    bool foundOn = qbg_isPowerOn();
    if (foundOn)
    {
        if (forceReset)
        {
            SC16IS7xx_start(false);                             // SPI-UART bridge functionality may/may not be running, init base functions: FIFO, levels, baud, framing (IRQ not req'd)
            qbg_reset(true);                                    // do hardware reset (aka power cycle)
        }
    }
    else 
    {
        qbg_powerOn();                                          // turn on BGx
    }
    S__initDevice(foundOn);
    return !foundOn;
}


/**
 *	@brief Static internal BGx initialization logic shared between start and reset
 */
void S__initDevice(bool foundOn)
{
    if (qbg_isPowerOn())                                    // ensure BGx powered on
    {
        SC16IS7xx_start();                                  // initialize NXP SPI-UART bridge base functions: FIFO, levels, baud, framing
        if (!foundOn)                                   
            IOP_awaitAppReady();                            // wait for BGx to signal out firmware ready (URC message)

        IOP_attachIrq();                                    // attach I/O processor ISR to IRQ
        SC16IS7xx_enableIrqMode();                          // enable IRQ generation on SPI-UART bridge (IRQ mode)

        qbg_setOptions();                                   // initialize BGx operating settings
        g_ltem.qbgDeviceState = qbgDeviceState_appReady;    // set device state
    }
}


/**
 *	@brief Powers off the modem without destroying memory objects. Modem device will require ltem_start() to reinit HW.
 */
void ltem_stop()
{
    spi_stop(g_ltem.spi);
    IOP_stopIrq();
    g_ltem.qbgDeviceState = qbgDeviceState_powerOff;
    qbg_powerOff();
}


/**
 *	@brief Performs a reset of LTEm.
 */
void ltem_reset(bool hardReset)
{
    qbg_reset(hardReset);                                       // reset module, indirectly SPI/UART (CFUNC or reset pin)
    S__initDevice(!hardReset);
}


/**
 *	@brief Performs a HW reset of LTEm1 and optionally executes start sequence.
 */
qbgDeviceState_t ltem_readDeviceState()
{
    if (gpio_readPin(g_ltem.pinConfig.statusPin))                  // ensure powered off device doesn't report otherwise
    {
        if (g_ltem.qbgDeviceState == qbgDeviceState_powerOff)
            g_ltem.qbgDeviceState = qbgDeviceState_powerOn; 
    }
    else
        g_ltem.qbgDeviceState = qbgDeviceState_powerOff;

    return g_ltem.qbgDeviceState;
}


/**
 *	@brief Background work task runner. To be called in application Loop() periodically.
 */
void ltem_doWork()
{
    if (!ltem_readDeviceState())
        ltem_notifyApp(appEvent_fault_hardFault, "LTEm I/O Error");

    for (size_t i = 0; i < sizeof(g_ltem.streamWorkers) / sizeof(moduleDoWorkFunc_t); i++)  // each stream with a doWork() register it at OPEN (removed if last CLOSE)
    {
        if (g_ltem.streamWorkers[i] != NULL)
            (g_ltem.streamWorkers[i])();
    }
}


/**
 *	@brief Function of last resort, catastrophic failure Background work task runner. To be called in application Loop() periodically.
 */
void ltem_notifyApp(uint8_t notifyType, const char *notifyMsg)
{
    if (g_ltem.appEventCB != NULL)                                       
        (g_ltem.appEventCB)(notifyType, notifyMsg);                                // if app handler registered, it may/may not return
}


/**
 *	@brief Registers the address (void*) of your application event nofication callback handler.
 * 
 *  @param eventNotifCallback [in] Callback function in application code to be invoked when LTEmC is in await section.
 */
void ltem_setEventNotifCallback(appEventCallback_func eventNotifCallback)
{
    g_ltem.appEventCB = eventNotifCallback;
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
#pragma endregion
