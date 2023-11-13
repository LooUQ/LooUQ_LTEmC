/** ****************************************************************************
  \file 
  \brief LTEmC INTERNAL BGx basic functions
  \author Greg Terrell LooUQ Incorporated

  \loouq

  \warning This source unit is low-level processing code. Updates should only 
  be performed as directed by LooUQ.
--------------------------------------------------------------------------------

    This project is released under the GPL-3.0 License.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 
***************************************************************************** */

#define SRCFILE "BGX"                       // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT
//#define ENABLE_DIAGPRINT                    // expand DPRINT into debug output
//#define ENABLE_DIAGPRINT_VERBOSE            // expand DPRINT and DPRINT_V into debug output
#define ENABLE_ASSERT
#include <lqdiag.h>

#include "ltemc-internal.h"
#include "ltemc-quectel-bg.h"
#include "platform\platform-gpio.h"

extern ltemDevice_t g_lqLTEM;

extern const char* const qbg_initCmds[];
extern int8_t qbg_initCmdsCnt;

#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))


/* Private static functions
 --------------------------------------------------------------------------------------------- */
bool S__statusFix();


#pragma region public functions
/* --------------------------------------------------------------------------------------------- */


/**
 * 	@brief Check for BGx power status
 */
bool QBG_isPowerOn()
{
    gpioPinValue_t statusPin = platform_readPin(g_lqLTEM.pinConfig.statusPin);

    #ifdef STATUS_LOW_PULLDOWN
    if (statusPin)                     // if pin high, assume latched
    {
        platform_closePin(g_lqLTEM.pinConfig.statusPin);
        platform_openPin(g_lqLTEM.pinConfig.statusPin, gpioMode_output);    // open status for write, set low
        platform_writePin(g_lqLTEM.pinConfig.statusPin, gpioValue_low);     // set low
        pDelay(1);
        platform_closePin(g_lqLTEM.pinConfig.statusPin);
        platform_openPin(g_lqLTEM.pinConfig.statusPin, gpioMode_input);     // reopen for normal usage (read)

        statusPin = platform_readPin(g_lqLTEM.pinConfig.statusPin);                     // perform 2nd read, after pull-down sequence
    }
    #endif

    g_lqLTEM.deviceState = statusPin ? MAX(deviceState_powerOn, g_lqLTEM.deviceState) : deviceState_powerOff;
    return statusPin;
}


/**
 *	@brief Power on the BGx module
 */
void QBG_powerOn()
{
    if (QBG_isPowerOn())
    {
        DPRINT(PRNT_DEFAULT, "LTEm found powered on\r");
        g_lqLTEM.deviceState = deviceState_ready;                       // Module start messages come only once, shortly after start, would have missed it 
        return;
    }
    g_lqLTEM.deviceState = deviceState_powerOff;

    DPRINT(PRNT_DEFAULT, "Powering LTEm On...");
    platform_writePin(g_lqLTEM.pinConfig.powerkeyPin, gpioValue_high);  // toggle powerKey pin to power on/off
    pDelay(BGX__powerOnDelay);
    platform_writePin(g_lqLTEM.pinConfig.powerkeyPin, gpioValue_low);

    uint8_t waitAttempts = 0;
    while (!QBG_isPowerOn())
    {
        if (waitAttempts++ == 60)
        {
            DPRINT(PRNT_DEFAULT, "FAILED\r");
            return;
        }
        pDelay(100);                                                    // allow background tasks to operate
    }
    g_lqLTEM.deviceState = deviceState_powerOn;
    DPRINT(PRNT_DEFAULT, "DONE\r");
}


/**
 *	@brief Powers off the BGx module.
 */
void QBG_powerOff()
{
    if (!QBG_isPowerOn())
    {
        DPRINT(PRNT_DEFAULT, "LTEm found powered off\r");
        g_lqLTEM.deviceState = deviceState_powerOff;
        return;
    }

    DPRINT(PRNT_DEFAULT, "Powering LTEm Off...");
	platform_writePin(g_lqLTEM.pinConfig.powerkeyPin, gpioValue_high);  // toggle powerKey pin to power on/off
	pDelay(BGX__powerOffDelay);
	platform_writePin(g_lqLTEM.pinConfig.powerkeyPin, gpioValue_low);

    uint8_t waitAttempts = 0;
    while (QBG_isPowerOn())
    {
        if (waitAttempts++ == 60)
        {
            DPRINT(PRNT_DEFAULT, "FAILED\r");
            return;
        }
        pDelay(100);                                                    // allow background tasks to operate
    }
    g_lqLTEM.deviceState = deviceState_powerOff;
    DPRINT(PRNT_DEFAULT, "DONE\r");
}


/**
 *	@brief Perform a hardware (pin)software reset of the BGx module
 */
void QBG_reset(resetAction_t resetAction)
{
    // if (resetAction == skipIfOn)
    // {fall through}

    if (resetAction == resetAction_swReset && QBG_isPowerOn())
    {
        char cmdData[] = "AT+CFUN=1,1\r";                                   // DMA SPI DMA may not tolerate Flash source
        IOP_startTx(cmdData, sizeof(cmdData));                              // soft-reset command: performs a module internal HW reset and cold-start

        uint32_t waitStart = pMillis();                                     // start timer to wait for status pin == OFF
        while (QBG_isPowerOn())
        {
            yield();                                                        // give application some time back for processing
            if (pMillis() - waitStart > PERIOD_FROM_SECONDS(3))
            {
                DPRINT(PRNT_WARN, "LTEm swReset:OFF timeout\r");
                // SC16IS7xx_sendBreak();
                // atcmd_exitTextMode();                                    // clear possible text mode (hung MQTT publish, etc.)
                QBG_reset(resetAction_powerReset);                          // recursive call with power-cycle reset specified
                return;
            }
        }

        waitStart = pMillis();                                              // start timer to wait for status pin == ON
        while (!QBG_isPowerOn())
        {
            yield();                                                        // give application some time back for processing
            if (pMillis() - waitStart > PERIOD_FROM_SECONDS(3))
            {
                DPRINT(PRNT_WARN, "LTEm swReset:ON timeout\r");
                return;
            }
        }
        DPRINT(PRNT_WHITE, "LTEm swReset\r");
    }
    else if (resetAction == resetAction_hwReset)
    {
        platform_writePin(g_lqLTEM.pinConfig.resetPin, gpioValue_high);     // hardware reset: reset pin (LTEm inverts)
        pDelay(4000);                                                       // BG96: active for 150-460ms , BG95: 2-3.8s
        platform_writePin(g_lqLTEM.pinConfig.resetPin, gpioValue_low);
        DPRINT(PRNT_WHITE, "LTEm hwReset\r");
    }
    else // if (resetAction == powerReset)
    {
        QBG_powerOff();                                                     
        pDelay(BGX__resetDelay);
        QBG_powerOn();
        DPRINT(PRNT_WHITE, "LTEm pwrReset\r");
    }
}


/**
 *	@brief Initializes the BGx module
 */
bool QBG_setOptions()
{
    DPRINT(PRNT_DEFAULT, "BGx Init:\r");
    bool setOptionsSuccess = true;
    char cmdBffr[120];

    for (size_t i = 0; i < qbg_initCmdsCnt; i++)                                    // sequence through list of start cmds
    {
        DPRINT(PRNT_DEFAULT, " > %s", qbg_initCmds[i]);
        strcpy(cmdBffr, qbg_initCmds[i]);

        if (atcmd_tryInvoke(cmdBffr))
        {
            if (atcmd_awaitResultWithOptions(2000, NULL) == resultCode__success)    // somewhat unknown cmd list for modem initialization, relax timeout
            {
                continue;
            }
        }
        DPRINT(PRNT_ERROR, "BGx Init CmdError: %s\r", qbg_initCmds[i]);
        setOptionsSuccess = false;
        break;
    }
    DPRINT(PRNT_DEFAULT, " -End BGx Init-\r");
    return setOptionsSuccess;
}


// /**
//  *	@brief Attempts recovery of command control of the BGx module left in data mode
//  */
// bool QBG_clearDataState()
// {
//     IOP_forceTx("\x1B", 1);                                                          // send ASCII ESC

//     atcmd_close();
//     atcmd_tryInvoke("AT");
//     resultCode_t result = atcmd_awaitResult();
//     return  result == resultCode__success;
// }


#pragma endregion

