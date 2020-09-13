/******************************************************************************
 *  \file LTEm1c-4-actions.ino
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
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
 * The test3-iop.ino tests the LTEm1 interrupt driven Input-Ouput processing
 * subsystem in the driver which multiplexes the command and protocol streams.
 *****************************************************************************/

#include <ltem1c.h>

#define _DEBUG
#include "platform/platform_stdio.h"

const int APIN_RANDOMSEED = 0;

ltem1PinConfig_t ltem1_pinConfig =
{
  spiCsPin : 13,
  irqPin : 12,
  statusPin : 6,
  powerkeyPin : 11,
  resetPin : 19,
  ringUrcPin : 5,
  wakePin : 10
};

spiConfig_t ltem1_spiConfig = 
{
  dataRate : 2000000U,
  dataMode : spiDataMode_0,
  bitOrder : spiBitOrder_msbFirst,
  csPin : ltem1_pinConfig.spiCsPin
};

ltem1Device_t *ltem1;


void setup() {
    #ifdef USE_SERIAL
        Serial.begin(115200);
        #if (USE_SERIAL)
        while (!Serial) {}
        #else
        delay(1000);
        #endif
    #endif

    PRINTF(0, "LTEm1c test4-actions\r");
    gpio_openPin(LED_BUILTIN, gpioMode_output);
    
    randomSeed(analogRead(APIN_RANDOMSEED));

    ltem1_create(&ltem1_pinConfig, ltem1Start_powerOn, ltem1Functionality_actions);
}


int loopCnt = 0;

void loop() {
    /* BG96 test pattern: get mfg\model
    *
    *  ATI
    * 
    *   Quectel
    *   BG96
    *   Revision: BG96MAR04A02M1G
    * 
    *   OK
    */

    uint8_t regValue = 0;
    char cmdStr[] = "ATI\r\0";
    char response[ACTION_DEFAULT_RESPONSE_SZ] = {0};
    PRINTF(dbgColor_none, "Invoking cmd: %s \r\n", cmdStr);

    action_tryInvoke(cmdStr, false);

    actionResult_t atResult = action_awaitResult(response, ACTION_DEFAULT_RESPONSE_SZ, 0, NULL);
    
    if (atResult == ACTION_RESULT_SUCCESS)    // statusCode == 200
    {
        PRINTF(dbgColor_info, "Got %d chars\r", strlen(response));
        PRINTF(dbgColor_cyan, "Resp: %s\r", response);  

        // test response v. expected 
        char* validResponse = "\r\nQuectel";
        uint8_t responseTest = strncmp(validResponse, response, strlen(validResponse)); 

        if (responseTest != 0)
            indicateFailure("Unexpected command response... failed."); 
    }
    else
    {
        PRINTF(dbgColor_error, "atResult=%d \r", atResult);
        // indicateFailure("Unexpected command response... failed."); 
    }

    loopCnt ++;
    indicateLoop(loopCnt, random(1000));
}


/*
========================================================================================================================= */


void recvResponse(char *response)
{
    iopXfrResult_t rxResult;
    uint8_t retries;
    do
    {
        rxResult = iop_rxGetCmdQueued(response, 65);
        timing_delay(100);
        retries++;
    } while (rxResult == iopXfrResult_incomplete && retries < 5);
}



/* test helpers
========================================================================================================================= */

void indicateFailure(char failureMsg[])
{
	PRINTF(dbgColor_error, "\r\n** %s \r", failureMsg);
    PRINTF(dbgColor_error, "** Test Assertion Failed. \r");

    #if 1
    PRINTF(dbgColor_error, "** Halting Execution \r\n");
    bool halt = true;
    while (halt)
    {
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        timing_delay(1000);
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        timing_delay(100);
    }
    #endif
}


void indicateLoop(int loopCnt, int waitNext) 
{
    PRINTF(dbgColor_info, "\r\nLoop=%i \r\n", loopCnt);

    for (int i = 0; i < 6; i++)
    {
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        timing_delay(50);
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        timing_delay(50);
    }

    PRINTF(dbgColor_magenta, "FreeMem=%u\r\n", getFreeMemory());
    PRINTF(dbgColor_none, "NextTest (millis)=%i\r\r", waitNext);
    timing_delay(waitNext);
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

