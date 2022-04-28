/******************************************************************************
 *  \file ltemc-filesys.h
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
 * Use BGx internal flash excess capacity as filesystem for device application 
 *****************************************************************************/

#ifndef __LTEMC_GPIO_H__
#define __LTEMC_GPIO_H__

#include "ltemc.h"


enum gpio__constants
{
    gpio__BG77__maxPin = 9,
    adc__BG77__maxPin = 2,

    gpio__LTEM3F__maxPin = 6,
    adc__LTEM3F__maxPin = 2,
};


typedef enum gpioActionMode_tag
{
    gpioActionMode_init = 1,
    gpioActionMode_read = 2,
    gpioActionMode_write = 3
} gpioActionMode_tag;


typedef enum gpioDirection_tag
{
    gpioDirection_input = 0,
    gpioDirection_output = 1
} gpioDirection_t;


typedef enum gpioPull_tag
{
    gpioPull_none = 0,
    gpioPull_down = 1,
    gpioPull_up = 3,
    gpioPull_noChange = 2,
    gpioPull_NA = 255                   // added to enum for source intent readability 
} gpioPull_t;


typedef enum gpioPullDrive_tag
{
    gpioPullDrive_2mA = 0,
    gpioPullDrive_4mA = 1,
    gpioPullDrive_6mA = 2,
    gpioPullDrive_8mA = 3,
    gpioPullDrive_10mA = 4,
    gpioPullDrive_12mA = 5,
    gpioPullDrive_14mA = 6,
    gpioPullDrive_16mA = 7,
    gpioPullDrive_NA = 255              // added to enum for source intent readability 
} gpioPullDrive_t;


#ifdef __cplusplus
extern "C" {
#endif

/**
 *	\brief Read valule of ADC interface.
 */
uint16_t adc_read(uint8_t portNumber);


/**
 *	\brief Configure a GPIO port for intended use.
 *	\param [in] portNumber - The GPIO port to configured, dependent on modem module and modem board.
 *	\param [in] direction - Set the GPIO port to be for input or output.
 *	\param [in] pullType - If the port is input, specifies the optional pull up/down behavior for the GPIO port. Ignored if "direction" is output.
 *	\param [in] pullDriveCurrent - If the port is input AND pull is specified, sets the current limit for the pull circuit. Ignored if "direction" is output.
 *  \return Error status: 0=no error, -1 indicates invalid port number.
 */
int8_t gpio_configPort(uint8_t portNumber, gpioDirection_t direction, gpioPull_t pullType, gpioPullDrive_t pullDriveCurrent);


/**
 *	\brief Read digitial value from a GPIO port.
 *	\param [in] portNumber - The GPIO port to read, dependent on modem module and modem board.
 *  \return Value read 0=low, 1=high. Result of -1 indicates invalid port number.
 */
int8_t gpio_read(uint8_t portNumber);


/**
 *	\brief Write digital value from to GPIO port.
 *	\param [in] portNumber - The GPIO port to read, dependent on modem module and modem board.
 *	\param [in] value - The 0 or 1 digital value to assign to the GPIO port output.
 *  \return Error status: 0=no error, -1 indicates invalid port number.
 */
int8_t gpio_write(uint8_t portNumber, uint8_t value);




#ifdef __cplusplus
}
#endif

#endif  // !__LTEMC_GPIO_H__
