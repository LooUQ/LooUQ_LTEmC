// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "ltem1c.h"

//#define _DEBUG
#include "dbgprint.h"

#define IMEI_OFFSET 2
#define IMEI_SIZE 15
#define ICCID_OFFSET 10
#define ICCID_SIZE 20


// private local declarations
static actionResult_t iccidCompleteParser(const char *response);


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
    char response[ACTION_DEFAULT_RESPONSE_SZ] = {0};

    if (*g_ltem1->modemInfo->imei == NULL)
    {
        action_tryInvoke("AT+GSN", true);
        actionResult_t atResult = action_awaitResult(response, ACTION_DEFAULT_RESPONSE_SZ, 0, NULL);

        if (atResult == ACTION_RESULT_SUCCESS)
        {
            strncpy(g_ltem1->modemInfo->imei, response + ASCII_szCRLF, IMEI_SIZE);
        }
    }

    if (*g_ltem1->modemInfo->iccid == NULL)                             // uses custom cmd str
    {
        //action_t *iccidCmd = action_build("AT+ICCID\r", 41, 500, iccidCompleteParser);
        action_tryInvoke("AT+ICCID", true);
        actionResult_t atResult = action_awaitResult(response, ACTION_DEFAULT_RESPONSE_SZ, 0, iccidCompleteParser);

        if (atResult == ACTION_RESULT_SUCCESS)
        {
            strncpy(g_ltem1->modemInfo->iccid, response + ICCID_OFFSET, ICCID_SIZE);
        }
    }

    if (*g_ltem1->modemInfo->fwver == NULL)
    {
        action_tryInvoke("AT+QGMR", true);
        actionResult_t atResult = action_awaitResult(response, ACTION_DEFAULT_RESPONSE_SZ, 0, NULL);

        if (atResult == ACTION_RESULT_SUCCESS)
        {
            char *term;
            term = strstr(response + ASCII_szCRLF, ASCII_sCRLF);
            *term = '\0';
            strcpy(g_ltem1->modemInfo->fwver, response + ASCII_szCRLF);
            term = strchr(g_ltem1->modemInfo->fwver, '_');
            *term = ' ';
        }
    }

    if (*g_ltem1->modemInfo->mfgmodel == NULL)
    {
        action_tryInvoke("ATI", true);
        actionResult_t atResult = action_awaitResult(response, ACTION_DEFAULT_RESPONSE_SZ, 0, NULL);

        if (atResult == ACTION_RESULT_SUCCESS)
        {
            char *term;
            term = strstr(response + ASCII_szCRLF, "\r\nRev");
            *term = '\0';
            strcpy(g_ltem1->modemInfo->mfgmodel, response + ASCII_szCRLF);
            term = strchr(g_ltem1->modemInfo->mfgmodel, '\r');
            *term = ':';
            term = strchr(g_ltem1->modemInfo->mfgmodel, '\n');
            *term = ' ';
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
    char response[ACTION_DEFAULT_RESPONSE_SZ] = {0};
    int16_t result;

    action_tryInvoke("AT+CSQ", true);
    actionResult_t atResult = action_awaitResult(response, ACTION_DEFAULT_RESPONSE_SZ, 0, NULL);

    if (atResult == ACTION_RESULT_SUCCESS)
    {
        char *term;
        term = strstr(response + ASCII_szCRLF, "+CSQ");
        result = strtol(term + 5, NULL, 10);
    }
    result = (result == 99) ? result = -999 : -113 + 2 * result;
    return result;
}


#pragma endregion

/* private (static) functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions


static actionResult_t iccidCompleteParser(const char *response)
{
    return action_gapResultParser(response, "+ICCID: ", true, 20, ASCII_sOK);
}


#pragma endregion
