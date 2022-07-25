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
ltemDevice_t g_lqLTEM;

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
    ASSERT(g_lqLTEM.atcmd == NULL, srcfile_ltemc_ltemc_c);                    // prevent multiple calls, memory leak calloc()

	g_lqLTEM.pinConfig = ltem_config;
    g_lqLTEM.spi = spi_create(g_lqLTEM.pinConfig.spiCsPin);

    g_lqLTEM.modemInfo = calloc(1, sizeof(modemInfo_t));
    ASSERT(g_lqLTEM.modemInfo != NULL, srcfile_ltemc_ltemc_c);

    IOP_create();
    
    g_lqLTEM.atcmd = calloc(1, sizeof(atcmd_t));
    ASSERT(g_lqLTEM.atcmd != NULL, srcfile_ltemc_ltemc_c);
    atcmd_setOptions(atcmd__defaultTimeoutMS, atcmd_okResponseParser);
    ATCMD_reset(true);

    ntwk_create();

    g_lqLTEM.cancellationRequest = false;
    g_lqLTEM.pdpContext = 1;
    g_lqLTEM.appEventCB = eventNotifCallback;
}



/**
 *	@brief Uninitialize the LTEm device structures.
 */
void ltem_destroy()
{
	ltem_stop();

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

    spi_start(g_lqLTEM.spi);                                              // start host SPI

    bool foundOn = qbg_isPowerOn();
    if (foundOn)
    {
        if (resetAction)
        {
            qbg_reset(true);                                    // do hardware reset (aka power cycle)
            foundOn = false;
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
    ASSERT(qbg_isPowerOn(), srcfile_ltemc_ltemc_c);

    SC16IS7xx_start();                                      // initialize NXP SPI-UART bridge base functions: FIFO, levels, baud, framing
    IOP_attachIrq();                                        // attach I/O processor ISR to IRQ
    SC16IS7xx_enableIrqMode();                              // enable IRQ generation on SPI-UART bridge (IRQ mode)

    if (foundOn)
        g_lqLTEM.deviceState = deviceState_appReady;    // assume device state = appReady, APP RDY sent in 1st ~10 seconds of BGx running
    else
    {
        uint32_t appRdyWaitStart = pMillis();
        while (g_lqLTEM.deviceState != deviceState_appReady)
        {
            pDelay(1);                                                      // yields behind the scenes
            if (pMillis() - appRdyWaitStart > PERIOD_FROM_SECONDS(15) && g_lqLTEM.deviceState == deviceState_powerOn)
                g_lqLTEM.deviceState = deviceState_appReady;            // missed it somehow
        }
    }

    // if (!foundOn)                                   
    //     IOP_awaitAppReady();                                // wait for BGx to signal out firmware ready (URC message)
    // else

    qbg_setOptions();                                       // initialize BGx operating settings
}


/**
 *	@brief Powers off the modem without destroying memory objects. Modem device will require ltem_start() to reinit HW.
 */
void ltem_stop()
{
    spi_stop(g_lqLTEM.spi);
    IOP_stopIrq();
    g_lqLTEM.deviceState = deviceState_powerOff;
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
deviceState_t ltem_readDeviceState()
{
    if (platform_readPin(g_lqLTEM.pinConfig.statusPin))           // ensure powered off device doesn't report otherwise
    {
        if (g_lqLTEM.deviceState == deviceState_powerOff)
            g_lqLTEM.deviceState = deviceState_powerOn; 
    }
    else
        g_lqLTEM.deviceState = deviceState_powerOff;

    return g_lqLTEM.deviceState;
}


/**
 *	@brief Background work task runner. To be called in application Loop() periodically.
 */
void ltem_doWork()
{
    if (!ltem_readDeviceState())
        ltem_notifyApp(appEvent_fault_hardFault, "LTEm I/O Error");

    for (size_t i = 0; i < sizeof(g_lqLTEM.streamWorkers) / sizeof(moduleDoWorkFunc_t); i++)  // each stream with a doWork() register it at OPEN (removed if last CLOSE)
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

void LTEM_registerDoWorker(moduleDoWorkFunc_t *doWorker)
{
    bool found = false;
    for (size_t i = 0; i < sizeof(g_lqLTEM.streamWorkers) / sizeof(moduleDoWorkFunc_t); i++)     // wireup sckt_doWork into g_ltemc worker array
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
