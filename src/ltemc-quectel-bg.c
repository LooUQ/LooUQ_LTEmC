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

#define _DEBUG 0                        // set to non-zero value for PRINTF debugging output, 
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


enum
{
    BGX__initCommandCnt = 1,
    BGX__initCommandAttempts = 2,
    BGX__powerOnDelay = 500,
    BGX__powerOffDelay = 1500,
    BGX__resetDelay = 300,
    BGX__baudRate = 115200
};

const char* const qbg_initCmds[] = 
{ 
    "ATE0",             // don't echo AT commands on serial
};

extern ltemDevice_t g_ltem;


/* Private static functions
 --------------------------------------------------------------------------------------------- */
bool S_issueStartCommand(const char *cmdStr);


#pragma region public functions
/* --------------------------------------------------------------------------------------------- */


/**
 *	\brief Check for BGx power status.
 *  \returns BGx power state
 */
bool qbg_isPowerOn()
{
    return gpio_readPin(g_ltem.pinConfig.statusPin);
}


/**
 *	\brief Power on the BGx module.
 *  \returns Prior BGx power state: true=previously powered on
 */
void qbg_powerOn()
{
    if (qbg_isPowerOn())
    {
        PRINTF(dbgColor__none, "LTEm found powered on.\r");
        g_ltem.qbgReadyState = qbg_readyState_appReady;             // APP READY msg comes only once, shortly after chip start, would have missed it 
    }
    else
    {
        g_ltem.qbgReadyState = qbg_readyState_powerOff;

        PRINTF(dbgColor__none, "Powering LTEm On...");
        gpio_writePin(g_ltem.pinConfig.powerkeyPin, gpioValue_high);
        pDelay(BGX__powerOnDelay);
        gpio_writePin(g_ltem.pinConfig.powerkeyPin, gpioValue_low);

        uint8_t waitAttempts = 0;
        while (waitAttempts++ < 50)                                 // wait for status=ready, HW Guide says 4.8s
        {
            if (qbg_isPowerOn())
            {
                g_ltem.qbgReadyState = qbg_readyState_powerOn;
                PRINTF(dbgColor__none, "DONE\r");
                break;
            }
            else
                pDelay(100);                // allow background tasks to operate
        }
    }
}


/**
 *	\brief Powers off the BGx module.
 */
void qbg_powerOff()
{
    PRINTF(dbgColor__none, "Powering LTEm Off\r");
	gpio_writePin(g_ltem.pinConfig.powerkeyPin, gpioValue_high);
	pDelay(BGX__powerOffDelay);
	gpio_writePin(g_ltem.pinConfig.powerkeyPin, gpioValue_low);

    while (gpio_readPin(g_ltem.pinConfig.statusPin))
    {
        pDelay(100);          // allow background tasks to operate
    }
}


/**
 *	\brief Initializes the BGx module.
 */
void qbg_setOptions()
{
    atcmd_tryInvoke("AT");
    if (atcmd_awaitResult() != resultCode__success)
    {
        ltem_notifyApp(lqNotifType_hardFault, "qbg-start() BGx is non-responsive");             // send notification, maybe app can recover
    }

    // init BGx state
    for (size_t i = 0; i < BGX__initCommandCnt; i++)                                            // sequence through list of start cmds
    {
        if (!S_issueStartCommand(qbg_initCmds[i]))
        {
            ltem_notifyApp(lqNotifType_hardFault, "qbg-start() init sequence failed");          // send notification, maybe app can recover
            while (true) {}                                                                     // should not get here!
        }
    }
}



#pragma endregion


bool S_issueStartCommand(const char *cmdStr)
{
    atcmd_reset(true);
    if (atcmd_tryInvoke(cmdStr) && atcmd_awaitResult() == resultCode__success)
        return true;

    return false;
}
