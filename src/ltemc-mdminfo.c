/** ****************************************************************************
  \file 
  \brief Public API get modem information
  \author Greg Terrell, LooUQ Incorporated

  \loouq

  \note This module provides user/application level API to modem functions.

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


#define SRCFILE "MDM"                       // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT
//#define ENABLE_DIAGPRINT                    // expand DIAGPRINT into debug output
//#define ENABLE_DIAGPRINT_VERBOSE            // expand DIAGPRINT and DIAGPRINT_V into debug output
#define ENABLE_ASSERT
#include <lqdiag.h>

#include "ltemc-internal.h"

extern ltemDevice_t g_lqLTEM;


#define MIN(x, y) (((x)<(y)) ? (x):(y))
#define MAX(x, y) (((x)>(y)) ? (x):(y))


// private local declarations
static cmdParseRslt_t S__iccidCompleteParser(ltemDevice_t *modem);


/* Public functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions


/**
 *  \brief Get the LTEm1 static device identification/provisioning information.
*/
modemInfo_t *mdminfo_ltem()
{
    if (ATCMD_awaitLock(atcmd__defaultTimeout))
    {
        if (g_lqLTEM.modemInfo->imei[0] == 0)
        {
            atcmd_invokeReuseLock("AT+GSN");
            if (atcmd_awaitResult() == resultCode__success)
            {
                strncpy(g_lqLTEM.modemInfo->imei, atcmd_getResponse(), ntwk__imeiSz);
            }
        }

        if (g_lqLTEM.modemInfo->fwver[0] == 0)
        {
            atcmd_invokeReuseLock("AT+QGMR");
            if (atcmd_awaitResult() == resultCode__success)
            {
                char * eol;
                if ((eol = strstr(atcmd_getResponse(), "\r\n")) != NULL)
                {
                    uint8_t sz = eol - atcmd_getResponse();
                    memcpy(g_lqLTEM.modemInfo->fwver, atcmd_getResponse(), MIN(sz, ntwk__dvcFwVerSz));
                }
            }
        }

        if (g_lqLTEM.modemInfo->mfgmodel[0] == 0)
        {
            atcmd_invokeReuseLock("ATI");
            if (atcmd_awaitResult() == resultCode__success)
            {
                char * eol;
                if ((eol = strstr(atcmd_getResponse(), "\r\nRevision")) != NULL)
                {
                    uint8_t sz = eol - atcmd_getResponse();
                    memcpy(g_lqLTEM.modemInfo->mfgmodel, atcmd_getResponse(), MIN(sz, ntwk__dvcMfgSz));
                    *(strchr(g_lqLTEM.modemInfo->mfgmodel, '\r')) = ':';
                    *(strchr(g_lqLTEM.modemInfo->mfgmodel, '\n')) = ' ';
                }
            }
        }

        if (g_lqLTEM.modemInfo->iccid[0] == 0)
        {
            atcmd_invokeReuseLock("AT+ICCID");
            if (atcmd_awaitResultWithOptions(atcmd__defaultTimeout, S__iccidCompleteParser) == resultCode__success)
            {
                strncpy(g_lqLTEM.modemInfo->iccid, atcmd_getResponse(), ntwk__iccidSz);
            }
        }
        atcmd_close();
    }
    return (modemInfo_t*)(g_lqLTEM.modemInfo);
}


/**
 *  \brief Get the signal strength reported by the LTEm device at a percent
*/
uint8_t mdmInfo_signalPercent()
{
    double csq;
    uint8_t signal = 0;
    const double csqFactor = 3.23;

    if (ltem_getDeviceState())
    {
        if (atcmd_tryInvoke("AT+CSQ"))
        {
            if (atcmd_awaitResult() == resultCode__success)
            {
                char *term;
                char *lastResponse = atcmd_getResponse();
                term = strstr(atcmd_getResponse(), "+CSQ");
                csq = strtod(term + 6, NULL);
            }
            // CSQ 99=no signal, range -51(best)  to -113(worst)
            signal = (csq == 99) ? 0 : (uint8_t)(csq * csqFactor);
            atcmd_close();
        }
    }
    return signal;
}


/**
 *  \brief Get the static device provisioning information about the LTEm1.
*/
int16_t mdminfo_signalRSSI()
{
    const int8_t rssiBase = -113;
    const int8_t rssiRange = 113 - 51;

    uint8_t signalPercent = mdmInfo_signalPercent();
    return (signalPercent == 0) ? rssiBase : (signalPercent * 0.01 * rssiRange) + rssiBase;
}


/** 
 *  \brief Get the signal strength, as a bar count for visualizations, (like on a smartphone) 
 * */
uint8_t mdminfo_signalBars(uint8_t displayBarCount)
{
    const int8_t barOffset = 20;                                // adjust point for full-bar percent (20 = full bar count at 80%)

    uint8_t barSpan = 100 / displayBarCount;
    uint8_t signalPercent = MIN(mdmInfo_signalPercent() + barOffset, 100);
    return (uint8_t)(signalPercent / barSpan);
}

#pragma endregion



#pragma region private static functions
/* --------------------------------------------------------------------------------------------- */


/**
 *	\brief Action response parser for iccid value request. 
 */
static cmdParseRslt_t S__iccidCompleteParser(ltemDevice_t *modem)
{
    return atcmd_stdResponseParser("+ICCID: ", true, "", 0, 0, "\r\n\r\nOK\r\n", 20);
}


#pragma endregion
