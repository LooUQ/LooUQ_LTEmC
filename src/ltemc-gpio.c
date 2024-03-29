/** ****************************************************************************
  \file 
  \brief Public API GPIO and ADC support (requires appropriate LTEm model)
  \author Greg Terrell, LooUQ Incorporated

  \loouq

--------------------------------------------------------------------------------

    This project is released under the GPL-3.0 License.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 
***************************************************************************** */


#define _DEBUG 0                                // set to non-zero value for PRINTF debugging output, 
// debugging output options                     // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if _DEBUG > 0
    asm(".global _printf_float");               // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                        // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG == 2
    #include <jlinkRtt.h>                       // output debug PRINTF macros to J-Link RTT channel
    #define PRINTF(c_,f_,__VA_ARGS__...) do { rtt_printf(c_, (f_), ## __VA_ARGS__); } while(0)
    #endif
#else
#define PRINTF(c_, f_, ...) 
#endif

#define SRCFILE "PIO"                           // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT
#include "ltemc-internal.h"
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
                *analogValue = atcmd_getValue();
                return resultCode__success;
            }
            return resultCode__badRequest;
        }
    }
    return resultCode__conflict;
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
    return resultCode__conflict;
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
            *pinValue = atcmd_getValue();
            return resultCode__success;
        }
    }
    return resultCode__conflict;
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
    return resultCode__conflict;
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
