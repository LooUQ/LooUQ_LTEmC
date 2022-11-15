/******************************************************************************
 *  \file ltemc-quectel-bg.c
 *  \author Jensen Miller, Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020,2021 LooUQ Incorporated.
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
 * Manages module genaral and non-protocol cellular radio functions
 *****************************************************************************/

#define _DEBUG 2                        // set to non-zero value for PRINTF debugging output, 
// debugging output options             // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if defined(_DEBUG) && _DEBUG > 0
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG == 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #define PRINTF(c_,f_,__VA_ARGS__...) do { rtt_printf(c_, (f_), ## __VA_ARGS__); } while(0)
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#endif

#include "ltemc-quectel-bg.h"
#include "ltemc-internal.h"

extern ltemDevice_t g_lqLTEM;

extern const char* const qbg_initCmds[];
extern int8_t qbg_initCmdsCnt;


/* Private static functions
 --------------------------------------------------------------------------------------------- */
bool S__issueStartCommand(const char *cmdStr);
bool S__statusFix();


#pragma region public functions
/* --------------------------------------------------------------------------------------------- */


/**
 * 	@brief Check for BGx power status
 */
bool qbg_isPowerOn()
{
    gpioPinValue_t statusPin = platform_readPin(g_lqLTEM.pinConfig.statusPin);
    #ifdef STATUS_LOW_PULLDOWN
    if (statusPin)                     // if pin high, assume latched
    {
        platform_closePin(g_lqLTEM.pinConfig.statusPin);
        platform_openPin(g_lqLTEM.pinConfig.statusPin, gpioMode_output);    // open status for write, set low
        platform_writePin(g_lqLTEM.pinConfig.statusPin, gpioValue_low);     // set low
        platform_closePin(g_lqLTEM.pinConfig.statusPin);
        platform_openPin(g_lqLTEM.pinConfig.statusPin, gpioMode_input);     // reopen for normal usage (read)

        statusPin = platform_readPin(g_lqLTEM.pinConfig.statusPin);                     // perform 2nd read, after pull-down sequence
    }
    return statusPin;
    #else
    return platform_readPin(g_lqLTEM.pinConfig.statusPin);
    #endif
}


/**
 *	@brief Power on the BGx module
 */
bool qbg_powerOn()
{
    if (qbg_isPowerOn())
    {
        PRINTF(dbgColor__none, "LTEm found powered on.\r");
        g_lqLTEM.deviceState = deviceState_appReady;             // APP READY msg comes only once, shortly after chip start, would have missed it 
        return true;
    }

    g_lqLTEM.deviceState = deviceState_powerOff;

    PRINTF(dbgColor__none, "Powering LTEm On...");
    platform_writePin(g_lqLTEM.pinConfig.powerkeyPin, gpioValue_high);
    pDelay(BGX__powerOnDelay);
    platform_writePin(g_lqLTEM.pinConfig.powerkeyPin, gpioValue_low);

    uint8_t waitAttempts = 0;
    while (waitAttempts++ < 60)                                     // wait for status=ready, HW Guide says 4.8s
    {
        if (qbg_isPowerOn())
        {
            g_lqLTEM.deviceState = deviceState_powerOn;
            PRINTF(dbgColor__none, "DONE\r");
            return true;
        }
        else
            pDelay(100);                                            // allow background tasks to operate
    }
    return false;
}


/**
 *	@brief Powers off the BGx module.
 */
void qbg_powerOff()
{
    PRINTF(dbgColor__none, "Powering LTEm Off...");
	platform_writePin(g_lqLTEM.pinConfig.powerkeyPin, gpioValue_high);
	pDelay(BGX__powerOffDelay);
	platform_writePin(g_lqLTEM.pinConfig.powerkeyPin, gpioValue_low);

    uint8_t waitAttempts = 0;
    while (waitAttempts++ < 60)
    {
        if (!qbg_isPowerOn())
        {
            g_lqLTEM.deviceState = deviceState_powerOn;
            PRINTF(dbgColor__none, "DONE\r");
            return true;
        }
        else
            pDelay(100);                                            // allow background tasks to operate
    }
}


/**
 *	@brief Perform a hardware (pin)software reset of the BGx module
 */
void qbg_reset(resetAction_t resetAction)
{
    // if (resetAction == skipIfOn)
    // {fall through}

    if (resetAction == swReset)
    {
        atcmd_sendCmdData("AT+CFUN=1,1\r", 12, "");                     // soft-reset command: performs a module internal HW reset and cold-start

        uint32_t waitStart = pMillis();                                 // start timer to wait for status pin == OFF
        while (qbg_isPowerOn())
        {
            yield();                                                    // give application some time back for processing
            if (pMillis() - waitStart > PERIOD_FROM_SECONDS(3))
            {
                PRINTF(dbgColor__warn, "LTEm swReset:OFF timeout\r");
                qbg_reset(powerReset);                                  // recursive call with power-cycle reset specified
                return;
            }
        }

        waitStart = pMillis();                                          // start timer to wait for status pin == ON
        while (!qbg_isPowerOn())
        {
            yield();                                                    // give application some time back for processing
            if (pMillis() - waitStart > PERIOD_FROM_SECONDS(3))
            {
                PRINTF(dbgColor__warn, "LTEm swReset:ON timeout\r");
                return;
            }
        }
        PRINTF(dbgColor__white, "LTEm swReset\r");
    }
    else if (resetAction == hwReset)
    {
        platform_writePin(g_lqLTEM.pinConfig.resetPin, gpioValue_high);                 // hardware reset: reset pin (LTEm inverts)
        pDelay(4000);                                                                   // BG96: active for 150-460ms , BG95: 2-3.8s
        platform_writePin(g_lqLTEM.pinConfig.resetPin, gpioValue_low);
        PRINTF(dbgColor__white, "LTEm hwReset\r");
    }
    else // if (resetAction == powerReset)
    {
        qbg_powerOff();                                                     
        pDelay(500);
        qbg_powerOn();
        PRINTF(dbgColor__white, "LTEm pwrReset\r");
    }
}


/**
 *	@brief Initializes the BGx module
 */
void qbg_setOptions()
{
    atcmd_tryInvoke("AT");
    if (atcmd_awaitResult() != resultCode__success)
    {
        /*  If above awaitResult gets a 408 (timeout), perform a HW check. The HW check 1st looks at status pin,the inspects
         *  SPI for basic Wr/Rd. 
         *  *** If either of the HW tests fail, flow will not get here. ***
         *  The check below attempts to clear a confused BGx that is sitting in data state (awaiting an end-of-transmission).
         */
        if (!qbg_clrDataState())
            ltem_notifyApp(appEvent_fault_hardFault, "BGx init cmd fault");         // send notification, maybe app can recover
    }

    // init BGx state
    PRINTF(dbgColor__none, "BGx Init:\r");
    for (size_t i = 0; i < qbg_initCmdsCnt; i++)                                             // sequence through list of start cmds
    {
        if (!S__issueStartCommand(qbg_initCmds[i]))
        {
            ltem_notifyApp(appEvent_fault_hardFault, "BGx init seq fault");         // send notification, maybe app can recover
            while (true) {}                                                         // should not get here!
        }
        PRINTF(dbgColor__none, " > %s", qbg_initCmds[i]);
    }
    PRINTF(dbgColor__none, "\r");
}


/**
 *	@brief Attempts recovery of command control of the BGx module left in data mode
 */
bool qbg_clrDataState()
{
    IOP_sendTx("\x1B", 1, true);            // send ASCII ESC
    atcmd_close();
    atcmd_tryInvoke("AT");
    resultCode_t result = atcmd_awaitResult();
    return  result == resultCode__success;
}


/**
 *	@brief Initializes the BGx module.
 */
const char *qbg_getModuleType()
{
    return g_lqLTEM.moduleType;
}


#pragma endregion


bool S__issueStartCommand(const char *cmdStr)
{
    ATCMD_reset(true);
    if (atcmd_tryInvoke(cmdStr) && atcmd_awaitResult() == resultCode__success)
        return true;
    return false;
}

