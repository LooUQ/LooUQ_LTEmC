//	Copyright (c) 2020 LooUQ Incorporated.

//	Licensed under The MIT License. See LICENSE in the root directory.
#include "cr_lte_modem.h"
#include <nxp/sc16is741a.h>
#include <quectel/bg96.h>
#include <pal/platform_gpio.h>
#include <pal/platform_threading.h>



lte_modem_config_t FEATHER_BREAKOUT =
{
	.chip_sel_line = 13,
	.irq_line = 12,
	.status_pin = 06,
	.power_key = 11,
	.reset_pin = 19,
	.connected_pin = 18
};


lte_modem_config_t RPI_BREAKOUT = 
{
	.chip_sel_line = 0, /*< J8_24 */
	.irq_line = 22U,	/*< J8_15 */
	.status_pin = 13U,	/*< J8_22 */
	.power_key = 24U,	/*< J8_18 */
	.reset_pin = 23U,	/*< J8_16 */
	.connected_pin = 12U/*< J8_32 */
};


typedef struct cr_lte_modem_tag
{
	const lte_modem_config_t* config;
	sc16is741a_device data_bridge;
	platform_gpio_pin power_key;
	platform_gpio_pin status_pin;
	platform_gpio_pin interrupt_line;
	platform_gpio_pin connected_line;
	platform_gpio_pin reset_line;
} cr_lte_modem_descriptor;



static void
ltemdm_irq_callback(platform_gpio_pin irq_pin)
{

}



static void
ltemdm_status_changed(platform_gpio_pin status_pin)
{

}



static void
ltemdm_connected_changed(platform_gpio_pin status_pin)
{

}



/**
 *	\brief Power on the modem.
 *
 *	\param[in] modem The LTE modem.
 */
static void
ltemdm_power_on(lte_modem modem)
{
	platform_gpio_pin_write(modem->power_key, PLATFORM_GPIO_PIN_VAL_HIGH);
	platform_thread_sleep_for(BG96_POWERON_DELAY);
	platform_gpio_pin_write(modem->power_key, PLATFORM_GPIO_PIN_VAL_LOW);
}



/**
 *	\brief Power off the modem.
 *
 *	\param[in] modem The LTE modem.
 */
static void
ltemdm_power_off(lte_modem modem)
{
	platform_gpio_pin_write(modem->power_key, PLATFORM_GPIO_PIN_VAL_HIGH);
	platform_thread_sleep_for(BG96_POWEROFF_DELAY);
	platform_gpio_pin_write(modem->power_key, PLATFORM_GPIO_PIN_VAL_LOW);
}



/**
 *	\brief Initialize the modems IO.
 *
 *	\param[in] modem The LTE modem.
 */
static void
ltemdm_init_ioctl(lte_modem modem)
{
	/* Attempt to open all pins. If a pin num is value -1, the pin will be NULL. */
	modem->power_key = platform_gpio_pin_open(modem->config->power_key, PLATFORM_GPIO_PIN_DIR_OUTPUT);
	platform_gpio_pin_write(modem->power_key, PLATFORM_GPIO_PIN_VAL_LOW);
	modem->status_pin = platform_gpio_pin_open(modem->config->status_pin, PLATFORM_GPIO_PIN_DIR_INPUT);
	modem->reset_line = platform_gpio_pin_open(modem->config->reset_pin, PLATFORM_GPIO_PIN_DIR_OUTPUT);
	platform_gpio_pin_write(modem->reset_line, PLATFORM_GPIO_PIN_VAL_LOW);
	modem->interrupt_line = platform_gpio_pin_open(modem->config->irq_line, PLATFORM_GPIO_PIN_DIR_INPUT_PULLUP);
	modem->connected_line = platform_gpio_pin_open(modem->config->connected_pin, PLATFORM_GPIO_PIN_DIR_INPUT_PULLUP);

	/* When interrupt is LOW, data is available. */
	platform_gpio_pin_allow_interrupt(modem->interrupt_line, true, PLATFORM_GPIO_PIN_INT_LOW, ltemdm_irq_callback);

	/* Interrupt when STATUS changes. */
	platform_gpio_pin_allow_interrupt(modem->status_pin, true, PLATFORM_GPIO_PIN_INT_BOTH, ltemdm_status_changed);

	/* Interrupt when CONNECTED changes. */
	platform_gpio_pin_allow_interrupt(modem->connected_line, true, PLATFORM_GPIO_PIN_INT_BOTH, ltemdm_connected_changed);

	ltemdm_power_on(modem);
}



/**
 *	\brief Initialize the data bridge.
 *
 *	\param[in] modem The LTE modem.
 */
static void
ltemdm_init_bridge(lte_modem modem)
{
	modem->data_bridge = sc16is741a_init(modem->config->chip_sel_line, LTEMDM_SPI_CLOCK_FREQ_HZ, BG96_BAUDRATE_DEFAULT);


	
	/* Lastly, enable the FIFO. */
	sc16is741a_allow_FIFO(modem->data_bridge, true);
}



/**
 *	\brief Initialize the LTE modem.
 *
 *	\param[in] ltemdm_config The LTE modem initial configurations.
 *	\returns An lte_modem instance.
 */
lte_modem ltemdm_init(const lte_modem_config_t* ltemdm_config)
{
	cr_lte_modem_descriptor* modem = (cr_lte_modem_descriptor*)malloc(sizeof(cr_lte_modem_descriptor));
	if (modem == NULL)
	{
		/* [error]: modem allocation failed. */
		return NULL;
	}	

	modem->config = ltemdm_config;
	ltemdm_init_ioctl(modem);
#if 0
	ltemdm_init_bridge(modem);
#endif

	return modem;
}



/**
 *	\brief Uninitialize the LTE modem.
 *
 *	\param[in] modem The LTE modem.
 */
void
ltemdm_uninit(lte_modem modem)
{
	if (modem == NULL)
	{
		return;
	}
#if 0
	sc16is741a_uninit(modem->data_bridge);
#endif
	ltemdm_power_off(modem);

	platform_gpio_pin_close(modem->power_key);
	platform_gpio_pin_close(modem->reset_line);
	platform_gpio_pin_close(modem->status_pin);
	platform_gpio_pin_close(modem->connected_line);
	platform_gpio_pin_close(modem->interrupt_line);

	free(modem);
}
