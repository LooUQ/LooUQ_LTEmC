/******************************************************************************
 *  \file ltemc-gpio.c
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2022 LooUQ Incorporated.
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
 *****************************************************************************/

//#define _DEBUG
#include "ltemc-gpio.h"


// private local declarations
static resultCode_t S__ioValueParser(const char *response, char **endptr);


/**
 *	\brief Read valule of ADC interface.
 *  AT+QADC=<port>  returns +QADC: <status>,<value>  WHERE <status> Indicate whether the ADC value is read successfully, 0=fail, 1=success; <value> The voltage of specified ADC channel. Unit: mV.
 */
uint16_t adc_read(uint8_t portNumber)
{
    ASSERT(portNumber > 0 && portNumber <= adc__BG77__maxPin, srcfile_ltemc_gpio_c);
    ASSERT_WARN(portNumber > 0 && portNumber <= adc__LTEM3F__maxPin, srcfile_ltemc_gpio_c);


}


/*  All GPIO functions are accessed via a single AT command
 *  >> AT+QCFG="gpio",<mode>,<pin>[,[<dir>,<pull>,<drv>]/[<val>][,<save>]]
 */


/**
 *	\brief Configure a GPIO port for intended use.
 */
int8_t gpio_configPort(uint8_t portNumber, gpioDirection_t direction, gpioPull_t pullType, gpioPullDrive_t pullDriveCurrent)
{
    ASSERT(portNumber > 0 && portNumber <= gpio__BG77__maxPin, srcfile_ltemc_gpio_c);
    ASSERT_W(portNumber > 0 && portNumber <= gpio__LTEM3F__maxPin, srcfile_ltemc_gpio_c, "Bad port num");

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
    ASSERT(portNumber > 0 && portNumber <= gpio__BG77__maxPin, srcfile_ltemc_gpio_c);
    ASSERT_W(portNumber > 0 && portNumber <= gpio__LTEM3F__maxPin, srcfile_ltemc_gpio_c, "Bad port num");

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
    ASSERT(portNumber > 0 && portNumber <= gpio__BG77__maxPin, srcfile_ltemc_gpio_c);
    ASSERT_W(portNumber > 0 && portNumber <= gpio__LTEM3F__maxPin, srcfile_ltemc_gpio_c, "Bad port num");

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
