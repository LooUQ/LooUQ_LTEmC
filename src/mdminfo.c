// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

//#define _DEBUG
#include "ltem1c.h"


#define IMEI_OFFSET 2
#define IMEI_SIZE 15
#define ICCID_OFFSET 10
#define ICCID_SIZE 20


// private local declarations
static resultCode_t iccidCompleteParser(const char *response, char **endptr);


/* Public functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions


/**
 *  \brief Get the LTEm1 static device identification\provisioning information.
 * 
 *  \return Modem information structure.
*/
modemInfo_t mdminfo_ltem1()
{
    if (*g_ltem1->modemInfo->imei == NULL)
    {
        if (action_tryInvoke("AT+GSN"))
        {
            actionResult_t atResult = action_awaitResult(false);
            if (atResult.statusCode == RESULT_CODE_SUCCESS)
            {
                strncpy(g_ltem1->modemInfo->imei, atResult.response + ASCII_szCRLF, IMEI_SIZE);
            }
            action_close();
        }
    }

    if (*g_ltem1->modemInfo->iccid == NULL)                             // uses custom cmd str
    {
        //action_t *iccidCmd = action_build("AT+ICCID\r", 41, 500, iccidCompleteParser);
        if (action_tryInvokeAdv("AT+ICCID", ACTION_RETRIES_DEFAULT, ACTION_TIMEOUT_DEFAULTmillis, iccidCompleteParser))
        {
            actionResult_t atResult = action_awaitResult(false);
            if (atResult.statusCode == RESULT_CODE_SUCCESS)
            {
                strncpy(g_ltem1->modemInfo->iccid, atResult.response + ICCID_OFFSET, ICCID_SIZE);
            }
            action_close();
        }
    }

    if (*g_ltem1->modemInfo->fwver == NULL)
    {
        if (action_tryInvoke("AT+QGMR"))
        {
            actionResult_t atResult = action_awaitResult(false);
            if (atResult.statusCode == RESULT_CODE_SUCCESS)
            {
                char *term;
                term = strstr(atResult.response + ASCII_szCRLF, ASCII_sCRLF);
                *term = '\0';
                strcpy(g_ltem1->modemInfo->fwver, atResult.response + ASCII_szCRLF);
                term = strchr(g_ltem1->modemInfo->fwver, '_');
                *term = ' ';
            }
            action_close();
        }
    }

    if (*g_ltem1->modemInfo->mfgmodel == NULL)
    {
        if (action_tryInvoke("ATI"))
        {
            actionResult_t atResult = action_awaitResult(false);
            if (atResult.statusCode == RESULT_CODE_SUCCESS)
            {
                char *term;
                term = strstr(atResult.response + ASCII_szCRLF, "\r\nRev");
                *term = '\0';
                strcpy(g_ltem1->modemInfo->mfgmodel, atResult.response + ASCII_szCRLF);
                term = strchr(g_ltem1->modemInfo->mfgmodel, '\r');
                *term = ':';
                term = strchr(g_ltem1->modemInfo->mfgmodel, '\n');
                *term = ' ';
            }
            action_close();
        }
    }
    return *g_ltem1->modemInfo;
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

    if (action_tryInvoke("AT+CSQ"))
    {
        actionResult_t atResult = action_awaitResult(false);
        if (atResult.statusCode == RESULT_CODE_SUCCESS)
        {
            char *term;
            term = strstr(atResult.response + ASCII_szCRLF, "+CSQ");
            csq = strtol(term + 5, NULL, 10);
        }
        rssi = (csq == 99) ? 0 : -113 + 2 * csq;        // raw=99: no signal, range -51 to -113
        action_close();
    }
    return rssi;
}

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


static resultCode_t iccidCompleteParser(const char *response, char **endptr)
{
    return action_defaultResultParser(response, "+ICCID: ", true, 20, ASCII_sOK, endptr);
}


#pragma endregion
