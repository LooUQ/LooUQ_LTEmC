/** ****************************************************************************
  \file 
  \brief Public API controlling modem interface with the cellular network
  \author Greg Terrell, LooUQ Incorporated

  \loouq

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

#define SRCFILE "NWK"                       // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT
//#define ENABLE_DIAGPRINT                    // expand DPRINT into debug output
//#define ENABLE_DIAGPRINT_VERBOSE            // expand DPRINT and DPRINT_V into debug output
#define ENABLE_ASSERT
#include <lqdiag.h>

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "ltemc-network.h"
#include "ltemc-internal.h"

extern ltemDevice_t g_lqLTEM;


#define PROTOCOLS_CMD_BUFFER_SZ 80
#define MIN(x, y) (((x)<(y)) ? (x):(y))
#define STREMPTY(charvar)  (charvar == NULL || charvar[0] == 0 )


// local static functions
static cmdParseRslt_t S__contextStatusCompleteParser(void * atcmd, const char *response);
static char *S__grabToken(char *source, int delimiter, char *tokenBuf, uint8_t tokenBufSz);
static void S__clearProviderInfo();


/* public tcpip functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions

/**
 *	\brief Initialize the IP network contexts structure.
 */
void ntwk_create()
{
    providerInfo_t *providerInfoPtr = (providerInfo_t*)calloc(1, sizeof(providerInfo_t));
    ASSERT(providerInfoPtr != NULL);
    g_lqLTEM.providerInfo = providerInfoPtr;
}


/**
 *	\brief Build default data context configuration for modem to use on startup.
 */
resultCode_t ntwk_setDefaultNetwork(uint8_t pdpContextId, pdpProtocol_t protoType, const char *apn)
{
    return ntwk_configPdpNetwork(pdpContextId, protoType, apn);
}


/**
 *  \brief Configure RAT searching sequence
*/
void ntwk_setProviderScanSeq(const char* scanSequence)
{
    /*AT+QCFG="nwscanseq"[,<scanseq>[,effect]]
    */
    if (strlen(scanSequence) > 0)
    {
        strcpy(g_lqLTEM.modemSettings->scanSequence, scanSequence);
        if (ltem_getDeviceState() == deviceState_appReady)
        {
            atcmd_tryInvoke("AT+QCFG=\"nwscanseq\",%s", scanSequence);
            atcmd_awaitResult();
        }
    }
}


/** 
 *  \brief Configure RAT(s) allowed to be searched.
*/
void ntwk_setProviderScanMode(ntwkScanMode_t scanMode)
{
   /* AT+QCFG="nwscanmode"[,<scanmode>[,<effect>]]
    */
   if (strstr(ltem_getModuleType(), "BG9") != NULL)                         // BG96, BG95 only
   {
        g_lqLTEM.modemSettings->scanMode = scanMode; 
        if (ltem_getDeviceState() == deviceState_appReady)
        {
            atcmd_tryInvoke("AT+QCFG=\"nwscanmode\",%d", scanMode);
            atcmd_awaitResult();
        }
   }
}


/** 
 *  \brief Configure the network category to be searched under LTE RAT.
 */
void ntwk_setIotMode(ntwkIotMode_t iotMode)
{
    /* AT+QCFG="iotopmode",<mode>
    */
    g_lqLTEM.modemSettings->iotMode = iotMode; 
    if (ltem_getDeviceState() == deviceState_appReady)
    {
        atcmd_tryInvoke("AT+QCFG=\"iotopmode\",%d", iotMode);
        atcmd_awaitResult();
    }
}



/**
 *	\brief Initialize BGx Radio Access Technology (RAT) options.
 */
void ntwk_setRatOptions()
{
    ntwk_setProviderScanSeq(g_lqLTEM.modemSettings->scanSequence);
    ntwk_setProviderScanMode(g_lqLTEM.modemSettings->scanMode);
    ntwk_setIotMode(g_lqLTEM.modemSettings->iotMode);
}


resultCode_t ntwk_configPdpNetwork(dataCntxt_t pdpContextId, pdpProtocol_t protoType, const char *apn)
{
    ASSERT(g_lqLTEM.providerInfo != NULL);                                              // ASSERT g_lqLTEM.providerInfo has been initialized
    ASSERT_W(protoType == pdpProtocol_IPV4, "OnlyIPV4SupportedCurrently");              // warn on not IPv4

    snprintf(g_lqLTEM.modemSettings->pdpNtwkConfig, sizeof(g_lqLTEM.modemSettings->pdpNtwkConfig), "AT+QICSGP=%d,%d,\"%s\"\r", pdpContextId, protoType, apn);

    resultCode_t rslt = resultCode__accepted;
    if (g_lqLTEM.deviceState == deviceState_appReady)
    {
        if(atcmd_tryInvoke(g_lqLTEM.modemSettings->pdpNtwkConfig))
        {
            rslt = atcmd_awaitResult();
        }
        atcmd_close();
    }
    return rslt;
}


/**
 *	\brief Configure PDP Context requiring authentication
 *  \details This configuration only supports IP4 data contexts
 */
resultCode_t ntwk_configPdpNetworkWithAuth(uint8_t pdpContextId, const char *apn, const char *userName, const char *pw, pdpCntxtAuthMethods_t authMethod)
{
    resultCode_t rslt;

    snprintf(g_lqLTEM.modemSettings->pdpNtwkConfig, sizeof(g_lqLTEM.modemSettings->pdpNtwkConfig), "AT+QICSGP=%d,1,\"%s\",\"%s\",\"%s\",%d", pdpContextId, apn, userName, pw, authMethod);

    if(atcmd_tryInvoke(g_lqLTEM.modemSettings->pdpNtwkConfig))
    {
        rslt = atcmd_awaitResult();
    }
    atcmd_close();
    return rslt;
}


/**
 *	\brief Apply the default PDP context configuration settings to BGx.
 */
void ntwk_applyPpdNetworkConfig()
{
    resultCode_t rslt;
    if(strlen(g_lqLTEM.modemSettings->pdpNtwkConfig) > 0 && atcmd_tryInvoke(g_lqLTEM.modemSettings->pdpNtwkConfig))
    {
        rslt = atcmd_awaitResult();
        if (rslt != resultCode__success)
            DPRINT(PRNT_CYAN, "DefaultNtwk Config Failed=%d\r", rslt);
    }
    atcmd_close();
}


/**
 *   \brief Wait for a network provider name and network mode. Can be cancelled in threaded env via g_lqLTEM->cancellationRequest.
*/
providerInfo_t *ntwk_awaitProvider(uint16_t waitSec)
{
    ASSERT(g_lqLTEM.providerInfo != NULL);         // ASSERT g_lqLTEM.providerInfo has been initialized

    uint32_t startMillis, endMillis;
    startMillis = endMillis = pMillis();
    uint32_t waitMs = (waitSec > 300) ? 300000 : waitSec * 1000;            // max is 5 minutes

    if (ATCMD_awaitLock(waitMs))                                            // open a reusable lock to complete multiple steps
    {
        S__clearProviderInfo();
        do 
        {
            atcmd_invokeReuseLock("AT+COPS?");                              // get PROVIDER cellular carrier
            if (atcmd_awaitResult() == resultCode__success)
            {
                char *pContinue;
                pContinue = strchr(atcmd_getResponse(), '"');
                if (pContinue != NULL)
                {
                    pContinue = S__grabToken(pContinue + 1, '"', g_lqLTEM.providerInfo->name, ntwk__providerNameSz);

                    uint8_t ntwkMode = (uint8_t)strtol(pContinue + 1, &pContinue, 10);
                    if (ntwkMode == 8)
                        strcpy(g_lqLTEM.providerInfo->iotMode, "M1");
                    else
                        strcpy(g_lqLTEM.providerInfo->iotMode, "NB1");
                }
            }
            if (!STREMPTY(g_lqLTEM.providerInfo->name))
                break;

            pDelay(1000);                                                                   // this yields, allowing alternate execution
            endMillis = pMillis();
        } while (endMillis - startMillis < waitMs || g_lqLTEM.cancellationRequest);         // timed out waiting OR global cancellation


        // got PROVIDER, get networks 

        /* NOTE: BGx will not return response for AT+CGPADDR *OVER THE SERIAL PORT*, unless it is suffixed with the contextID
         * This is one of a handfull of commands that exhibit this behavior; AT+CGPADDR works perfectly over the USB AT port.
        */
        if (!STREMPTY(g_lqLTEM.providerInfo->name))
        {
            char *pContinue;
            uint8_t ntwkIndx = 0;

            atcmd_invokeReuseLock("AT+QIACT?");
            if (atcmd_awaitResultWithOptions(PERIOD_FROM_SECONDS(20), NULL) == resultCode__success)
            {
                pContinue = strstr(atcmd_getResponse(), "+QIACT: ");
                while (pContinue != NULL && ntwkIndx < ntwk__pdpContextCnt)
                {
                    g_lqLTEM.providerInfo->networks[ntwkIndx].pdpContextId = strtol(pContinue + 8, &pContinue, 10);
                    g_lqLTEM.providerInfo->networks[ntwkIndx].isActive = *(++pContinue) == '1';
                    // only supported protocol now is IPv4, alias IP
                    g_lqLTEM.providerInfo->networks[ntwkIndx].pdpProtocol = pdpProtocol_IPV4;
                    pContinue = strstr(pContinue, "+QIACT: ");
                    if (pContinue == NULL)
                        break;
                    ntwkIndx++;
                }
            }
            // get IP addresses
            for (size_t i = 0; i <= ntwkIndx; i++)
            {
                if (g_lqLTEM.providerInfo->networks[i].isActive)
                {
                    atcmd_invokeReuseLock("AT+CGPADDR=%d", g_lqLTEM.providerInfo->networks[i].pdpContextId);
                    if (atcmd_awaitResult() == resultCode__success)
                    {
                        pContinue = strstr(atcmd_getResponse(), "+CGPADDR: ");
                        pContinue = strchr(pContinue + 10, ',') + 1;
                        char *pLineEnd = strchr(pContinue, '\r');
                        strncpy(g_lqLTEM.providerInfo->networks[i].ipAddress, pContinue, MIN(pLineEnd - pContinue, ntwk__ipAddressSz));
                    }
                }
                else
                {
                    strcpy(g_lqLTEM.providerInfo->networks[i].ipAddress, "0.0.0.0");
                }
            }
            g_lqLTEM.providerInfo->networkCnt = ++ntwkIndx;
        }
    }
    atcmd_close();
    return g_lqLTEM.providerInfo;
}


/**
 *	\brief Deactivate PDP Context/APN.
 */
void ntwk_activateNetwork(uint8_t cntxtId)
{
    if (atcmd_tryInvoke("AT+CGACT=1,%d", cntxtId))
    {
        resultCode_t rslt = atcmd_awaitResultWithOptions(atcmd__defaultTimeout, S__contextStatusCompleteParser);
        if ( rslt == resultCode__success)
            ntwk_awaitProvider(5);
    }
}


/**
 *	\brief Deactivate PDP Context/APN.
 */
void ntwk_deactivateNetwork(uint8_t cntxtId)
{
    if (atcmd_tryInvoke("AT+CGACT=0,%d", cntxtId))
    {
        resultCode_t rslt = atcmd_awaitResultWithOptions(atcmd__defaultTimeout, S__contextStatusCompleteParser);
        if ( rslt == resultCode__success)
            ntwk_awaitProvider(5);
    }
}


/**
 *   \brief Get current provider information. If not connected to a provider will be an empty providerInfo struct
 *   \return Struct containing the network operator name (operName) and network mode (ntwkMode).
*/
providerInfo_t *ntwk_getProviderInfo()
{
    if (strlen(g_lqLTEM.providerInfo->name) > 0)
        return &g_lqLTEM.providerInfo;
    return NULL;
}


/**
 *	\brief Get count of APN active data contexts from BGx.
 */
uint8_t ntwk_getActiveNetworkCount()
{
    return g_lqLTEM.providerInfo->networkCnt;
}


/**
 *	\brief Get network (PDP) information
 */
networkInfo_t *ntwk_getNetworkInfo(uint8_t pdpContextId)
{
    for (size_t i = 0; i < g_lqLTEM.providerInfo->networkCnt; i++)
    {
        if (g_lqLTEM.providerInfo->networks[i].pdpContextId == pdpContextId)
        {
            return &g_lqLTEM.providerInfo->networks[i];
        }
    }
    return NULL;
}


/**
 *	@brief Get current network registration status.
 *  @return The current network operator registration status.
 */
uint8_t ntwk_getRegistrationStatus()
{
    if (atcmd_tryInvoke("AT+CREG?"))
    {
        // TODO need a parser wrapper to grab <stat> (pos 2)
        resultCode_t rslt = atcmd_awaitResult();
        return 0;
    }
    else
    {
        return 255;
    }
}


// /**
//  *	@brief Set network operator.
//  *  @details The characteristics of the selected operator are accessible using the atcmd_getResponse() function.

//  *  @param [in] mode Action to be performed, set/clear/set default.
//  *  @param [in] format The form for the ntwkOperator parameter value: long, short, numeric.
//  *  @param [in] ntwkOperator Operator to select, presented in the "format". Not all modes require/act on this parameter.
//  *  @return Current operator selection mode. Note:
//  */
// uint8_t ntwk_setOperator(uint8_t mode, uint8_t format, const char* ntwkOperator)
// {
// }


/** 
 *  \brief Development/diagnostic function to retrieve visible providers from cell radio.
 */
void ntwkDiagnostics_getProviders(char *providersList, uint16_t listSz)
{
    /* AT+COPS=? */
    ASSERT_W(false, "ntwkDiagnostics_getProviders() blocks and is SLOW!");

    if (ATCMD_awaitLock(atcmd__defaultTimeout))
    {
        if (g_lqLTEM.modemInfo->imei[0] == 0)
        {
            atcmd_invokeReuseLock("AT+COPS=?");
            if (atcmd_awaitResultWithOptions(PERIOD_FROM_SECONDS(180), NULL) == resultCode__success)
            {
                strncpy(providersList, atcmd_getResponse() + 9, listSz - 1);
            }
        }
    }
    atcmd_close();
}


#pragma endregion


/* private functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions


static void S__clearProviderInfo()
{
    memset((void*)g_lqLTEM.providerInfo->networks, 0, g_lqLTEM.providerInfo->networkCnt * sizeof(networkInfo_t));
    memset((void*)g_lqLTEM.providerInfo, 0, sizeof(providerInfo_t));
}


/**
 *   \brief Tests for the completion of a network APN context activate action.
 *   \return standard action result integer (http result).
*/
static cmdParseRslt_t S__contextStatusCompleteParser(void *atcmd, const char *response)
{
    return atcmd_stdResponseParser("+QIACT: ", false, NULL, 2, 2, NULL, 0);
}


/**
 *  \brief Scans a C-String (char array) for the next delimeted token and null terminates it.
 * 
 *  \param [in] source - Original char array to scan.
 *  \param [in] delimeter - Character separating tokens (passed as integer value).
 *  \param [out] tokenBuf - Pointer to where token should be copied to.
 *  \param [in] tokenBufSz - Size of buffer to receive token.
 * 
 *  \return Pointer to the location in the source string immediately following the token.
*/
static char *S__grabToken(char *source, int delimiter, char *tokenBuf, uint8_t tokenBufSz) 
{
    char *delimAt;
    if (source == NULL)
        return false;

    delimAt = strchr(source, delimiter);
    uint8_t tokenSz = delimAt - source;
    if (tokenSz == 0)
        return NULL;

    memset(tokenBuf, 0, tokenBufSz);
    memcpy(tokenBuf, source, MIN(tokenSz, tokenBufSz-1));
    return delimAt + 1;
}

#pragma endregion
