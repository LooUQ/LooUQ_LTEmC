/******************************************************************************
 *  \file LTEmC-4-atcmd.ino
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
 * Test 4: AT command automatic parsing and result output.
 * 
 * The sketch is designed for debug output to observe results.
 *****************************************************************************/


#define _DEBUG 2                        // set to non-zero value for PRINTF debugging output, 
// debugging output options             // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if defined(_DEBUG)
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #define PRINTF(c_,f_,__VA_ARGS__...) do { rtt_printf(c_, (f_), ## __VA_ARGS__); } while(0)
    #else
    #define SERIAL_DBG _DEBUG           // enable serial port output using devl host platform serial, _DEBUG 0=start immediately, 1=wait for port
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#endif

// define options, specify the pin configuration
#define HOST_FEATHER_UXPLOR_L
// #define HOST_FEATHER_UXPLOR
// #define HOST_FEATHER_LTEM3F

#include <ltemc.h>

#define STRCMP(x,y)  (strcmp(x, y) == 0)


void setup() 
{
    #ifdef SERIAL_OPT
        Serial.begin(115200);
        #if (SERIAL_OPT > 0)
        while (!Serial) {}      // force wait for serial ready
        #else
        delay(5000);            // just give it some time
        #endif
    #endif

    PRINTF(dbgColor__red, "\r\rLTEmC - Test #4: AT Commands\r");
    lqDiag_setNotifyCallback(applEvntNotify);                           // enable ASSERTS to callback into application

    ltem_create(ltem_pinConfig, NULL, applEvntNotify);                  // create LTEmC modem
    ltem_start(resetAction_swReset);                                    // ... and start it
}


int loopCnt = 0;

void loop() 
{
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
    char cmdStr[] = "ATI";
    // char cmdStr[] = "AT+CPIN?";
    // char cmdStr[] = "AT+QCCID";
    PRINTF(dbgColor__none, "Invoking cmd: %s \r\n", cmdStr);

    if (atcmd_tryInvoke(cmdStr))
    {
        resultCode_t atResult = atcmd_awaitResult();
        
            char *response = atcmd_getRawResponse();
            PRINTF(dbgColor__info, "Got %d chars\r", strlen(response));
            PRINTF(dbgColor__white, "Resp:");
            PRINTF(dbgColor__cyan, "%s\r", response);
                                                                                            // test response v. expected 
            char *validResponse = "\r\nQuectel\r\nBG";                                      // near beginning (depends on BGx echo)
            if (!strstr(response, validResponse))
                indicateFailure("Expected cmd response missing... failed."); 

        if (atResult != resultCode__success)                                                // statusCode == 200 (similar to HTTP codes)
        {
            PRINTF(dbgColor__error, "atResult=%d \r", atResult);
            // indicateFailure("Unexpected command response... failed."); 
            indicateFailure("Invalid BGx response");
        }

        /* atcmd_close();       Not needed here since tryInvokeDefaults(). 
                                With options and manual lock, required when done with response to close action and release action lock */
    }
    else
        PRINTF(dbgColor__warn, "Unable to get action lock.\r");

    loopCnt ++;
    indicateLoop(loopCnt, random(1000));
}


/* test helpers
========================================================================================================================= */


void applEvntNotify(appEvents_t eventType, const char *notifyMsg)
{
    if (eventType == appEvent_fault_assertFailed)
        PRINTF(dbgColor__error, "LTEmC-HardFault: %s\r", notifyMsg);
    else 
        PRINTF(dbgColor__white, "LTEmC Info: %s\r", notifyMsg);
    return;
}


void indicateFailure(char failureMsg[])
{
	PRINTF(dbgColor__error, "\r\n** %s \r", failureMsg);
    PRINTF(dbgColor__error, "** Test Assertion Failed. \r");

    #if 1
    PRINTF(dbgColor__error, "** Halting Execution \r\n");
    bool halt = true;
    while (halt)
    {
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        pDelay(1000);
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        pDelay(100);
    }
    #endif
}


void indicateLoop(int loopCnt, int waitNext) 
{
    PRINTF(dbgColor__info, "\r\nLoop=%i \r\n", loopCnt);

    for (int i = 0; i < 6; i++)
    {
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        pDelay(50);
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        pDelay(50);
    }

    PRINTF(dbgColor__magenta, "FreeMem=%u\r\n", getFreeMemory());
    PRINTF(dbgColor__none, "NextTest (millis)=%i\r\r", waitNext);
    pDelay(waitNext);
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

