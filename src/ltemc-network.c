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


#define _DEBUG 2                        // set to non-zero value for PRINTF debugging output, 
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

#define SRCFILE "NWK"                           // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT
#include "ltemc-internal.h"
#include "ltemc-network.h"

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
 *	\brief Configure PDP Context
 */
void ntwk_setDefaulNetworkConfig(uint8_t pdpContextId, const char *protoType, const char *apn)
{
    ASSERT(g_lqLTEM.providerInfo != NULL);                                             // ASSERT g_lqLTEM.providerInfo has been initialized
    ASSERT_W(strcmp(protoType, PDP_PROTOCOL_IPV4) == 0, "OnlyIPV4SupportedCurrently"); // warn on not IPv4 and IPv4 override

    snprintf(g_lqLTEM.modemSettings->defaultNtwkConfig, sizeof(g_lqLTEM.modemSettings->defaultNtwkConfig), "AT+CGDCONT=%d,\"%s\",\"%s\"\r", pdpContextId, PDP_PROTOCOL_IPV4, apn);
}



/**
 *	\brief Initialize BGx Radio Access Technology (RAT) options.
 */
void NTWK_initRatOptions()
{
    ltem_setProviderScanSeq(g_lqLTEM.modemSettings->scanSequence);
    ltem_setProviderScanMode(g_lqLTEM.modemSettings->scanMode);
    ltem_setIotMode(g_lqLTEM.modemSettings->iotMode);
}


/**
 *	\brief Apply the default PDP context config to BGx.
 */
void NTWK_applyDefaulNetwork()
{
    resultCode_t atResult;
    if(atcmd_tryInvoke(g_lqLTEM.modemSettings->defaultNtwkConfig))
    {
        atResult = atcmd_awaitResult();
        if (atResult != resultCode__success)
            PRINTF(dbgColor__cyan, "DefaultNtwk Config Failed=%d\r", atResult);
    }
    atcmd_close();
}


void ntwk_setNetworkConfig(uint8_t pdpContextId, const char *protoType, const char *apn)
{
    ASSERT(g_lqLTEM.providerInfo != NULL);                                             // ASSERT g_lqLTEM.providerInfo has been initialized
    ASSERT_W(strcmp(protoType, PDP_PROTOCOL_IPV4) == 0, "OnlyIPV4SupportedCurrently"); // warn on not IPv4 and IPv4 override
    // protoType = pdpProtocolType_IPV4;

    snprintf(g_lqLTEM.modemSettings->defaultNtwkConfig, sizeof(g_lqLTEM.modemSettings->defaultNtwkConfig), "AT+CGDCONT=%d,%d,\"%s\"\r", pdpContextId, protoType, apn);

    resultCode_t atResult;
    if(atcmd_tryInvoke("AT+CGDCONT=%d,%d,\"%s\"\r", pdpContextId, protoType, apn))
    {
        atResult = atcmd_awaitResult();
    }
    atcmd_close();
    return atResult;
}


/* Deferred Implementation: Cannot find a network provider requiring authentication and Quectel doesn't support beyond IPV4
*/
// /**
//  *	\brief Configure PDP Context requiring authentication
//  */
// networkInfo_t * ntwk_configureNetworkWithAuth(uint8_t pdpContextId, pdpProtocolType_t protoType, const char *apn, const char *userName, const char *pw, pdpCntxtAuthMethods_t authMethod)
// {
//     resultCode_t atResult;
//     if(atcmd_tryInvoke("AT+QICSGP=%d,1,\"%s\",\"%s\",\"%s\",%d", pdpContextId, apn, userName, pw, authMethod))
//     {
//         atResult = atcmd_awaitResult();
//     }
//     atcmd_close();
//     return atResult;
// }


/**
 *   \brief Wait for a network provider name and network mode. Can be cancelled in threaded env via g_lqLTEM->cancellationRequest.
*/
providerInfo_t *ntwk_awaitProvider(uint16_t waitSec)
{
    ASSERT(g_lqLTEM.providerInfo != NULL);         // ASSERT g_lqLTEM.providerInfo has been initialized

    uint32_t startMillis, endMillis;
    startMillis = endMillis = pMillis();
    uint32_t waitDuration = (waitSec > 300) ? 300000 : waitSec * 1000;      // max is 5 minutes

    if (ATCMD_awaitLock(waitSec))                                           // open a reusable lock to complete multiple steps
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
        } while (endMillis - startMillis < waitDuration || g_lqLTEM.cancellationRequest);   // timed out waiting OR global cancellation


        // got PROVIDER, get networks 

        /* NOTE: BGx will not return response for AT+CGPADDR *OVER THE SERIAL PORT*, unless it is suffixed with the contextID
         * This is one of a handfull of commands that exhibit this behavior; AT+CGPADDR works perfectly over the USB AT port.
        */
        if (!STREMPTY(g_lqLTEM.providerInfo->name))
        {
            char *pContinue;
            uint8_t ntwkIndx = 0;

            atcmd_invokeReuseLock("AT+CGACT?");
            if (atcmd_awaitResultWithOptions(PERIOD_FROM_SECONDS(20), NULL) == resultCode__success)
            {
                pContinue = strstr(atcmd_getResponse(), "+CGACT: ");
                while (pContinue != NULL && ntwkIndx < ntwk__pdpContextCnt)
                {
                    g_lqLTEM.providerInfo->networks[ntwkIndx].pdpContextId = strtol(pContinue + 8, &pContinue, 10);
                    g_lqLTEM.providerInfo->networks[ntwkIndx].isActive = *(++pContinue) == '1';
                    // only supported protocol now is IPv4, alias IP
                    strcpy(g_lqLTEM.providerInfo->networks[ntwkIndx].pdpProtocolType, PDP_PROTOCOL_IPV4);
                    pContinue = strstr(pContinue, "+CGACT: ");
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
        resultCode_t atResult = atcmd_awaitResultWithOptions(atcmd__defaultTimeout, S__contextStatusCompleteParser);
        if ( atResult == resultCode__success)
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
        resultCode_t atResult = atcmd_awaitResultWithOptions(atcmd__defaultTimeout, S__contextStatusCompleteParser);
        if ( atResult == resultCode__success)
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
 *  \brief Development/diagnostic function to retrieve visible providers from cell radio.
 */
void ntwk_getProviders(char *providersList, uint16_t listSz)
{
    /* AT+COPS=? */
    ASSERT_W(false, "ntwk_getProviders() blocks and is SLOW!");

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
 *   \brief Get the network operator name and network mode.
 *   \return Struct containing the network operator name (operName) and network mode (ntwkMode).
*/
static providerInfo_t *S__getNetworkProvider()
{
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
