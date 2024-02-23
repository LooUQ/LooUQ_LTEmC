/** ***************************************************************************
  @file ltemc-gpio.c
  @brief Modem GPIO/ADC expansion features/services.

  @author Greg Terrell, LooUQ Incorporated

  \loouq
-------------------------------------------------------------------------------

LooUQ-LTEmC // Software driver for the LooUQ LTEm series cellular modems.
Copyright (C) 2022-2023 LooUQ Incorporated

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
Also add information on how to contact you by electronic and paper mail.

**************************************************************************** */


#include <lq-embed.h>
#define lqLOG_LEVEL lqLOGLEVEL_DBG                                  ///< Logging detail level for this source file
//#define DISABLE_ASSERTS                                           ///< ASSERT/ASSERT_W enabled by default, can be disabled 
#define LQ_SRCFILE "PIO"                                            ///< create SRCFILE (3 char) MACRO for lq-diagnostics lqASSERT

#include "ltemc-internal.h"
#include "ltemc-gpio.h"


/**
 * @brief Read valule of ADC interface.
 */
resultCode_t gpio_adcRead(uint8_t portNumber, uint16_t * analogValue)
{
    ASSERT(portNumber > 0 && portNumber <= adc__BG77__maxPin);

    atcmd_configParser("+QADC: ", true, ",", 1, "\r\n", 0);

    if (IS_SUCCESS(atcmd_dispatch("AT+QADC=%d", portNumber)))
    {
        if (memcmp(atcmd_getResponse(), "+QADC: 1", 8) == 0)
        {
            *analogValue = atcmd_getValue();
            return resultCode__success;
        }
    }
    return resultCode__badRequest;
}


/**
 * @brief Configure a GPIO port for intended use.
 */
resultCode_t gpio_configPort(uint8_t portNumber, gpioDirection_t direction, gpioPull_t pullType, gpioPullDrive_t pullDriveCurrent)
{
    ASSERT(portNumber > 0 && portNumber <= gpio__BG77__maxPin);
    ASSERT_W(portNumber > 0 && portNumber <= gpio__LTEM3F__maxPin, "Bad port");

    RSLT;
    if (direction == gpioDirection_input)
        rslt = atcmd_dispatch("AT+QCFG=\"gpio\",1,%d,0,%d,%d", portNumber, pullType, pullDriveCurrent);
    else
        rslt = atcmd_dispatch("AT+QCFG=\"gpio\",1,%d,1", portNumber);

    if (IS_SUCCESS(rslt))
        return resultCode__success;

    return resultCode__badRequest;
}


/**
 *	@brief Read digitial value from a GPIO port.
 */
resultCode_t gpio_read(uint8_t portNumber, bool* pinValue)
{
    ASSERT(portNumber > 0 && portNumber <= gpio__BG77__maxPin);
    ASSERT_W(portNumber > 0 && portNumber <= gpio__LTEM3F__maxPin, "Bad port");

    atcmd_configParser("+QCFG: \"gpio\",", true, ",", 1, "\r\n", 0);
    if (IS_SUCCESS(atcmd_dispatch("AT+QCFG=\"gpio\",2,%d", portNumber)))
    {
        *pinValue = atcmd_getValue();
        return resultCode__success;
    }
    return resultCode__badRequest;
}


/**
 *	@brief Write digital value from to GPIO port.
 */
resultCode_t gpio_write(uint8_t portNumber, bool pinValue)
{
    ASSERT(portNumber > 0 && portNumber <= gpio__BG77__maxPin);
    ASSERT_W(portNumber > 0 && portNumber <= gpio__LTEM3F__maxPin, "Bad port");

    return atcmd_dispatch("AT+QCFG=\"gpio\",3,%d/%d", portNumber, pinValue);
}
