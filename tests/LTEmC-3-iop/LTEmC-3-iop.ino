/******************************************************************************
 *  \file LTEmC-3-iop.ino
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
 * Tests the LTEm interrupt driven Input-Ouput processing subsystem in the 
 * driver which multiplexes the command and protocol streams.
 * Does not require a carrier network (SIM and activation).
 * 
 * The sketch is designed for debug output to observe results.
 *****************************************************************************/

#define _DEBUG 2                        // set to non-zero value for PRINTF debugging output, 
// debugging output options             // LTEmC will satisfy PRINTF references with empty definition if not already resolved
#if defined(_DEBUG) && _DEBUG > 0
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG == 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#endif


// define options for how to assemble this build
#define HOST_FEATHER_UXPLOR             // specify the pin configuration

#include <ltemc.h>


void setup() {
    #ifdef SERIAL_OPT
        Serial.begin(115200);
        #if (SERIAL_OPT > 0)
        while (!Serial) {}      // force wait for serial ready
        #else
        delay(5000);            // just give it some time
        #endif
    #endif

    PRINTF(DBGCOLOR_red, "LTEmC Test3: iop\r");
    gpio_openPin(LED_BUILTIN, gpioMode_output);
    
    randomSeed(analogRead(7));

    PRINTF(DBGCOLOR_info, "Base: FreeMem=%u\r", getFreeMemory());

    ltem_create(ltem_pinConfig, appNotifyCB);           // otherwise reference your application notification callback
    //ltem_create(ltem_pinConfig, NULL);                // if application doesn't implement a notification callback, provide NULL
    ltem_start(pdpProtocol_none);                       // start LTEm with only IOP configured, AT commands but no protocols. 

    PRINTF(DBGCOLOR_info, "LTEm Initialized: FreeMem=%u\r\n", getFreeMemory());
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

    iop_resetCmdBuffer();                                   // send command
    iop_txSend(cmd, strlen(cmd), true);
    PRINTF(0, "CmdSent\r\n");

    lDelay(100);                                            // give BGx time to respond
    // timing_delay(5000);                                  // give BGx time to respond, longer for network

    int len = strlen(g_ltem->iop->rxCmdBuf->buffer);
    PRINTF(DBGCOLOR_green, "Got %d chars (so far)\r", len);
    PRINTF(DBGCOLOR_cyan, "Resp: %s\r", g_ltem->iop->rxCmdBuf->buffer);  

    loopCnt ++;
    indicateLoop(loopCnt, 1000);      // BG reference says 300mS cmd response time, we will wait 1000
}



/* test helpers
========================================================================================================================= */

void appNotifyCB(uint8_t notifType, const char *notifMsg)
{
    if (notifType > 200)
    {
        PRINTF(DBGCOLOR_error, "LQCloud-HardFault: %s\r", notifMsg);
        while (1) {}
    }
    PRINTF(DBGCOLOR_info, "LQCloud Info: %s\r", notifMsg);
    return;
}


void indicateLoop(int loopCnt, int waitNext) 
{
    PRINTF(DBGCOLOR_info, "\r\nLoop=%i \r\n", loopCnt);
    PRINTF(0, "FreeMem=%u\r\n", getFreeMemory());
    PRINTF(0, "NextTest (millis)=%i\r\r", waitNext);
    lDelay(waitNext);
}


void indicateFailure(char failureMsg[])
{
	PRINTF(DBGCOLOR_error, "\r\n** %s \r", failureMsg);
    PRINTF(DBGCOLOR_error, "** Test Assertion Failed. \r");

    #if 1
    PRINTF(DBGCOLOR_error, "** Halting Execution \r");
    while (1)
    {
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        lDelay(1000);
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        lDelay(100);
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

