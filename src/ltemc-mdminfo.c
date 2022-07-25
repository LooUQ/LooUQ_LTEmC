/******************************************************************************
 *  \file ltemc-mdminfo.c
 *  \author Greg Terrell
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
 * Obtain basic modem identification and operational information 
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

#include "ltemc-internal.h"


#define IMEI_OFFSET 2
#define IMEI_SIZE 15
#define ICCID_OFFSET 10
#define ICCID_SIZE 20


extern ltemDevice_t g_lqLTEM;


// private local declarations
static cmdParseRslt_t S_iccidCompleteParser(ltemDevice_t *modem);


/* Public functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions


/**
 *  @brief Get the LTEm1 static device identification/provisioning information.
*/
modemInfo_t *mdminfo_ltem()
{
    if (ATCMD_awaitLock(atcmd__defaultTimeoutMS))
    {
        atcmd_setOptions(atcmd__defaultTimeoutMS, atcmd__useDefaultOKCompletionParser);

        if (((modemInfo_t*)g_lqLTEM.modemInfo)->imei[0] == 0)
        {
            atcmd_invokeReuseLock("AT+GSN");
            if (atcmd_awaitResult() == resultCode__success)
            {
                strncpy(((modemInfo_t*)g_lqLTEM.modemInfo)->imei, atcmd_getLastParsed() + 2, IMEI_SIZE);        // skip leading /r/n
            }
        }

        if (((modemInfo_t*)g_lqLTEM.modemInfo)->fwver[0] == 0)
        {
            atcmd_invokeReuseLock("AT+QGMR");
            if (atcmd_awaitResult() == resultCode__success)
            {
                char *term;
                term = strstr(atcmd_getLastParsed() + 2, "\r\n");
                *term = '\0';
                strcpy(((modemInfo_t*)g_lqLTEM.modemInfo)->fwver, atcmd_getLastParsed() + 2);
                term = strchr(((modemInfo_t*)g_lqLTEM.modemInfo)->fwver, '_');
                *term = ' ';
            }
        }

        if (((modemInfo_t*)g_lqLTEM.modemInfo)->mfgmodel[0] == 0)
        {
            atcmd_invokeReuseLock("ATI");
            if (atcmd_awaitResult() == resultCode__success)
            {
                char *term;
                term = strstr(atcmd_getLastParsed() + 2, "\r\nRev");
                *term = '\0';
                strcpy(((modemInfo_t*)g_lqLTEM.modemInfo)->mfgmodel, atcmd_getLastParsed() + 2);
                term = strchr(((modemInfo_t*)g_lqLTEM.modemInfo)->mfgmodel, '\r');
                *term = ':';
                term = strchr(((modemInfo_t*)g_lqLTEM.modemInfo)->mfgmodel, '\n');
                *term = ' ';
            }
        }

        if (((modemInfo_t*)g_lqLTEM.modemInfo)->iccid[0] == 0)
        {
            atcmd_setOptions(atcmd__defaultTimeoutMS, S_iccidCompleteParser);
            atcmd_invokeReuseLock("AT+ICCID");
            if (atcmd_awaitResult() == resultCode__success)
            {
                strncpy(((modemInfo_t*)g_lqLTEM.modemInfo)->iccid, atcmd_getLastParsed() + ICCID_OFFSET, ICCID_SIZE);
            }
        }
        atcmd_close();
    }
    return (modemInfo_t*)(g_lqLTEM.modemInfo);
}


/**
 *  @brief Get the static device provisioning information about the LTEm1.
*/
int16_t mdminfo_signalRSSI()
{
    uint8_t csq = 0;
    int8_t rssi = -999;

    if (ltem_readDeviceState())
    {
        if (atcmd_tryInvoke("AT+CSQ"))
        {
            if (atcmd_awaitResult() == resultCode__success)
            {
                char *term;
                char *lastResponse = atcmd_getLastParsed();
                term = strstr(atcmd_getLastParsed() + 2, "+CSQ");
                csq = strtol(term + 5, NULL, 10);
            }
            rssi = (csq == 99) ? -999 : csq * 2 - 113;        // raw=99: no signal, range -51 to -113
            atcmd_close();
        }
    }
    return rssi;
}


/**
 *  @brief Get the signal strength reported by the LTEm device at a percent
*/
uint8_t mdmInfo_signalPercent()
{
    // The radio signal strength in the range of -51dBm to -113dBm (-999 is no signal)
    int16_t rssi = mdminfo_rssi(); 

    if (rssi == -999)
        return 0;

    rssi += 113;
    rssi /= 62;
    return (uint8_t)rssi;
}

#pragma endregion



#pragma region private static functions
/* --------------------------------------------------------------------------------------------- */


/**
 *	@brief Action response parser for iccid value request. 
 */
static cmdParseRslt_t S_iccidCompleteParser(ltemDevice_t *modem)
{
    return atcmd__defaultResponseParser("+ICCID: ", true, "", 20, 0, "OK\r\n");
}


#pragma endregion
