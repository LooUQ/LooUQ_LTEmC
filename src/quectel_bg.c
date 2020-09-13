/******************************************************************************
 *  \file quectel_bg.c
 *  \author Jensen Miller, Greg Terrell
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
 * Manages module genaral and non-protocol cellular radio functions,
 *****************************************************************************/

#include "ltem1c.h"
//#include "quectel_bg96.h"

#define _DEBUG
#include "dbgprint.h"

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
 *	\brief Power on the BGx module.
 */
void qbg_powerOn()
{
    PRINTF(dbgColor_none, "Powering LTEm1 On...");
    gpio_writePin(g_ltem1->gpio->powerkeyPin, gpioValue_high);
    timing_delay(QBG_POWERON_DELAY);
    gpio_writePin(g_ltem1->gpio->powerkeyPin, gpioValue_low);

    // wait for status=ready
    while (!gpio_readPin(g_ltem1->gpio->statusPin))
    {
        timing_delay(500);
    }
    PRINTF(dbgColor_none, "DONE\r");
}



/**
 *	\brief Powers off the BGx module.
 */
void qbg_powerOff()
{
    PRINTF(dbgColor_none, "Powering LTEm1 Off\r");
	gpio_writePin(g_ltem1->gpio->powerkeyPin, gpioValue_high);
	timing_delay(QBG_POWEROFF_DELAY);
	gpio_writePin(g_ltem1->gpio->powerkeyPin, gpioValue_low);
}



void qbg_start()
{
    uint8_t attempts = 0;
    char response[ACTION_DEFAULT_RESPONSE_SZ] = {0};
    actionResult_t cmdResult = ACTION_RESULT_TIMEOUT;

    qbg_startRetry:

    // toss out an empty AT command to flush any debris in the command channel
    action_tryInvoke("AT", false);
    (void)action_awaitResult(response, ACTION_DEFAULT_RESPONSE_SZ, 0, NULL);

    for (size_t i = 0; i < BG96_INIT_COMMAND_COUNT; i++)
    {

        if (action_tryInvoke(qbg_initCmds[i], false))
        {
            cmdResult = action_awaitResult(response, ACTION_DEFAULT_RESPONSE_SZ, 0, NULL);
        }

        if (cmdResult != ACTION_RESULT_SUCCESS)
        {
            if (attempts == 0)
            {
                attempts++;
                PRINTF(dbgColor_warn, "BGx reseting: init failed!");
                qbg_powerOff();
                timing_millis(100);
                qbg_powerOn();
                goto qbg_startRetry;
            }
            ltem1_faultHandler(cmdResult, "qbg-start() init sequence failed");
        }
    }
}



/**
 *  \brief Configure RAT searching sequence
*/
/*
    AT+QCFG="nwscanseq"[,<scanseq>[,effect]]

    <scanseq> Number format. RAT search sequence.
    (e.g.: 020301 stands for LTE Cat M1  LTE Cat NB1  GSM))
        00 Automatic (LTE Cat M1  LTE Cat NB1  GSM)
        01 GSM
        02 LTE Cat M1
        03 LTE Cat NB1
    <effect> Number format. When to take effect.
        0 Take effect after UE reboots
        1 Take effect immediately
*/
void qbg_setNwScanSeq(const char* sequence)
{
}


/** 
 *  \brief Configure RAT(s) allowed to be searched
*/
/*
    AT+QCFG="nwscanmode"[,<scanmode>[,<effect>]]

    <scanmode> Number format. RAT(s) to be searched.
        0 Automatic
        1 GSM only
        3 LTE only
    <effect> Number format. When to take effect.
        0 Take effect after UE reboots
        1 Take effect immediately
*/
void qbg_setNwScanMode(qbg_nw_scan_mode_t mode)
{
}



/** 
 *  \brief Configure the network category to be searched under LTE RAT.
*/
/*
    AT+QCFG="iotopmode"[,<mode>[,<effect>]]

    <mode> Number format. Network category to be searched under LTE RAT.
        0 LTE Cat M1
        1 LTE Cat NB1
        2 LTE Cat M1 and Cat NB1
    <effect> Number format. When to take effect.
        0 Take effect after UE reboots
        1 Take effect immediately
*/
void qbg_setIotOpMode(qbg_nw_iot_mode_t mode)
{
}



// void qbg_queueUrcStateMsg(const char *message)
// {
// }


void qbg_monitorState()
{
    if (g_ltem1->iop->urcStateMsg[0] == ASCII_cNULL)
        return;

    char *landmarkAt;

    // pdp context deactivated (timeout)
    landmarkAt = strstr(g_ltem1->iop->urcStateMsg, "pdpdeact");
    if (landmarkAt != NULL)
    {
        uint16_t cntxtId = strtol(g_ltem1->iop->urcStateMsg + 11, NULL, 10);
        g_ltem1->network->contexts[cntxtId].contextState = context_state_inactive;
        g_ltem1->network->contexts[cntxtId].ipAddress[0] = ASCII_cNULL;

        PRINTF(dbgColor_warn, "*** PDP Context %d Deactivated", cntxtId);
        ntwk_closeContext(cntxtId);
    }
}


#pragma endregion
