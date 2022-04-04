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


extern ltemDevice_t g_ltem;


const char* const qbg_initCmds[] = 
{ 
    "ATE0",                             // don't echo AT commands on serial
};


/* Private static functions
 --------------------------------------------------------------------------------------------- */
bool S_issueStartCommand(const char *cmdStr);


#pragma region public functions
/* --------------------------------------------------------------------------------------------- */


/**
 * 	@brief Check for BGx power status
 */
bool qbg_isPowerOn()
{
    return gpio_readPin(g_ltem.pinConfig.statusPin);
}


/**
 *	@brief Power on the BGx module
 */
bool qbg_powerOn()
{
    if (qbg_isPowerOn())
    {
        PRINTF(dbgColor__none, "LTEm found powered on.\r");
        g_ltem.qbgDeviceState = qbgDeviceState_appReady;             // APP READY msg comes only once, shortly after chip start, would have missed it 
        return true;
    }

    g_ltem.qbgDeviceState = qbgDeviceState_powerOff;

    PRINTF(dbgColor__none, "Powering LTEm On...");
    gpio_writePin(g_ltem.pinConfig.powerkeyPin, gpioValue_high);
    pDelay(BGX__powerOnDelay);
    gpio_writePin(g_ltem.pinConfig.powerkeyPin, gpioValue_low);

    uint8_t waitAttempts = 0;
    while (waitAttempts++ < 72)                                     // wait for status=ready, HW Guide says 4.8s
    {
        if (qbg_isPowerOn())
        {
            g_ltem.qbgDeviceState = qbgDeviceState_powerOn;
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
 *	@brief Perform a hardware (pin)software reset of the BGx module
 */
void qbg_reset(bool hwReset)
{
    if (hwReset) 
    {
        qbg_powerOff();                                                 // power cycle the BGx
        pDelay(500);
        qbg_powerOn();

        // gpio_writePin(g_ltem.pinConfig.resetPin, gpioValue_high);       // hardware reset: reset pin active for 150-460ms 
        // pDelay(250);
        // gpio_writePin(g_ltem.pinConfig.resetPin, gpioValue_low);
    }
    else 
    {
        atcmd_sendCmdData("AT+CFUN=1,1\r", 12, "");                     // soft-reset command: performs a module internal HW reset and cold-start
        pDelay(1000);                                                   // delay for status to go inactive, then wait for status active up to timeout
    }

    uint32_t waitStart = pMillis();                                     // start timer to wait for status pin
    while (!gpio_readPin(g_ltem.pinConfig.statusPin))                   // the reset command blocks caller until status=true or timeout
    {
        yield();                                                        // give application some time back for processing
        if (pMillis() - waitStart > PERIOD_FROM_SECONDS(8))
        {
            PRINTF(dbgColor__warn, "Error: BGx reset wait expired!");
            return;
        }
    }
    PRINTF(dbgColor__white, "BGx reset\r");
}


/**
 *	@brief Initializes the BGx module
 */
void qbg_setOptions()
{
    atcmd_tryInvoke("AT");
    if (atcmd_awaitResult() != resultCode__success)
    {
        /*  If above awaitResult gets a 408 (timeout), a HW check will automatically be performed. The HW check
         *  1st looks at status pin, then inspects SPI for basic Wr/Rd. If either of the HW tests fail, flow will not
         *  get here. The check below attempts to clear a confused BGx that is sitting in data state (awaiting an end-of-transmission).
         */
        if (!qbg_clrDataState())
            ltem_notifyApp(appEvent_fault_hardFault, "BGx init cmd fault");           // send notification, maybe app can recover
    }

    // init BGx state
    for (size_t i = 0; i < BGX__initCommandCnt; i++)                            // sequence through list of start cmds
    {
        if (!S_issueStartCommand(qbg_initCmds[i]))
        {
            ltem_notifyApp(appEvent_fault_hardFault, "BGx init seq fault");           // send notification, maybe app can recover
            while (true) {}                                                     // should not get here!
        }
    }
}


/**
 *	@brief Attempts recovery of command control of the BGx module left in data mode
 */
bool qbg_clrDataState()
{
    IOP_sendTx("\x1B", 1, true);            // send ESC - Ctrl-[
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
    return g_ltem.moduleType;
}


#pragma endregion


bool S_issueStartCommand(const char *cmdStr)
{
    ATCMD_reset(true);
    if (atcmd_tryInvoke(cmdStr) && atcmd_awaitResult() == resultCode__success)
        return true;

    return false;
}
