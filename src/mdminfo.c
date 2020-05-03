// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "ltem1c.h"

#define IMEI_OFFSET 2
#define IMEI_SIZE 15
#define ICCID_OFFSET 10
#define ICCID_SIZE 20


#pragma region private functions
/* --------------------------------------------------------------------------------------------- */


static action_result_t iccidCompleteParser(const char *response)
{
    return action_gapResultParser(response, "+ICCID: ", true, 20, ASCII_sCRLF);
}


#pragma endregion


/**
 *  \brief Get the static device provisioning information about the LTEm1.
*/
struct ltem1_modemInfo_tag mdminfo_ltem1()
{
    if (*g_ltem1->modemInfo->imei == NULL)
    {
        action_invoke("AT+GSN\r");                                       // uses singleton cmd struct in g_ltem1
        action_result_t atResult = action_awaitResult(NULL);

        if (atResult == ACTION_RESULT_SUCCESS)
        {
            strncpy(g_ltem1->modemInfo->imei, g_ltem1->dAction->resultHead + ASCII_szCRLF, IMEI_SIZE);
        }
    }

    if (*g_ltem1->modemInfo->iccid == NULL)                             // uses custom cmd str
    {
        //action_t *iccidCmd = action_build("AT+ICCID\r", 41, 500, iccidCompleteParser);
        action_invokeWithParser("AT+ICCID\r", iccidCompleteParser);
        action_result_t atResult = action_awaitResult(NULL);

        if (atResult == ACTION_RESULT_SUCCESS)
        {
            strncpy(g_ltem1->modemInfo->iccid, g_ltem1->dAction->resultHead + ICCID_OFFSET, ICCID_SIZE);
        }
    }

    if (*g_ltem1->modemInfo->fwver == NULL)
    {
        action_invoke("AT+QGMR\r");
        action_result_t atResult = action_awaitResult(NULL);

        if (atResult == ACTION_RESULT_SUCCESS)
        {
            char *term;
            term = strstr(g_ltem1->dAction->resultHead + ASCII_szCRLF, ASCII_sCRLF);
            *term = '\0';
            strcpy(g_ltem1->modemInfo->fwver, g_ltem1->dAction->resultHead + ASCII_szCRLF);
            term = strchr(g_ltem1->modemInfo->fwver, '_');
            *term = ' ';
        }
    }

    if (*g_ltem1->modemInfo->mfgmodel == NULL)
    {
        action_invoke("ATI\r");
        action_result_t atResult = action_awaitResult(NULL);

        if (atResult == ACTION_RESULT_SUCCESS)
        {
            char *term;
            term = strstr(g_ltem1->dAction->resultHead + ASCII_szCRLF, "\r\nRev");
            *term = '\0';
            strcpy(g_ltem1->modemInfo->mfgmodel, g_ltem1->dAction->resultHead + ASCII_szCRLF);
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
    char *endPtr;
    int16_t result;

    action_invoke("AT+CSQ\r");
    action_result_t atResult = action_awaitResult(NULL);

    if (atResult == ACTION_RESULT_SUCCESS)
    {
        char *term;
        term = strstr(g_ltem1->dAction->resultHead + ASCII_szCRLF, "+CSQ");
        result = strtol(term + 5, &endPtr, 10);
    }

    result = (result == 99) ? result = -999 : -113 + 2 * result;
    return result;
}

