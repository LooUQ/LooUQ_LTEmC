/******************************************************************************
 *  \file quectel_bg96.c
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

#include "..\ltem1c.h"
//#include "quectel_bg96.h"

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
 *	\brief Power BG96 module on.
 */
void qbg_powerOn()
{
    PRINTF("Powering LTEm1 On...");
    gpio_writePin(g_ltem1->gpio->powerkeyPin, gpioValue_high);
    timing_delay(QBG_POWERON_DELAY);
    gpio_writePin(g_ltem1->gpio->powerkeyPin, gpioValue_low);

    // wait for status=ready
    while (!gpio_readPin(g_ltem1->gpio->statusPin))
    {
        timing_delay(500);
    }
    PRINTF("DONE\r");
}



/**
 *	\brief Powers off the BG96 module.
 */
void qbg_powerOff()
{
    PRINTF("Powering LTEm1 Off\r");
	gpio_writePin(g_ltem1->gpio->powerkeyPin, gpioValue_high);
	timing_delay(QBG_POWEROFF_DELAY);
	gpio_writePin(g_ltem1->gpio->powerkeyPin, gpioValue_low);
}



void qbg_start()
{
    for (size_t i = 0; i < BG96_INIT_COMMAND_COUNT; i++)
    {
        action_invoke(qbg_initCmds[i]);
        action_result_t cmdResult = action_awaitResult(g_ltem1->dAction);

        if (cmdResult != ACTION_RESULT_SUCCESS)
            ltem1_faultHandler("qbg:sendInitCmds init sequence encountered error");
    }
}



void qbg_setNwScanSeq(const char* sequence)
{
}



void qbg_setNwScanMode(qbg_nw_scan_mode_t mode)
{
}



void qbg_setIotOpMode(qbg_nw_iot_mode_t mode)
{
}



// void qbg_queueUrcStateMsg(const char *message)
// {
// }


void qbg_processUrcStateQueue()
{
    if (g_ltem1->iop->urcStateMsg[0] == ASCII_cNULL)
        return;

    char *landmarkAt;

    // pdp context deactivated (timeout)
    landmarkAt = strstr(g_ltem1->iop->urcStateMsg, "pdpdeact");
    if (landmarkAt != NULL)
    {
        uint16_t cntxt = strtol(g_ltem1->iop->urcStateMsg + 11, NULL, 10);
        g_ltem1->network->contexts[cntxt].contextState = context_state_inactive;
        g_ltem1->network->contexts[cntxt].ipAddress[0] = ASCII_cNULL;
    }
}


#pragma endregion
