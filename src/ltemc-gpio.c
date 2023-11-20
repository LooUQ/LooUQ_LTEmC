/** ***************************************************************************
  @file 
  @brief Modem GPIO/ADC expansion features/services.

  @author Greg Terrell, LooUQ Incorporated

  \loouq

  @warning Internal dependencies, changes only as directed by LooUQ staff.

-------------------------------------------------------------------------------

LooUQ-LTEmC // Software driver for the LooUQ LTEm series cellular modems.
Copyright (C) 2017-2023 LooUQ Incorporated

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


#define SRCFILE "PIO"                       // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT
//#define ENABLE_DIAGPRINT                    // expand DIAGPRINT into debug output
//#define ENABLE_DIAGPRINT_VERBOSE            // expand DIAGPRINT and DIAGPRINT_V into debug output
#define ENABLE_ASSERT
#include <lqdiag.h>

#include "ltemc-iTypes.h"
#include "ltemc-gpio.h"


// private local declarations
static resultCode_t S__adcValueParser(const char *response, char **endptr);
static resultCode_t S__ioValueParser(const char *response, char **endptr);


/*  GPIO functions are accessed via a single AT command
 *  >> AT+QCFG="gpio",<mode>,<pin>[,[<dir>,<pull>,<drv>]/[<val>][,<save>]]
 *  ADC functions are accessed via 
 *  >> AT+QADC=<port>
 *  BG95&BG77&BG600L Series QCFG AT Commands Manual
 */


/**
 *	@brief Read valule of ADC interface.
 */
resultCode_t gpio_adcRead(uint8_t portNumber, uint16_t* analogValue)
{
    ASSERT(portNumber > 0 && portNumber <= adc__BG77__maxPin);
    ASSERT_W(portNumber > 0 && portNumber <= adc__LTEM3F__maxPin, "Bad port");

    if (atcmd_tryInvoke("AT+QADC=%d", portNumber))
    {
        if (atcmd_awaitResultWithOptions(atcmd__defaultTimeout, S__adcValueParser) == resultCode__success)
        {
            if (memcmp(atcmd_getResponse(), "+QADC: 1", 8) == 0)
            {
                const char* token = atcmd_getToken(1);
                *analogValue = strtol(token, NULL, 10);
                return resultCode__success;
            }
            return resultCode__badRequest;
        }
    }
    return resultCode__locked;
}


/**
 *	\brief Configure a GPIO port for intended use.
 */
resultCode_t gpio_configPort(uint8_t portNumber, gpioDirection_t direction, gpioPull_t pullType, gpioPullDrive_t pullDriveCurrent)
{
    ASSERT(portNumber > 0 && portNumber <= gpio__BG77__maxPin);
    ASSERT_W(portNumber > 0 && portNumber <= gpio__LTEM3F__maxPin, "Bad port");

    bool invoked;
    if (direction == gpioDirection_input)
        invoked = atcmd_tryInvoke("AT+QCFG=\"gpio\",1,%d,0,%d,%d", portNumber, pullType, pullDriveCurrent);
    else
        invoked = atcmd_tryInvoke("AT+QCFG=\"gpio\",1,%d,1", portNumber);

    if (invoked)
    {
        resultCode_t rslt = atcmd_awaitResult();
        if (rslt == resultCode__success)
        {
            return resultCode__success;
        }
        else
        {
            return resultCode__badRequest;
        }
    }
    return resultCode__locked;
}


/**
 *	\brief Read digitial value from a GPIO port.
 */
resultCode_t gpio_read(uint8_t portNumber, bool* pinValue)
{
    ASSERT(portNumber > 0 && portNumber <= gpio__BG77__maxPin);
    ASSERT_W(portNumber > 0 && portNumber <= gpio__LTEM3F__maxPin, "Bad port");

    if (atcmd_tryInvoke("AT+QCFG=\"gpio\",2,%d", portNumber))
    {
        if (atcmd_awaitResultWithOptions(atcmd__defaultTimeout, S__ioValueParser) == resultCode__success)
        {
            const char* token = atcmd_getToken(1);
            *pinValue = strtol(token, NULL, 10);
            return resultCode__success;
        }
    }
    return resultCode__locked;
}


/**
 *	\brief Write digital value from to GPIO port.
 */
resultCode_t gpio_write(uint8_t portNumber, bool pinValue)
{
    ASSERT(portNumber > 0 && portNumber <= gpio__BG77__maxPin);
    ASSERT_W(portNumber > 0 && portNumber <= gpio__LTEM3F__maxPin, "Bad port");

    if (atcmd_tryInvoke("AT+QCFG=\"gpio\",3,%d/%d", portNumber, pinValue))
    {
        if (atcmd_awaitResult() == resultCode__success)
        {
            return resultCode__success;
        }
    }
    return resultCode__locked;
}



/* Static local functions
 * --------------------------------------------------------------------------------------------- */

static resultCode_t S__adcValueParser(const char *response, char **endptr)
{
    cmdParseRslt_t parseRslt = atcmd_stdResponseParser("+QADC: ", true, ",", 1, 0, "\r\n", 0);
    return parseRslt;
}


static resultCode_t S__ioValueParser(const char *response, char **endptr)
{
    cmdParseRslt_t parseRslt = atcmd_stdResponseParser("+QCFG: \"gpio\",", true, ",", 1, 0, "\r\n", 0);
    return parseRslt;
}
