//	Copyright (c) 2020 LooUQ Incorporated.
//	Licensed under The MIT License. See LICENSE in the root directory.


// #define _DEBUG
#include <ltem1c.h>


/* ------------------------------------------------------------------------------------------------
 * GLOBAL LTEm1 Device Object, One LTEm1 supported
 * --------------------------------------------------------------------------------------------- */
ltem1Device_t *g_ltem1;

// private local declarations 
static void s_initIO();


#pragma region public functions


/**
 *	\brief Initialize the LTEm1 modem.
 *
 *	\param ltem1_config [in] - The LTE modem gpio pin configuration.
 *  \param funcLevel [in] - Determines the LTEm1 functionality to create and start.
 *  \param appNotifyCB [in] - If supplied (not NULL), this function will be invoked for significant LTEm1 events.
 */
void ltem1_create(const ltem1PinConfig_t ltem1_config, appNotify_func appNotifyCB)
{
	g_ltem1 = calloc(1, sizeof(ltem1Device_t));
	if (g_ltem1 == NULL)
	{
        ltem1_notifyApp(ltem1NotifType_memoryAllocFault, "ltem1-could not alloc ltem1 object");
	}

	g_ltem1->pinConfig = ltem1_config;
    g_ltem1->spi = spi_create(g_ltem1->pinConfig.spiCsPin);
    //g_ltem1->spi = createSpiConfig(g_ltem1->gpio->spiCsPin, LTEM1_SPI_DATARATE, BG96_BAUDRATE_DEFAULT);

    g_ltem1->modemInfo = calloc(1, sizeof(modemInfo_t));
	if (g_ltem1->modemInfo == NULL)
	{
        ltem1_notifyApp(ltem1NotifType_memoryAllocFault, "ltem1-could not alloc ltem1 modem info object");
	}

    g_ltem1->dataContext = 1;

    g_ltem1->action = calloc(1, sizeof(action_t));
    g_ltem1->action->lastActionError = calloc(1, sizeof(actionHistory_t));
    g_ltem1->action->isOpen = false;
    g_ltem1->cancellationRequest = false;
    g_ltem1->appNotifyCB = appNotifyCB;

    iop_create();
    ntwk_create();

    //g_ltem1->mqtt = mqtt_create();

    // if (funcLevel >= ltem1Functionality_iop)
    // {
    //     g_ltem1->iop = iop_create();
    // }
    // if (funcLevel >= ltem1Functionality_actions)
    // {
    //     g_ltem1->action = calloc(1, sizeof(action_t));
    //     g_ltem1->action->lastAction = calloc(1, sizeof(actionHistory_t));
    //     g_ltem1->action->isOpen = false;
    // }
    // if (funcLevel >= ltem1Functionality_services)
    // {
    //     g_ltem1->network = ntwk_create();
    //     g_ltem1->sockets = sckt_create();
    //     g_ltem1->mqtt = mqtt_create();
    // }
    // g_ltem1->funcLevel = funcLevel;

    // if (ltem1Start)
    //     ltem1_start();
    return g_ltem1;
}



/**
 *	\brief Power on and start the modem (perform component init).
 * 
 *  \param ltem1Start [in] - Determines if the LTEm1 should be started (aka powered ON) during initialization process.
 */
void ltem1_start()
{
    s_initIO();                             // set GPIO pins to operating state
    spi_start(g_ltem1->spi);

	if (!gpio_readPin(g_ltem1->pinConfig.statusPin))
	{
        qbg_powerOn();
        g_ltem1->qbgReadyState = qbg_readyState_powerOn;
	}
	else
	{
		PRINTF(dbgColor_info, "LTEm1 found powered on.\r\n");
        g_ltem1->qbgReadyState = qbg_readyState_appReady;

        // power off/on if IRQ latched: previously fired and not serviced
        gpioPinValue_t irqState = gpio_readPin(g_ltem1->pinConfig.irqPin);
        if (irqState == gpioValue_low)
        {
    		PRINTF(dbgColor_warn, "Warning: LTEm1 IRQ invalid, reseting!\r\n");
            sc16is741a_writeReg(SC16IS741A_UARTRST_ADDR, SC16IS741A_SW_RESET_MASK);
        }
	}

    sc16is741a_start();     // start NXP SPI-UART bridge
    iop_start();
    iop_awaitAppReady();    // wait for BGx to signal out firmware ready
    qbg_start();            // initialize BGx operating settings
}



/**
 *	\brief Performs a HW reset of LTEm1 and optionally executes start sequence.
 */
void ltem1_reset()
{
    qbg_reset();
    ltem1_start();
}



/**
 *	\brief Check the BGx for hardware ready (status pin).
 *
 *  \return True is status HIGH, hardware ready.
 */
bool ltem1_chkHwReady()
{
	return gpio_readPin(g_ltem1->pinConfig.statusPin);
}



/**
 *	\brief Performs a HW reset of LTEm1 and optionally executes start sequence.
 */
qbgReadyState_t ltem1_getReadyState()
{
    return g_ltem1->qbgReadyState;
}



/**
 *	\brief Powers off the modem without destroying memory objects. Modem device will require ltem1_start() to reinit HW.
 */
void ltem1_stop()
{
    g_ltem1->qbgReadyState = qbg_readyState_powerOff;
    bg96_powerOff();
}



/**
 *	\brief Uninitialize the LTE modem.
 */
void ltem1_destroy()
{
	ltem1_stop();

	gpio_pinClose(g_ltem1->pinConfig.irqPin);
	gpio_pinClose(g_ltem1->pinConfig.powerkeyPin);
	gpio_pinClose(g_ltem1->pinConfig.resetPin);
	gpio_pinClose(g_ltem1->pinConfig.statusPin);

    ip_destroy();
    free(g_ltem1->action);
    iop_destroy();
    spi_destroy(g_ltem1->spi);
	free(g_ltem1);
}



/**
 *	\brief Background work task runner. To be called in application Loop() periodically.
 */
void ltem1_doWork()
{
    if (g_ltem1->scktWork_func != NULL)
    {
        g_ltem1->scktWork_func();
    }
    //sckt_doWork();
    
    if (g_ltem1->mqttWork_func != NULL)
    {
        g_ltem1->mqttWork_func();
    }
    //mqtt_doWork();
}


/**
 *	\brief Function of last resort, catastrophic failure Background work task runner. To be called in application Loop() periodically.
 * 
 *  \param notifyType [in] - Enum of broad notification categories.
 *  \param notifyMsg [in] - Message from origination about the fatal condition.
 */
void ltem1_notifyApp(uint8_t notifType, const char *msg)
{
    PRINTF(dbgColor_error, "\r\rLTEm1C FaultCd=%d - %s\r", notifType, msg);         // log to debugger if attached

    if (g_ltem1->appNotifyCB != NULL)                                       
        g_ltem1->appNotifyCB(notifType, msg);                   // if app handler registered, it may/may not return

    while (notifType > ltem1NotifType__CATASTROPHIC) {}         // if notice is "fatal" loop forever and hope for a watchdog
}



/**
 *	\brief Registers the address (void*) of your application yield callback handler.
 * 
 *  \param [in] customFaultHandler - Pointer to application provided fault handler.
 */
void ltem1_setYieldCb(platform_yieldCB_func_t yieldCb_func)
{
    platform_yieldCB_func = yieldCb_func;
}


#pragma endregion

/* private (static) functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions


/**
 *	\brief Initialize the modems IO.
 *
 *	\param[in] modem The LTE modem.
 */
static void s_initIO()
{
	// on Arduino, ensure pin is in default "logical" state prior to opening
	gpio_writePin(g_ltem1->pinConfig.powerkeyPin, gpioValue_low);
	gpio_writePin(g_ltem1->pinConfig.resetPin, gpioValue_low);
	gpio_writePin(g_ltem1->pinConfig.spiCsPin, gpioValue_high);

	gpio_openPin(g_ltem1->pinConfig.powerkeyPin, gpioMode_output);		// powerKey: normal low
	gpio_openPin(g_ltem1->pinConfig.resetPin, gpioMode_output);			// resetPin: normal low
	gpio_openPin(g_ltem1->pinConfig.spiCsPin, gpioMode_output);			// spiCsPin: invert, normal gpioValue_high

	gpio_openPin(g_ltem1->pinConfig.statusPin, gpioMode_input);
	gpio_openPin(g_ltem1->pinConfig.irqPin, gpioMode_inputPullUp);
}


#pragma endregion
