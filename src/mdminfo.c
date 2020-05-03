// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "ltem1c.h"

#define IMEI_OFFSET 2
#define IMEI_SIZE 15
#define ICCID_OFFSET 10
#define ICCID_SIZE 20


#pragma region private functions
/* --------------------------------------------------------------------------------------------- */


static bool iccidCompleteParser(const char *response)
{
    return atcmd_gapCompletedHelper(response, "+ICCID: ", 20, ASCII_sCRLF);
}


#pragma endregion


/**
 *  \brief Get the static device provisioning information about the LTEm1.
*/
struct ltem1_modemInfo_tag mdminfo_ltem1()
{
    char resultBuf[41] = {0};

    if (*g_ltem1->modemInfo->imei == NULL)
    {
        atcmd_invoke("AT+GSN\r");                                       // uses singleton cmd struct in g_ltem1
        atcmd_result_t atResult = atcmd_awaitResult(g_ltem1->atcmd);

        if (atResult == ATCMD_RESULT_SUCCESS)
        {
            strncpy(g_ltem1->modemInfo->imei, g_ltem1->atcmd->resultHead + ASCII_szCRLF, IMEI_SIZE);
        }
    }

    if (*g_ltem1->modemInfo->iccid == NULL)                             // uses custom cmd str
    {
        atcmd_t *iccidCmd = atcmd_build("AT+ICCID\r", resultBuf, 41, 500, iccidCompleteParser);
        atcmd_invokeAdv(iccidCmd);
        atcmd_result_t atResult = atcmd_awaitResult(iccidCmd);

        if (atResult == ATCMD_RESULT_SUCCESS)
        {
            strncpy(g_ltem1->modemInfo->iccid, iccidCmd->resultHead + ICCID_OFFSET, ICCID_SIZE);
        }
    }

    if (*g_ltem1->modemInfo->fwver == NULL)
    {
        atcmd_invoke("AT+QGMR\r");
        atcmd_result_t atResult = atcmd_awaitResult(g_ltem1->atcmd);

        if (atResult == ATCMD_RESULT_SUCCESS)
        {
            char *term;
            term = strstr(g_ltem1->atcmd->resultHead + ASCII_szCRLF, ASCII_sCRLF);
            *term = '\0';
            strcpy(g_ltem1->modemInfo->fwver, g_ltem1->atcmd->resultHead + ASCII_szCRLF);
            term = strchr(g_ltem1->modemInfo->fwver, '_');
            *term = ' ';
        }
    }

    if (*g_ltem1->modemInfo->mfgmodel == NULL)
    {
        atcmd_invoke("ATI\r");
        atcmd_result_t atResult = atcmd_awaitResult(g_ltem1->atcmd);

        if (atResult == ATCMD_RESULT_SUCCESS)
        {
            char *term;
            term = strstr(g_ltem1->atcmd->resultHead + ASCII_szCRLF, "\r\nRev");
            *term = '\0';
            strcpy(g_ltem1->modemInfo->mfgmodel, g_ltem1->atcmd->resultHead + ASCII_szCRLF);
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

    atcmd_invoke("AT+CSQ\r");
    atcmd_result_t atResult = atcmd_awaitResult(g_ltem1->atcmd);

    if (atResult == ATCMD_RESULT_SUCCESS)
    {
        char *term;
        term = strstr(g_ltem1->atcmd->resultHead + ASCII_szCRLF, "+CSQ");
        result = strtol(term + 5, &endPtr, 10);
    }

    result = (result == 99) ? result = -999 : -113 + 2 * result;
    return result;
}

