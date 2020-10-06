/******************************************************************************
 *  \file LTEm1c-3-iop.ino
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
 * Tests the LTEm1 interrupt driven Input-Ouput processing subsystem in the 
 * driver which multiplexes the command and protocol streams.
 * Does not required carrier network (SIM and activation).
 *****************************************************************************/

#define HOST_FEATHER_UXPLOR
#include <platform_pins.h>
#include <ltem1c.h>

#define _DEBUG
#include "dbgprint.h"
//#define USE_SERIAL 0


// test environment
const int APIN_RANDOMSEED = 7;


void setup() {
    #ifdef USE_SERIAL
        Serial.begin(115200);
        #if (USE_SERIAL)
        while (!Serial) {}
        #else
        delay(1000);
        #endif
    #endif

    PRINTF(dbgColor_none, "LTEm1c Test: 3-iop\r\n");          // same as color=0
    gpio_openPin(LED_BUILTIN, gpioMode_output);
    
    randomSeed(analogRead(APIN_RANDOMSEED));

    PRINTF(dbgColor_info, "Base: FreeMem=%u\r\n", getFreeMemory());
    ltem1_create(ltem1_pinConfig, ltem1Start_powerOn, ltem1Functionality_iop);
    PRINTF(dbgColor_info, "LTEm1 Initialized: FreeMem=%u\r\n", getFreeMemory());
}

int loopCnt = 0;

void loop() 
{
    uint8_t regValue = 0;
    // char cmd[] = "AT+GSN\r\0";                           // short response (contained in 1 rxCtrlBlock)
    char cmd[] = "AT+GSN;+QCCID;+GSN;+QCCID\r\0";           // long response (requires multiple rxCtrlBlocks)
    // char cmd[] = "at+cgpaddr\r\0";                       // long response (requires multiple rxCtrlBlocks)
    // char cmd[] = "AT+QIDNSGIP=1,\"www.loouq.com\"\r\0";
    // char cmd[] = "AT+QPOWD\r\0";                         // something is wrong! Is is rx or tx (tx works if BG powers down)
    PRINTF(0, "Invoking cmd: %s \r\n", cmd);

    iop_resetCmdRespBuffer();                               // send command
    iop_txSend(cmd, strlen(cmd), false);
    PRINTF(0, "CmdSent\r\n");

    timing_delay(100);                                      // give BGx time to respond
    // timing_delay(5000);                                  // give BGx time to respond, longer for network

    int len = strlen(g_ltem1->iop->rxCmdBuf->buffer);
    PRINTF(dbgColor_green, "Got %d chars (so far)\r", len);
    PRINTF(dbgColor_cyan, "Resp: %s\r", g_ltem1->iop->rxCmdBuf->buffer);  

    loopCnt ++;
    indicateLoop(loopCnt, 1000);      // BG reference says 300mS cmd response time, we will wait 1000
}



/* test helpers
========================================================================================================================= */

void indicateLoop(int loopCnt, int waitNext) 
{
    PRINTF(dbgColor_info, "\r\nLoop=%i \r\n", loopCnt);
    PRINTF(0, "FreeMem=%u\r\n", getFreeMemory());
    PRINTF(0, "NextTest (millis)=%i\r\r", waitNext);
    timing_delay(waitNext);
}


void indicateFailure(char failureMsg[])
{
	PRINTF(dbgColor_error, "\r\n** %s \r", failureMsg);
    PRINTF(dbgColor_error, "** Test Assertion Failed. \r");

    #if 1
    PRINTF(dbgColor_error, "** Halting Execution \r");
    while (1)
    {
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        timing_delay(1000);
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        timing_delay(100);
    }
    #endif
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

