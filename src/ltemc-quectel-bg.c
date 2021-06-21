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
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#endif


#include "ltemc.h"

#define BG96_INIT_COMMAND_COUNT 1
const char* const qbg_initCmds[] = 
{ 
    "ATE0",             // don't echo AT commands on serial
};


#pragma region private functions
/* --------------------------------------------------------------------------------------------- */


#pragma endregion


#pragma region public functions
/* --------------------------------------------------------------------------------------------- */


/**
 *	\brief Check for BGx power status.
 *  \returns BGx power state
 */
bool qbg_isPowerOn()
{
    return gpio_readPin(g_ltem->pinConfig.statusPin);
}


/**
 *	\brief Power on the BGx module.
 *  \returns Prior BGx power state: true=previously powered on
 */
void qbg_powerOn()
{
    if (gpio_readPin(g_ltem->pinConfig.statusPin))
    {
        PRINTF(DBGCOLOR_dGreen, "LTEm1 already powered on.");
        g_ltem->qbgReadyState = qbg_readyState_powerOn;
        return true;
    }

    PRINTF(DBGCOLOR_dGreen, "Powering LTEm1 On...");
    gpio_writePin(g_ltem->pinConfig.powerkeyPin, gpioValue_high);
    lDelay(QBG_POWERON_DELAY);
    gpio_writePin(g_ltem->pinConfig.powerkeyPin, gpioValue_low);

    uint8_t attempts = 0;
    while (attempts++ < 10)             // wait for status=ready
    {
        if (gpio_readPin(g_ltem->pinConfig.statusPin))
        {
            g_ltem->qbgReadyState = qbg_readyState_powerOn;
            PRINTF(DBGCOLOR_dGreen, "DONE\r");
            return false;
        }
        lDelay(500);                    // allow background tasks to operate
    }
    PRINTF(DBGCOLOR_warn, "FAILED\r");
    return false;
}


/**
 *	\brief Powers off the BGx module.
 */
void qbg_powerOff()
{
    PRINTF(dbgColor_none, "Powering LTEm1 Off\r");
	gpio_writePin(g_ltem->pinConfig.powerkeyPin, gpioValue_high);
	lDelay(QBG_POWEROFF_DELAY);
	gpio_writePin(g_ltem->pinConfig.powerkeyPin, gpioValue_low);

    while (gpio_readPin(g_ltem->pinConfig.statusPin))
    {
        lDelay(500);          // allow background tasks to operate
    }
}


/**
 *	\brief Powers off the BGx module.
 */
void qbg_reset()
{
    PRINTF(dbgColor_none, "Reseting LTEm1\r");
    g_ltem->qbgReadyState = qbg_readyState_powerOn;
    iop_txSend("AT\r", 3, true);
    iop_txSend("AT+CFUN=1,1\r", 11, true);
    while (!gpio_readPin(g_ltem->pinConfig.statusPin))
    {
        lDelay(500);          // allow background tasks to operate
    }
}


#define INIT_MAX_ATTEMPTS 2
/**
 *	\brief Initializes the BGx module.
 */
void qbg_start()
{
    uint8_t attempts = 0;
    atcmdResult_t atResult = {.statusCode = RESULT_CODE_TIMEOUT};

    qbg_startRetry:

    // toss out an empty AT command to flush any debris in the command channel
    atcmd_tryInvoke("AT");
    atcmd_awaitResult(true);

    // init BGx state
    for (size_t i = 0; i < BG96_INIT_COMMAND_COUNT; i++)
    {

        if (atcmd_tryInvoke(qbg_initCmds[i]))
        {
            if (atcmd_awaitResult(true).statusCode != RESULT_CODE_SUCCESS)
            {
                while(attempts < INIT_MAX_ATTEMPTS)
                {
                    attempts++;
                    PRINTF(dbgColor_warn, "BGx reseting: init failed!\r");
                    qbg_powerOff();
                    qbg_powerOn();
                    iop_awaitAppReady();
                    goto qbg_startRetry;
                }
                ltem_notifyApp(ltemNotifType_hwInitFailed, "qbg-start() init sequence failed");
            }
        }
    }
}



/**
 *  \brief Configure RAT searching sequence
*/
void qbg_setNwScanSeq(const char* sequence)
{
    //AT+QCFG="nwscanseq"[,<scanseq>[,effect]]
    /*
    <scanseq> Number format. RAT search sequence.
    (e.g.: 020301 stands for LTE Cat M1 | LTE Cat NB1 | GSM))
        00 Automatic (LTE Cat M1 | LTE Cat NB1 | GSM)
        01 GSM
        02 LTE Cat M1
        03 LTE Cat NB1
    <effect> Number format. When to take effect.
        0 Take effect after UE reboots
        1 Take effect immediately
    */
    char atCmd[DFLT_ATBUFSZ] = {0};
    snprintf(atCmd, DFLT_ATBUFSZ, "AT+QCFG=\"nwscanseq\",%s", sequence);
    atcmd_tryInvoke(atCmd);
    atcmd_awaitResult(true);
}


/** 
 *  \brief Configure RAT(s) allowed to be searched
*/
void qbg_setNwScanMode(qbg_nw_scan_mode_t mode)
{
    // AT+QCFG="nwscanmode"[,<scanmode>[,<effect>]]
    /*
    <scanmode> Number format. RAT(s) to be searched.
        0 Automatic
        1 GSM only
        3 LTE only
    <effect> Number format. When to take effect.
        0 Take effect after UE reboots
        1 Take effect immediately    
    */
    char atCmd[DFLT_ATBUFSZ] = {0};
    snprintf(atCmd, DFLT_ATBUFSZ, "AT+QCFG=\"nwscanmode\",%d", mode);
    atcmd_tryInvoke(atCmd);
    atcmd_awaitResult(true);
}



/** 
 *  \brief Configure the network category to be searched under LTE RAT.
 */
void qbg_setIotOpMode(qbg_nw_iot_mode_t mode)
{
    //AT+QCFG="iotopmode"[,<mode>[,<effect>]]
    /*
    <mode> Number format. Network category to be searched under LTE RAT.
        0 LTE Cat M1
        1 LTE Cat NB1
        2 LTE Cat M1 and Cat NB1
    <effect> Number format. When to take effect.
        0 Take effect after UE reboots
        1 Take effect immediately
    */
    char atCmd[DFLT_ATBUFSZ] = {0};
    snprintf(atCmd, DFLT_ATBUFSZ, "AT+QCFG=\"iotopmode\",%d", mode);
    atcmd_tryInvoke(atCmd);
    atcmd_awaitResult(true);
}

#pragma endregion
