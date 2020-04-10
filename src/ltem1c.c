//	Copyright (c) 2020 LooUQ Incorporated.
//	Licensed under The MIT License. See LICENSE in the root directory.

#include "ltem1c.h"
// #include "components/nxp_sc16is741a.h"
// #include "components/quectel_bg96.h"
// #include "platform/platform_stdio.h"
// #include "platform/platform_gpio.h"
// #include "platform/platform_timing.h"
//#include "iop.h"


ltem1_pinConfig_t FEATHER_BREAKOUT =
{
	.spiCsPin = 13,
	.spiIrqPin = 12,
	.statusPin = 06,
	.powerkeyPin = 11,
	.resetPin = 19,
	.ringUrcPin = 0,
    .wakePin = 0
};

ltem1_pinConfig_t RPI_BREAKOUT = 
{
	.spiCsPin = 0, 			//< J8_24
	.spiIrqPin = 22U,		//< J8_15
	.statusPin = 13U,		//< J8_22
	.powerkeyPin = 24U,		//< J8_18
	.resetPin = 23U,		//< J8_16
	.ringUrcPin = 0,
    .wakePin = 0
};

ltem1_device_t *g_ltem1;

/**
 *	\brief Power on the modem.
 *
 *	\param[in] modem The LTE modem.
 */
void ltem1_powerOn(ltem1_device ltem1)
{
	if (!gpio_readPin(ltem1->pinConfig->statusPin))
	{
		DBGPRINTF("Powering LTEm1 On...");
		gpio_writePin(g_ltem1->pinConfig->powerkeyPin, gpioValue_high);
		timing_delay(BG96_POWERON_DELAY);
		gpio_writePin(ltem1->pinConfig->powerkeyPin, gpioValue_low);
		while (!gpio_readPin(ltem1->pinConfig->statusPin))
		{
			timing_delay(500);
		}
		DBGPRINTF("DONE.\r\n");
	}
	else
	{
		DBGPRINTF("LTEm1 is already powered on.\r\n");
	}
}



/**
 *	\brief Power off the modem.
 *
 *	\param[in] modem The LTE modem.
 */
void ltem1_powerOff(ltem1_device ltem1)
{
	gpio_writePin(ltem1->pinConfig->powerkeyPin, gpioValue_high);
	timing_delay(BG96_POWEROFF_DELAY);
	gpio_writePin(ltem1->pinConfig->powerkeyPin, gpioValue_low);
}



/**
 *	\brief Initialize the modems IO.
 *
 *	\param[in] modem The LTE modem.
 */
void ltem1_initIO(ltem1_device ltem1, bool enableIrqMode)
{
	// on Arduino, ensure pin is in normal "logical" state prior to opening
	gpio_writePin(ltem1->pinConfig->powerkeyPin, gpioValue_low);
	gpio_writePin(ltem1->pinConfig->resetPin, gpioValue_low);
	gpio_writePin(ltem1->pinConfig->spiCsPin, gpioValue_high);

	gpio_openPin(ltem1->pinConfig->powerkeyPin, gpioMode_output);		// powerKey: normal low
	gpio_openPin(ltem1->pinConfig->statusPin, gpioMode_input);
	gpio_openPin(ltem1->pinConfig->resetPin, gpioMode_output);			// resetPin: normal low
	gpio_openPin(ltem1->pinConfig->spiIrqPin, gpioMode_inputPullUp);
	gpio_openPin(ltem1->pinConfig->spiCsPin, gpioMode_output);			// spiCsPin: invert, normal gpioValue_high

	if (enableIrqMode)
	{
		/* Interrupt when NXP sc16is741a bridge has event to service. */
		gpio_attachIsr(ltem1->pinConfig->spiIrqPin, true, gpioIrqTriggerOn_low, iop_irqCallback_bridge);

		// /* Interrupt when BG96 signals external network URC message received. */
		// gpio_attachIsr(ltem1->pinConfig->ringUrcPin, true, gpioIrqTriggerOn_change, ltem1_irqCallback_bg96Urc);
	}
}



/**
 *	\brief Initialize the data bridge.
 *
 *	\param[in] modem The LTE modem.
 */
static void ltem1_initBridge(ltem1_device ltem1, bool enableIrqMode)
{
	ltem1->bridge = sc16is741a_init(ltem1->pinConfig->spiCsPin, LTEM1_SPI_DATARATE, BG96_BAUDRATE_DEFAULT, enableIrqMode);
}



/**
 *	\brief Initialize the LTEm1 modem.
 *
 *	\param[in] ltem1_config The LTE modem initial configurations.
 *  \param[in] startIo Initialize hardware and start modem.
 *  \param[in] enableIrqMode Initialize ltem1 to use IRQ/ISR for communication events.
 * 
 *	\returns An ltem1 modem instance.
 */
ltem1_device ltem1_init(ltem1_device ltem1, const ltem1_pinConfig_t* ltem1_config, bool startIo, bool enableIrqMode)
{
	ltem1 = malloc(sizeof(ltem1_device_t));
	if (ltem1 == NULL)
	{
		/* [error]: modem allocation failed. */
		return NULL;
	}	

	ltem1->pinConfig = ltem1_config;
	ltem1->urcPending = false;

	if (startIo)
	{
		ltem1_initIO(ltem1, enableIrqMode);					// optionally wires up ISR functions.
		ltem1_powerOn(ltem1);
		ltem1_initBridge(ltem1, enableIrqMode);				// optionally configures bridge registers with IRQ settings.
	}
	return ltem1;
}



/**
 *	\brief Uninitialize the LTE modem.
 *
 *	\param[in] modem The LTE modem.
 */
void ltem1_uninit(ltem1_device ltem1)
{
	if (ltem1 == NULL)
	{
		return;
	}
	//sc16is741a_uninit(modem->data_bridge);
	ltem1_powerOff(ltem1);

	gpio_pinClose(ltem1->pinConfig->spiIrqPin);
	gpio_pinClose(ltem1->pinConfig->powerkeyPin);
	gpio_pinClose(ltem1->pinConfig->resetPin);
	gpio_pinClose(ltem1->pinConfig->statusPin);
	//gpio_pinClose(modem->pinConfig->connectedPin);

	free(ltem1);
}



/**
 *	\brief Background work task runner. To be called in application Loop() periodically.
 */
void ltem1_dowork()
{
	// iop_classifyReadBuffers()
}
