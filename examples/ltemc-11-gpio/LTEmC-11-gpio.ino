/** ***************************************************************************
  @file 
  @brief LTEm example/test for GPIO/ADC I/O operations.

  @author Greg Terrell, LooUQ Incorporated

  \loouq
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


#define ENABLE_DIAGPRINT                    // expand DPRINT into debug output
//#define ENABLE_DIAGPRINT_VERBOSE            // expand DPRINT and DPRINT_V into debug output
#define ENABLE_ASSERT
#include <lqdiag.h>

/* specify the pin configuration 
 * --------------------------------------------------------------------------------------------- */
#ifdef ARDUINO_ARCH_ESP32
    #define HOST_ESP32_DEVMOD_BMS
#else
    #define HOST_FEATHER_UXPLOR_L
    // #define HOST_FEATHER_UXPLOR             
    // #define HOST_FEATHER_LTEM3F
#endif

#include <lq-SAMDutil.h>                // allows read of reset cause

#include <ltemc.h>
#include <ltemc-gpio.h>

#define ASSERT(expected_true, failMsg)  if(!(expected_true))  indicateFailure(failMsg)
#define ASSERT_NOTEMPTY(string, failMsg)  if(string[0] == '\0') indicateFailure(failMsg)
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

const uint8_t testGpio = 1;
const uint8_t testAdc = 0;


void setup() 
{
    #ifdef DIAGPRINT_SERIAL
        Serial.begin(115200);
        delay(5000);            // just give it some time
    #endif

    DPRINT(PRNT_RED, "\rLTEmC test-11-gpio\r");
    DPRINT(PRNT_WHITE, "RCause=%d\r\n", lqSAMD_getResetCause());
    platform_openPin(LED_BUILTIN, gpioMode_output);
    //lqDiag_setNotifyCallback(applEvntNotify);

    ltem_create(ltem_pinConfig, NULL, applEvntNotify);
    ltem_start(resetAction_skipIfOn);                                            // start LTEm, if found on reset it

    modemInfo_t *modemInfo  = ltem_getModemInfo();
    if (strcmp(modemInfo->model,"BG77") == 0)
    {
        DPRINT(PRNT_INFO, "Modem: LTEM3F\r");
    }
    else
    {
        DPRINT(PRNT_ERROR, "Modem does not support GPIO\r");
        while (1);
    }

    // init GPIO ports controlled by test
    gpio_configPort(testGpio, gpioDirection_output, gpioPull_NA, gpioPullDrive_NA);
    gpio_configPort(testGpio + 1, gpioDirection_input, gpioPull_none, gpioPullDrive_NA);
}

int loopCnt = 0;

void loop() 
{
    gpio_write(testGpio, loopCnt % 2);
    bool pinValue;
    gpio_read(testGpio + 1, &pinValue);

    if (pinValue != loopCnt % 2)
    {
        DPRINT(PRNT_ERROR, "GPIO compare failed.\r");
        while (1) {}
    }

    uint16_t adcValue;
    gpio_adcRead(testAdc, &adcValue);
    DPRINT(PRNT_CYAN, "ADC value=%dmV\r");

    pDelay(2000);
    loopCnt ++;
    indicateLoop(loopCnt, random(1000));
}



/* test helpers
========================================================================================================================= */

void applEvntNotify(appEvent_t eventType, const char *notifyMsg)
{
    if (eventType == appEvent_fault_assertFailed)
    {
        DPRINT(PRNT_ERROR, "LTEmC-HardFault: %s\r", notifyMsg);
    }
    else 
    {
        DPRINT(PRNT_WHITE, "LTEmC Info: %s\r", notifyMsg);
    }
    return;
}


void indicateLoop(int loopCnt, int waitNext) 
{
    DPRINT(PRNT_MAGENTA, "\r\nLoop=%i \r\n", loopCnt);

    for (int i = 0; i < 6; i++)
    {
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        pDelay(50);
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        pDelay(50);
    }

    DPRINT(PRNT_DEFAULT, "FreeMem=%u\r\n", getFreeMemory());
    DPRINT(PRNT_DEFAULT, "NextTest (millis)=%i\r\r", waitNext);
    pDelay(waitNext);
}


void indicateFailure(char failureMsg[])
{
	DPRINT(PRNT_ERROR, "\r\n** %s \r\n", failureMsg);
    DPRINT(PRNT_ERROR, "** Test Assertion Failed. \r\n");

    uint8_t halt = 1;
    while (halt)
    {
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        pDelay(1000);
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        pDelay(100);
    }
}


/* Check free memory (stack-heap) 
 * - Remove if not needed for production
--------------------------------------------------------------------------------- */

#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__

int getFreeMemory() 
{
    char top;
    #ifdef __arm__
    return &top - reinterpret_cast<char*>(sbrk(0));
    #elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
    return &top - __brkval;
    #else  // __arm__
    return __brkval ? &top - __brkval : &top - __malloc_heap_start;
    #endif  // __arm__
}
