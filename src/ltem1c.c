//	Copyright (c) 2020 LooUQ Incorporated.
//	Licensed under The MIT License. See LICENSE in the root directory.

#include "ltem1c.h"
// #include "components/nxp_sc16is741a.h"
// #include "components/quectel_bg96.h"
// #include "platform/platform_stdio.h"
// #include "platform/platform_gpio.h"
// #include "platform/platform_timing.h"
//#include "iop.h"


ltem1PinConfig_t FEATHER_BREAKOUT =
{
	.spiCsPin = 13,
	.irqPin = 12,
	.statusPin = 06,
	.powerkeyPin = 11,
	.resetPin = 19,
	.ringUrcPin = 0,
    .wakePin = 0
};

ltem1PinConfig_t RPI_BREAKOUT = 
{
	.spiCsPin = 0, 			//< J8_24
	.irqPin = 22U,		//< J8_15
	.statusPin = 13U,		//< J8_22
	.powerkeyPin = 24U,		//< J8_18
	.resetPin = 23U,		//< J8_16
	.ringUrcPin = 0,
    .wakePin = 0
};

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
 *	\param[in] ltem1_config The LTE modem gpio pin configuration.
 *  \param[in] funcLevel Determines the LTEm1 functionality to create and start.
 */
void ltem1_create(const ltem1PinConfig_t* ltem1_config, ltem1Functionality_t funcLevel)
{
	g_ltem1 = calloc(1, sizeof(ltem1Device_t));
	if (g_ltem1 == NULL)
	{
        ltem1_faultHandler(0, "ltem1-could not alloc ltem1 object");
	}

	g_ltem1->gpio = ltem1_config;
    g_ltem1->spi = spi_create(g_ltem1->gpio->spiCsPin);
    //g_ltem1->spi = createSpiConfig(g_ltem1->gpio->spiCsPin, LTEM1_SPI_DATARATE, BG96_BAUDRATE_DEFAULT);

    g_ltem1->modemInfo = calloc(1, sizeof(modemInfo_t));
	if (g_ltem1->modemInfo == NULL)
	{
        ltem1_faultHandler(0, "ltem1-could not alloc ltem1 modem info object");
	}

    g_ltem1->dataContext = 1;

    if (funcLevel >= ltem1Functionality_iop)
    {
        g_ltem1->iop = iop_create();
    }
    if (funcLevel >= ltem1Functionality_actions)
    {
        g_ltem1->action = action_reset();
    }
    if (funcLevel >= ltem1Functionality_services)
    {
        g_ltem1->network = ntwk_createNetwork();
        g_ltem1->protocols = ntwk_createProtocols();
    }
    g_ltem1->funcLevel = funcLevel;

    ltem1_start(funcLevel);
}



/**
 *	\brief Power on and start the modem (perform component init).
 * 
 *  \param [in] funcLevel - Enum specifying which LTEm1c subsystems should be initialized and started.
 */
void ltem1_start(ltem1Functionality_t funcLevel)
{
    initIO();   // make sure GPIO pins in correct state
    spi_start(g_ltem1->spi);

	if (!gpio_readPin(g_ltem1->gpio->statusPin))
	{
        qbg_powerOn();
        g_ltem1->qbgReadyState = qbg_readyState_powerOn;
        g_ltem1->funcLevel = funcLevel;
	}
	else
	{
		PRINTF_INFO("LTEm1 found powered on.\r\n");
        g_ltem1->qbgReadyState = qbg_readyState_appReady;

        // power off/on if IRQ latched: previously fired and not serviced
        gpioPinValue_t irqState = gpio_readPin(g_ltem1->gpio->irqPin);
        if (irqState == gpioValue_low)
        {
    		PRINTF_WARN("Invalid LTEm1 IRQ state active.\r\n");
            sc16is741a_writeReg(SC16IS741A_UARTRST_ADDR, SC16IS741A_SW_RESET_MASK);

            // bg96_powerOff();
            // timing_delay(1000);
            // bg96_powerOn();
        }

        /* future HW rev, reset bridge v. BG */
        // // reset IRQ if latched: previous fired and not serviced
        // gpio_pinValue_t irqState = gpio_readPin(g_ltem1->gpio->irqPin);
        // if (irqState == gpioValue_low)
        //     ltem1_reset(false);
	}

    // start NXP SPI-UART bridge
    sc16is741a_start();

    if (funcLevel >= ltem1Functionality_iop)
        iop_start();

    // actions doesn't have a start/stop 

    if (funcLevel >= ltem1Functionality_services)
        qbg_start();
}



/**
 *	\brief Performs a HW reset of LTEm1 and optionally executes start sequence.
 */
void ltem1_reset(bool restart)
{
	// gpio_writePin(g_ltem1->gpio->resetPin, gpioValue_low);
	// timing_delay(BG96_RESET_DELAY);
	// gpio_writePin(g_ltem1->gpio->resetPin, gpioValue_high);

    // if (restart)
    // {
    //     sc16is741a_start();
    //     if (g_ltem1->funcLevel == ltem1_functionality_iop)
    //     {
    //         iop_start();
    //     }
    //     bg96_start();
    // }
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

	gpio_pinClose(g_ltem1->gpio->irqPin);
	gpio_pinClose(g_ltem1->gpio->powerkeyPin);
	gpio_pinClose(g_ltem1->gpio->resetPin);
	gpio_pinClose(g_ltem1->gpio->statusPin);

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
    ip_receiverDoWork();
    qbg_processUrcStateQueue();
}


/**
 *	\brief Function of last resort, catestrophic failure Background work task runner. To be called in application Loop() periodically.
 * 
 *  \param [in] faultMsg - Message from origination about the fatal condition.
 */
void ltem1_faultHandler(uint16_t statusCode, const char * faultMsg)
{
    PRINTF_ERR("\r\rStatus=%d %s\r", statusCode, faultMsg);

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
	gpio_writePin(g_ltem1->gpio->powerkeyPin, gpioValue_low);
	gpio_writePin(g_ltem1->gpio->resetPin, gpioValue_low);
	gpio_writePin(g_ltem1->gpio->spiCsPin, gpioValue_high);

	gpio_openPin(g_ltem1->gpio->powerkeyPin, gpioMode_output);		// powerKey: normal low
	gpio_openPin(g_ltem1->gpio->resetPin, gpioMode_output);			// resetPin: normal low
	gpio_openPin(g_ltem1->gpio->spiCsPin, gpioMode_output);			// spiCsPin: invert, normal gpioValue_high

	gpio_openPin(g_ltem1->gpio->statusPin, gpioMode_input);
	gpio_openPin(g_ltem1->gpio->irqPin, gpioMode_inputPullUp);
}


#pragma endregion
