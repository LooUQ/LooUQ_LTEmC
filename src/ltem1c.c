//	Copyright (c) 2020 LooUQ Incorporated.
//	Licensed under The MIT License. See LICENSE in the root directory.


// #define _DEBUG
#include "ltem1c.h"


/* ------------------------------------------------------------------------------------------------
 * GLOBAL LTEm1 Device Object
 * --------------------------------------------------------------------------------------------- */
ltem1Device_t *g_ltem1;

// private local declarations 
static void initIO();


#pragma region public functions


/**
 *	\brief Initialize the LTEm1 modem.
 *
 *	\param [in] ltem1_config - The LTE modem gpio pin configuration.
 *  \param [in] ltem1Start - Determines if the LTEm1 is power ON or OFF during start.
 *  \param [in] funcLevel - Determines the LTEm1 functionality to create and start.
 */
void ltem1_create(const ltem1PinConfig_t ltem1_config, ltem1Start_t ltem1Start, ltem1Functionality_t funcLevel)
{
	g_ltem1 = calloc(1, sizeof(ltem1Device_t));
	if (g_ltem1 == NULL)
	{
        ltem1_faultHandler(0, "ltem1-could not alloc ltem1 object");
	}

	g_ltem1->pinConfig = ltem1_config;
    g_ltem1->spi = spi_create(g_ltem1->pinConfig.spiCsPin);
    //g_ltem1->spi = createSpiConfig(g_ltem1->gpio->spiCsPin, LTEM1_SPI_DATARATE, BG96_BAUDRATE_DEFAULT);

    g_ltem1->modemInfo = calloc(1, sizeof(modemInfo_t));
	if (g_ltem1->modemInfo == NULL)
	{
        ltem1_faultHandler(500, "ltem1-could not alloc ltem1 modem info object");
	}

    g_ltem1->dataContext = 1;

    if (funcLevel >= ltem1Functionality_iop)
    {
        g_ltem1->iop = iop_create();
    }
    if (funcLevel >= ltem1Functionality_actions)
    {
        g_ltem1->action = calloc(1, sizeof(action_t));
        g_ltem1->action->lastAction = calloc(1, sizeof(actionHistory_t));
        g_ltem1->action->isOpen = false;
    }
    if (funcLevel >= ltem1Functionality_services)
    {
        g_ltem1->network = ntwk_create();
        g_ltem1->sockets = sckt_create();
    }
    g_ltem1->funcLevel = funcLevel;
    g_ltem1->cancellationRequest = false;

    if (ltem1Start)
        ltem1_start();
}



/**
 *	\brief Power on and start the modem (perform component init).
 * 
 *  \param [in] funcLevel - Enum specifying which LTEm1c subsystems should be initialized and started.
 */
void ltem1_start()
{
    initIO();   // make sure GPIO pins in correct state
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

    // start NXP SPI-UART bridge
    sc16is741a_start();

    if (g_ltem1->funcLevel >= ltem1Functionality_iop)
    {
        iop_start();
        iop_awaitAppReady();
    }

    if (g_ltem1->funcLevel >= ltem1Functionality_actions)
        qbg_start();
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
 *	\brief Powers off the modem without destroying memory objects. Modem device will require ltem1_start() to reinit HW.
 */
void ltem1_stop()
{
    g_ltem1->qbgReadyState = qbg_readyState_powerOff;
    bg96_powerOff();
}



/**
 *	\brief Uninitialize the LTE modem.
 *
 *	\param[in] modem The LTE modem.
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
    sckt_doWork();
    
    #ifdef LTEM1_MQTT
    mqtt_doWork();
    #endif
}


/**
 *	\brief Function of last resort, catestrophic failure Background work task runner. To be called in application Loop() periodically.
 * 
 *  \param [in] statusCode - HTTP style result code, generally sourced from BGx response to an operation.
 *  \param [in] faultMsg - Message from origination about the fatal condition.
 */
void ltem1_faultHandler(uint16_t statusCode, const char * faultMsg)
{
    PRINTF(dbgColor_error, "\r\rStatus=%d %s\r", statusCode, faultMsg);

    // NotImplemented if custom fault handler go there

    int halt = 1;               // make it possible while debugging to reset error conditions and return
    while(halt) {};
}



/**
 *	\brief Registers the address (void*) of your application custom fault handler.
 * 
 *  \param [in] customFaultHandler - Pointer to application provided fault handler.
 */
void ltem1_registerApplicationFaultHandler(void * customFaultHandler)
{
    // TBD
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
static void initIO()
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
