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
static resultCode_t S__ioValueParser(const char *response, char **endptr);


/**
 *	\brief Read valule of ADC interface.
 *  AT+QADC=<port>  returns +QADC: <status>,<value>  WHERE <status> Indicate whether the ADC value is read successfully, 0=fail, 1=success; <value> The voltage of specified ADC channel. Unit: mV.
 */
uint16_t adc_read(uint8_t portNumber)
{
    ASSERT(portNumber > 0 && portNumber <= adc__BG77__maxPin);
    ASSERT_WARN(portNumber > 0 && portNumber <= adc__LTEM3F__maxPin);
}


/*  All GPIO functions are accessed via a single AT command
 *  >> AT+QCFG="gpio",<mode>,<pin>[,[<dir>,<pull>,<drv>]/[<val>][,<save>]]
 */


/**
 *	\brief Configure a GPIO port for intended use.
 */
int8_t gpio_configPort(uint8_t portNumber, gpioDirection_t direction, gpioPull_t pullType, gpioPullDrive_t pullDriveCurrent)
{
    ASSERT(portNumber > 0 && portNumber <= gpio__BG77__maxPin);
    ASSERT_W(portNumber > 0 && portNumber <= gpio__LTEM3F__maxPin, "Bad port num");

    if (direction == gpioDirection_input)
        atcmd_tryInvoke("AT+QCFG=\"gpio\",1,%d,0,%d,%d", portNumber, pullType, pullDriveCurrent);
    else
        atcmd_tryInvoke("AT+QCFG=\"gpio\",1,%d,1", portNumber);
}


/**
 *	\brief Read digitial value from a GPIO port.
 */
int8_t gpio_read(uint8_t portNumber)
{
    ASSERT(portNumber > 0 && portNumber <= gpio__BG77__maxPin);
    ASSERT_W(portNumber > 0 && portNumber <= gpio__LTEM3F__maxPin, "Bad port num");

    atcmd_setOptions(atcmd__defaultTimeout, S__ioValueParser);
    atcmd_tryInvokeWithOptions("AT+QCFG=\"gpio\",2,%d/%d", portNumber);
    if (atcmd_awaitResult() == resultCode__success)
    {
    }
}


/**
 *	\brief Write digital value from to GPIO port.
 */
int8_t gpio_write(uint8_t portNumber, uint8_t value)
{
    ASSERT(portNumber > 0 && portNumber <= gpio__BG77__maxPin);
    ASSERT_W(portNumber > 0 && portNumber <= gpio__LTEM3F__maxPin, "Bad port num");

    atcmd_tryInvoke("AT+QCFG=\"gpio\",3,%d/%d", portNumber, value);
}



/* Static local functions
 * --------------------------------------------------------------------------------------------- */
/**
 *	\brief LTEM_C result parser, optionally applied to AT cmd response pipeline.
 */
static resultCode_t S__ioValueParser(const char *response, char **endptr)
{
    const char landmark = "+QCFG: \"gpio\",";
    uint8_t resultIndx = 1;

    char *next = strstr(response, landmark);
    if (next == NULL)
        return cmdParseRslt_pending;

    next += strlen(landmark);
    int16_t resultVal;

    for (size_t i = 0; i < resultIndx; i++)
    {
        if (next == NULL)
            break;
        next = strchr(next, ',');
        next++;         // point past comma
    }
    if (next != NULL)
        resultVal = (uint16_t)strtol(next, endptr, 10);

    // return a success value 200 + result (range: 200 - 300)
    return  (resultVal < 99) ? resultCode__success + resultVal : resultVal;

}
