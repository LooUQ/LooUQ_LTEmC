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
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#endif


#include "ltemc.h"


#define IMEI_OFFSET 2
#define IMEI_SIZE 15
#define ICCID_OFFSET 10
#define ICCID_SIZE 20


// private local declarations
static resultCode_t s_iccidCompleteParser(const char *response, char **endptr);


/* Public functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions


/**
 *  \brief Get the LTEm1 static device identification/provisioning information.
 * 
 *  \return Modem information struct, see mdminfo.h for details.
*/
modemInfo_t mdminfo_ltem()
{
    if (*g_ltem->modemInfo->imei == NULL)
    {
        if (atcmd_tryInvoke("AT+GSN"))
        {
            atcmdResult_t atResult = atcmd_awaitResult(false);
            if (atResult.statusCode == RESULT_CODE_SUCCESS)
            {
                strncpy(g_ltem->modemInfo->imei, atResult.response + ASCII_szCRLF, IMEI_SIZE);
            }
            atcmd_close();
        }
    }

    if (*g_ltem->modemInfo->iccid == NULL)                             // uses custom cmd str
    {
        //atcmd_t *iccidCmd = atcmd_build("AT+ICCID\r", 41, 500, iccidCompleteParser);
        if (atcmd_tryInvokeAdv("AT+ICCID", ACTION_TIMEOUTml, s_iccidCompleteParser))
        {
            atcmdResult_t atResult = atcmd_awaitResult(false);
            if (atResult.statusCode == RESULT_CODE_SUCCESS)
            {
                strncpy(g_ltem->modemInfo->iccid, atResult.response + ICCID_OFFSET, ICCID_SIZE);
            }
            atcmd_close();
        }
    }

    if (*g_ltem->modemInfo->fwver == NULL)
    {
        if (atcmd_tryInvoke("AT+QGMR"))
        {
            atcmdResult_t atResult = atcmd_awaitResult(false);
            if (atResult.statusCode == RESULT_CODE_SUCCESS)
            {
                char *term;
                term = strstr(atResult.response + ASCII_szCRLF, ASCII_sCRLF);
                *term = '\0';
                strcpy(g_ltem->modemInfo->fwver, atResult.response + ASCII_szCRLF);
                term = strchr(g_ltem->modemInfo->fwver, '_');
                *term = ' ';
            }
            atcmd_close();
        }
    }

    if (*g_ltem->modemInfo->mfgmodel == NULL)
    {
        if (atcmd_tryInvoke("ATI"))
        {
            atcmdResult_t atResult = atcmd_awaitResult(false);
            if (atResult.statusCode == RESULT_CODE_SUCCESS)
            {
                char *term;
                term = strstr(atResult.response + ASCII_szCRLF, "\r\nRev");
                *term = '\0';
                strcpy(g_ltem->modemInfo->mfgmodel, atResult.response + ASCII_szCRLF);
                term = strchr(g_ltem->modemInfo->mfgmodel, '\r');
                *term = ':';
                term = strchr(g_ltem->modemInfo->mfgmodel, '\n');
                *term = ' ';
            }
            atcmd_close();
        }
    }
    return *g_ltem->modemInfo;
}


/**
 *  \brief Get the static device provisioning information about the LTEm1.
 * 
 *  \return The radio signal strength in the range of -51dBm to -113dBm (-999 is no signal)
*/
int16_t mdminfo_rssi()
{
    uint8_t csq = 0;
    int8_t rssi;

    if (atcmd_tryInvoke("AT+CSQ"))
    {
        atcmdResult_t atResult = atcmd_awaitResult(false);
        if (atResult.statusCode == RESULT_CODE_SUCCESS)
        {
            char *term;
            term = strstr(atResult.response + ASCII_szCRLF, "+CSQ");
            csq = strtol(term + 5, NULL, 10);
        }
        rssi = (csq == 99) ? -128 : csq * 2 -113;        // raw=99: no signal, range -51 to -113
        atcmd_close();
    }
    return rssi;
}


/**
 *  \brief Get the RSSI value as a calculate bar count for visualizations. Like on a smartphone.
 * 
 *  \param numberOfBars [in] The total number of bars on your display. 
 * 
 *  \return The number of bars to indicate based on the current RSSI and your scale.
*/
uint8_t mdminfo_rssiBars(uint8_t numberOfBars)
{
    uint8_t barSpan = (113 - 51) / numberOfBars;
    int rssi = mdminfo_rssi();
    return (int)((rssi + 113 + barSpan) / barSpan);
}


#pragma endregion

/* private (static) functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions

/**
 *	\brief Action response parser for iccid value request. 
 */
static resultCode_t s_iccidCompleteParser(const char *response, char **endptr)
{
    return atcmd_defaultResultParser(response, "+ICCID: ", true, 20, ASCII_sOK, endptr);
}


#pragma endregion
