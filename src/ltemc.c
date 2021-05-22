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


#define _DEBUG 0                        // set to non-zero value for PRINTF debugging output, 
// debugging output options             // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if defined(_DEBUG) && _DEBUG > 0
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG == 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#endif


#include "ltemc.h"

/* ------------------------------------------------------------------------------------------------
 * GLOBAL LTEm1 Device Object, One LTEm1 supported
 * --------------------------------------------------------------------------------------------- */
ltemDevice_t *g_ltem;


#pragma region public functions


/**
 *	\brief Initialize the LTEm1 modem.
 *
 *	\param ltem_config [in] - The LTE modem gpio pin configuration.
 *  \param funcLevel [in] - Determines the LTEm1 functionality to create and start.
 *  \param appNotifyCB [in] - If supplied (not NULL), this function will be invoked for significant LTEm1 events.
 */
void ltem_create(const ltemPinConfig_t ltem_config, appNotify_func appNotifyCB)
{
	g_ltem = calloc(1, sizeof(ltemDevice_t));
	if (g_ltem == NULL)
	{
        if (appNotifyCB != NULL)                                       
            appNotifyCB(ltemNotifType_memoryAllocFault, "ltem1-could not alloc ltem1 object");
	}

	g_ltem->pinConfig = ltem_config;
    g_ltem->spi = spi_create(g_ltem->pinConfig.spiCsPin);
    //g_ltem->spi = createSpiConfig(g_ltem->gpio->spiCsPin, LTEM1_SPI_DATARATE, BG96_BAUDRATE_DEFAULT);

    g_ltem->modemInfo = calloc(1, sizeof(modemInfo_t));
	if (g_ltem->modemInfo == NULL)
	{
        if (appNotifyCB != NULL)                                       
            appNotifyCB(ltemNotifType_memoryAllocFault, "ltem1-could not alloc ltem1 modem info object");
	}

    g_ltem->dataContext = 1;

    g_ltem->atcmd = calloc(1, sizeof(atcmd_t));
    g_ltem->atcmd->lastActionError = calloc(1, sizeof(atcmdHistory_t));
    g_ltem->atcmd->isOpen = false;
    g_ltem->cancellationRequest = false;
    g_ltem->appNotifyCB = appNotifyCB;

    iop_create();
    ntwk_create();

    return g_ltem;
}



/**
 *	\brief Power on and start the modem (perform component init).
 * 
 *  \param protos [in] - Binary-OR'd list of expected protocol services to validate inclusion and start.
 */
void ltem_start(uint16_t protocolBitMap)
{
    // validate create
    if (protocolBitMap != pdpProtocol_none)
    {
        if (protocolBitMap & pdpProtocol_sockets && g_ltem->sockets == NULL)
            ltem_notifyApp(ltemNotifType_hardFault, "No sckt_create()");

        if (protocolBitMap & pdpProtocol_mqtt && g_ltem->mqtt == NULL)
            ltem_notifyApp(ltemNotifType_hardFault, "No mqtt_create()");

        // if (protos & ltem1Proto_http && g_ltem->http == NULL)
        //     ltem_notifyApp(ltemNotifType_hardFault, "HTTP noCreate");
    }

    ltem__initIo();                            // set GPIO pins to operating state
    spi_start(g_ltem->spi);

    if (qbg_powerOn())                          // returns prior power-state
    {
		PRINTF(DBGCOLOR_info, "LTEm1 found powered on.\r\n");
        g_ltem->qbgReadyState = qbg_readyState_appReady;

        // power off/on if IRQ latched: previously fired and not serviced
        gpioPinValue_t irqState = gpio_readPin(g_ltem->pinConfig.irqPin);
        if (irqState == gpioValue_low)
        {
    		PRINTF(DBGCOLOR_warn, "Warning: LTEm1 IRQ invalid, reseting!\r\n");
            sc16is741a_writeReg(SC16IS741A_UARTRST_ADDR, SC16IS741A_SW_RESET_MASK);
        }
    }
    g_ltem->qbgReadyState = qbg_readyState_powerOn;

    sc16is741a_start();     // start NXP SPI-UART bridge
    iop_start();
    iop_awaitAppReady();    // wait for BGx to signal out firmware ready
    qbg_start();            // initialize BGx operating settings
}



/**
 *	\brief Performs a HW reset of LTEm1 and optionally executes start sequence.
 */
void ltem_reset()
{
    qbg_reset();
    ltem_start(pdpProtocol_none);
}



/**
 *	\brief Check the BGx for hardware ready (status pin).
 *
 *  \return True is status HIGH, hardware ready.
 */
bool ltem_chkHwReady()
{
	return gpio_readPin(g_ltem->pinConfig.statusPin);
}



/**
 *	\brief Performs a HW reset of LTEm1 and optionally executes start sequence.
 */
qbgReadyState_t ltem_getReadyState()
{
    return g_ltem->qbgReadyState;
}



/**
 *	\brief Powers off the modem without destroying memory objects. Modem device will require ltem_start() to reinit HW.
 */
void ltem_stop()
{
    g_ltem->qbgReadyState = qbg_readyState_powerOff;
    bg96_powerOff();
}



/**
 *	\brief Uninitialize the LTE modem.
 */
void ltem_destroy()
{
	ltem_stop();

	gpio_pinClose(g_ltem->pinConfig.irqPin);
	gpio_pinClose(g_ltem->pinConfig.powerkeyPin);
	gpio_pinClose(g_ltem->pinConfig.resetPin);
	gpio_pinClose(g_ltem->pinConfig.statusPin);

    ip_destroy();
    free(g_ltem->atcmd);
    iop_destroy();
    spi_destroy(g_ltem->spi);
	free(g_ltem);
}



/**
 *	\brief Background work task runner. To be called in application Loop() periodically.
 */
void ltem_doWork()
{
    if (!ltem_chkHwReady())
        ltem_notifyApp(ltemNotifType_hwNotReady, "LTEm1 I/O Error");

    if (g_ltem->scktWork_func != NULL)
    {
        g_ltem->scktWork_func();
    }
    if (g_ltem->mqttWork_func != NULL)
    {
        g_ltem->mqttWork_func();
    }
}


/**
 *	\brief Function of last resort, catastrophic failure Background work task runner. To be called in application Loop() periodically.
 * 
 *  \param notifyType [in] - Enum of broad notification categories.
 *  \param notifyMsg [in] - Message from origination about the issue being reported.
 */
void ltem_notifyApp(uint8_t notifType, const char *msg)
{
    PRINTF(DBGCOLOR_error, "\r\rLTEm1C FaultCd=%d - %s\r", notifType, msg);         // log to debugger if attached

    if (g_ltem->appNotifyCB != NULL)                                       
        g_ltem->appNotifyCB(notifType, msg);                   // if app handler registered, it may/may not return

    while (notifType > ltemNotifType__CATASTROPHIC) {}         // if notice is "fatal" loop forever and hope for a watchdog
}



/**
 *	\brief Registers the address (void*) of your application yield callback handler.
 * 
 *  \param [in] customFaultHandler - Pointer to application provided fault handler.
 */
void ltem_setYieldCb(platform_yieldCB_func_t yieldCb_func)
{
    platform_yieldCB_func = yieldCb_func;
}


/**
 *	\brief Initialize the modems IO.
 *
 *	\param[in] modem The LTE modem.
 */
void ltem__initIo()
{
	// on Arduino, ensure pin is in default "logical" state prior to opening
	gpio_writePin(g_ltem->pinConfig.powerkeyPin, gpioValue_low);
	gpio_writePin(g_ltem->pinConfig.resetPin, gpioValue_low);
	gpio_writePin(g_ltem->pinConfig.spiCsPin, gpioValue_high);

	gpio_openPin(g_ltem->pinConfig.powerkeyPin, gpioMode_output);		// powerKey: normal low
	gpio_openPin(g_ltem->pinConfig.resetPin, gpioMode_output);			// resetPin: normal low
	gpio_openPin(g_ltem->pinConfig.spiCsPin, gpioMode_output);			// spiCsPin: invert, normal gpioValue_high

	gpio_openPin(g_ltem->pinConfig.statusPin, gpioMode_input);
	gpio_openPin(g_ltem->pinConfig.irqPin, gpioMode_inputPullUp);
}


#pragma endregion

/* private (static) functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions

#pragma endregion
