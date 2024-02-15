/** ***************************************************************************
  @file ltemc-quectel-bg.c
  @brief BGx module functions/services.

  @author Greg Terrell, LooUQ Incorporated

  \loouq

  @warning Internal dependencies, changes only as directed by LooUQ staff.

-------------------------------------------------------------------------------

LooUQ-LTEmC // Software driver for the LooUQ LTEm series cellular modems.
Copyright (C) 2017-2023 LooUQ Incorporated

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
Also add information on how to contact you by electronic and paper mail.

**************************************************************************** */


#include <lq-embed.h>
#define lqLOG_LEVEL lqLOGLEVEL_OFF
//#define DISABLE_ASSERTS                                   // ASSERT/ASSERT_W enabled by default, can be disabled 
#define LQ_SRCFILE "BGX"                                    // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT

#include "ltemc-internal.h"
#include "ltemc-quectel-bg.h"
#include <platform\lq-platform_gpio.h>

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
        lqLOG_INFO("LTEm found powered on\r\n");
        g_lqLTEM.deviceState = deviceState_ready;                       // Module start messages come only once, shortly after start, would have missed it 
        return;
    }
    g_lqLTEM.deviceState = deviceState_powerOff;

    lqLOG_INFO("Powering LTEm On...");
    platform_writePin(g_lqLTEM.pinConfig.powerkeyPin, gpioValue_high);  // toggle powerKey pin to power on/off
    pDelay(BGX__powerOnDelay);
    platform_writePin(g_lqLTEM.pinConfig.powerkeyPin, gpioValue_low);

    uint8_t waitAttempts = 0;
    while (!QBG_isPowerOn())
    {
        if (waitAttempts++ == 60)
        {
            lqLOG_ERROR("FAILED\r\n");
            return;
        }
        pDelay(100);                                                    // allow background tasks to operate
    }
    g_lqLTEM.deviceState = deviceState_powerOn;
    lqLOG_INFO("DONE\r\n");
}


/**
 *	@brief Powers off the BGx module.
 */
void QBG_powerOff()
{
    if (!QBG_isPowerOn())
    {
        lqLOG_INFO("LTEm found powered off\r\n");
        g_lqLTEM.deviceState = deviceState_powerOff;
        return;
    }

    lqLOG_INFO("Powering LTEm Off...");
	platform_writePin(g_lqLTEM.pinConfig.powerkeyPin, gpioValue_high);  // toggle powerKey pin to power on/off
	pDelay(BGX__powerOffDelay);
	platform_writePin(g_lqLTEM.pinConfig.powerkeyPin, gpioValue_low);

    uint8_t waitAttempts = 0;
    while (QBG_isPowerOn())
    {
        if (waitAttempts++ == 60)
        {
            lqLOG_ERROR("FAILED\r\n");
            return;
        }
        pDelay(100);                                                    // allow background tasks to operate
    }
    g_lqLTEM.deviceState = deviceState_powerOff;
    lqLOG_INFO("DONE\r\n");
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
        char cmdData[] = "AT+CFUN=1,1\r\n";                                   // DMA SPI DMA may not tolerate Flash source
        IOP_startTx(cmdData, sizeof(cmdData));                              // soft-reset command: performs a module internal HW reset and cold-start

        uint32_t waitStart = pMillis();                                     // start timer to wait for status pin == OFF
        while (QBG_isPowerOn())
        {
            yield();                                                        // give application some time back for processing
            if (pMillis() - waitStart > PERIOD_FROM_SECONDS(3))
            {
                lqLOG_WARN("LTEm swReset:OFF timeout\r\n");
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
                lqLOG_WARN("LTEm swReset:ON timeout\r\n");
                return;
            }
        }
        lqLOG_INFO("LTEm swReset\r\n");
    }
    else if (resetAction == resetAction_hwReset)
    {
        platform_writePin(g_lqLTEM.pinConfig.resetPin, gpioValue_high);     // hardware reset: reset pin (LTEm inverts)
        pDelay(4000);                                                       // BG96: active for 150-460ms , BG95: 2-3.8s
        platform_writePin(g_lqLTEM.pinConfig.resetPin, gpioValue_low);
        lqLOG_INFO("LTEm hwReset\r\n");
    }
    else // if (resetAction == powerReset)
    {
        QBG_powerOff();                                                     
        pDelay(BGX__resetDelay);
        QBG_powerOn();
        lqLOG_INFO("LTEm pwrReset\r\n");
    }
}


/**
 *	@brief Initializes the BGx module
 */
bool QBG_setOptions()
{
    lqLOG_INFO("Module Init:\r\n");
    bool setOptionsSuccess = true;
    char cmdBffr[120];

    for (size_t i = 0; i < qbg_initCmdsCnt; i++)                                    // sequence through list of start cmds
    {
        lqLOG_INFO(" > %s", qbg_initCmds[i]);
        strcpy(cmdBffr, qbg_initCmds[i]);

      	atcmd_ovrrdDCmpltTimeout(SEC_TO_MS(2));
        if (IS_SUCCESS(atcmd_dispatch(cmdBffr)))
        {
            continue;
        }
        lqLOG_ERROR("BGx Init CmdError: %s\r\n", qbg_initCmds[i]);
        setOptionsSuccess = false;
        break;
    }
    lqLOG_INFO(" -End BGx Init-\r\n");
    return setOptionsSuccess;
}

#pragma endregion

