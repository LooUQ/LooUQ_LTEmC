/******************************************************************************
 *  \file ltemc-network.c
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
 * PDP network support
 *****************************************************************************/

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


#include "ltemc-internal.h"
#include <lq-network.h>

#define PROTOCOLS_CMD_BUFFER_SZ 80
#define MIN(x, y) (((x)<(y)) ? (x):(y))


// Modem global (singleton)
extern ltemDevice_t g_ltem;


// local static functions
static cmdParseRslt_t S_contextStatusCompleteParser(void * atcmd, const char *response);
static providerInfo_t *S_getNetworkProvider();
static char *S_grabToken(char *source, int delimiter, char *tokenBuf, uint8_t tokenBufSz);
static networkInfo_t *S_activateNetwork(uint8_t cntxtId, networkPDPType_t protoType, const char *pApn, const char *pUserName, const char *pPW, pdpCntxtAuthMethods_t authMethod);


/* public tcpip functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions

/**
 *	\brief Initialize the IP network contexts structure.
 */
void ntwk_create()
{
    providerInfo_t *providerInfoPtr = (providerInfo_t*)calloc(1, sizeof(providerInfo_t));
    ASSERT(providerInfoPtr != NULL, srcfile_ltemc_network_c);
    g_ltem.providerInfo = providerInfoPtr;
}


/**
 *  \brief Configure RAT searching sequence
*/
void ntwk_setProviderScanSeq(const char* scanSequence)
{
    //AT+QCFG="nwscanseq"[,<scanseq>[,effect]]
    /*
    */
    atcmd_tryInvoke("AT+QCFG=\"nwscanseq\",%s", scanSequence);
    atcmd_awaitResult();
}


/** 
 *  \brief Configure RAT(s) allowed to be searched.
*/
void ntwk_setProviderScanMode(ntwkScanMode_t scanMode)
{
    // AT+QCFG="nwscanmode"[,<scanmode>[,<effect>]]
    atcmd_tryInvoke("AT+QCFG=\"nwscanmode\",%d", scanMode);
    atcmd_awaitResult();
}


/** 
 *  \brief Configure the network category to be searched under LTE RAT.
 */
void ntwk_setIotMode(ntwk_iotMode_t iotMode)
{
    atcmd_tryInvoke("AT+QCFG=\"iotopmode\",%d", iotMode);
    atcmd_awaitResult();
}


/**
 *   \brief Wait for a network provider name and network mode. Can be cancelled in threaded env via g_ltem->cancellationRequest.
*/
providerInfo_t *ntwk_awaitProvider(uint16_t waitDurSeconds)
{
    // networkInfo_t ntwk;
    providerInfo_t *provider;
    uint32_t startMillis, endMillis;
    startMillis = endMillis = pMillis();
    uint32_t waitDuration = (waitDurSeconds > 300) ? 300000 : waitDurSeconds * 1000;    // max is 5 minutes

    do 
    {
        provider = S_getNetworkProvider();                  // perform a single provider query (COPS?): process if provider found, else return NULL
        provider->defaultContext = 1;                       // most networks use context ID = 1, application can override with ntwk_setProviderDefaultContext()
        if (!STREMPTY(provider->name))
            return provider;

        pDelay(1000);                                       // this yields, allowing alternate execution
        endMillis = pMillis();
    } while (endMillis - startMillis < waitDuration || g_ltem.cancellationRequest);     // timed out waiting || global cancellation
    return provider;
}



/**
 *   \brief Set the default/data context number for provider. Default is 1 if not overridden here.
*/
void ntwk_setProviderDefaultContext(uint8_t defaultContext)
{
    ASSERT(defaultContext < 16, srcfile_ltemc_network_c);
    ((providerInfo_t*)g_ltem.providerInfo)->defaultContext = defaultContext;
}



// networkInfo_t *lqcNtwk_activateNetwork(uint8_t contextId, networkPDPType_t networkPDPType, const char *apn)
// {
//     ASSERT(ltem_readDeviceState() == qbgDeviceState_appReady, srcfile_cm_lqcloudNtwkLtemc_c);
//     providerInfo_t *provider = ntwk_awaitProvider(PERIOD_FROM_SECONDS(2));                       // this is a query to verify connection to provider
//     ASSERT(strlen(provider->name) > 0, srcfile_cm_lqcloudNtwkLtemc_c);
//     PRINTF(dbgColor__white, "Network type is %s on %s\r", provider.iotMode, provider.name);
//     networkInfo_t *ntwk;
//     uint8_t pdpCount = ntwk_readActiveNetworkCount();                                           // populates network APN table in LTEmC, > 0 network exists
//     if (pdpCount == 0)
//     {
//         /*  Activating PDP context is network carrier dependent, it is not required on most carrier networks. 
//             *  If not required... ntwk_activateContext() stills "warms up" the connection.
//             *
//             *  If apnCount > 0, assume "data" context is available. Can test with ntwk_getContext(context_ID) != NULL.
//             */
//         ntwk_activateNetwork(contextId, networkPDPType, apn);
//         ntwk = ntwk_getNetworkInfo(contextId);
//         if (ntwk->isActive)
//             PRINTF(dbgColor__white, "Network(PDP) Context=1, IPaddr=%s\r", pdpCntxt->ipAddress);
//     }
//     return ntwk;
// }

#define STREMPTY(charvar)  (charvar == NULL || charvar[0] == 0 )

static networkInfo_t *S_activateNetwork(uint8_t cntxtId, networkPDPType_t protoType, const char *apn, const char *userName, const char *pw, pdpCntxtAuthMethods_t authMethod)
{
    networkInfo_t *ntwk = ntwk_getNetworkInfo(cntxtId);
    if (ntwk != NULL)
        return ntwk;                                                                            // network context is already active

    ASSERT(ltem_readDeviceState() == qbgDeviceState_appReady, srcfile_ltemc_network_c);         // ASSET modem is ready
    providerInfo_t *provider = ntwk_awaitProvider(PERIOD_FROM_SECONDS(2));                      // ASSERT provider active
    ASSERT(strlen(provider->name) > 0, srcfile_ltemc_network_c);
    PRINTF(dbgColor__white, "Provider is %s, mode=%s\r", provider->name, provider->iotMode);

    /*  Activating PDP context is network carrier dependent, it is not required on most carrier networks. 
     *  If not required... ntwk_activateContext() stills "warms up" the connection.
     *
     *  If apnCount > 0, assume "data" context is available. Can test with ntwk_getContext(context_ID) != NULL.
     */

    bool ntwkActivated = false;
    if (ATCMD_awaitLock(atcmd__defaultTimeoutMS))
    {
        atcmd_setOptions(atcmd__defaultTimeoutMS, atcmd__useDefaultOKCompletionParser);

        if (STREMPTY(userName) || STREMPTY(pw))
            atcmd_invokeReuseLock("AT+QICSGP=%d,%d,\"%s\"\r", cntxtId, protoType, apn);
        else
            atcmd_invokeReuseLock("AT+QICSGP=%d,%d,\"%s\"\r", cntxtId, protoType, apn, userName, pw, authMethod);

        resultCode_t atResult = atcmd_awaitResult();
        if (atResult == resultCode__success)
        {
            atcmd_setOptions(atcmd__defaultTimeoutMS, S_contextStatusCompleteParser); 
            atcmd_invokeReuseLock("AT+QIACT=%d\r", cntxtId);
            atResult = atcmd_awaitResult();
            if (atResult == resultCode__success)
                ntwkActivated = true;
        }
    }
    atcmd_close();

    if (ntwkActivated)
    {
        ntwk = ntwk_getNetworkInfo(cntxtId);
        if (ntwk->isActive)
        {
            PRINTF(dbgColor__white, "Network(PDP) Context=%d, IPaddr=%s\r", cntxtId, ntwk->ipAddress);
            return ntwk;
        }
    }
    // if fell through, PDP not available... clear network provider
    memset((providerInfo_t*)g_ltem.providerInfo, 0, sizeof(providerInfo_t));
    return NULL;
}


/**
 *	\brief Activate PDP Context/APN.
 */
networkInfo_t * ntwk_activateNetwork(uint8_t cntxtId, networkPDPType_t protoType, const char *apn)
{
    return S_activateNetwork(cntxtId, protoType, apn, NULL, NULL, 0);

    // if (ntwk_getNetworkInfo(cntxtId) != NULL)
    //     return true;
    // bool pdpActivated = false;
    // if (ATCMD_awaitLock(atcmd__defaultTimeoutMS))
    // {
    //     atcmd_setOptions(atcmd__defaultTimeoutMS, atcmd__useDefaultOKCompletionParser);
    //     atcmd_invokeReuseLock("AT+QICSGP=%d,%d,\"%s\"\r", cntxtId, protoType, apn);
    //     resultCode_t atResult = atcmd_awaitResult();
    //     if (atResult == resultCode__success)
    //     {
    //         atcmd_setOptions(atcmd__defaultTimeoutMS, S_contextStatusCompleteParser); 
    //         atcmd_invokeReuseLock("AT+QIACT=%d\r", cntxtId);
    //         atResult = atcmd_awaitResult();
    //         if (atResult == resultCode__success)
    //             pdpActivated = true;
    //     }
    // }
    // atcmd_close();
    // if (pdpActivated)
    //     ntwk_readActiveNetworkCount();
    // if (ntwk_getNetworkInfo(cntxtId) != NULL)
    //     return true;
    // // if fell through, PDP not available... clear network provider
    // memset((providerInfo_t*)g_ltem.providerInfo, 0, sizeof(providerInfo_t));
    // return false;
}


/**
 *	\brief Activate PDP Context/APN requiring authentication.
 */
networkInfo_t * ntwk_activateNetworkWithAuth(uint8_t cntxtId, networkPDPType_t protoType, const char *apn, const char *userName, const char *pw, pdpCntxtAuthMethods_t authMethod)
{
    return S_activateNetwork(cntxtId, protoType, apn, userName, pw, authMethod);

    // if (ntwk_getNetworkInfo(cntxtId) != NULL)
    //     return true;
    // bool pdpActivated = false;
    // if (ATCMD_awaitLock(atcmd__defaultTimeoutMS))
    // {
    //     atcmd_setOptions(atcmd__defaultTimeoutMS, atcmd__useDefaultOKCompletionParser);
    //     atcmd_invokeReuseLock("AT+QICSGP=%d,%d,\"%s\"\r", cntxtId, protoType, pApn, pUserName, pPW, authMethod);
    //     resultCode_t atResult = atcmd_awaitResult();
    //     if (atResult == resultCode__success)
    //     {
    //         atcmd_setOptions(atcmd__defaultTimeoutMS, S_contextStatusCompleteParser); 
    //         atcmd_invokeReuseLock("AT+QIACT=%d\r", cntxtId);
    //         atResult = atcmd_awaitResult();
    //         if (atResult == resultCode__success)
    //             pdpActivated = true;
    //     }
    // }
    // atcmd_close();
    // if (pdpActivated)
    //     ntwk_fetchActivePdpCntxts();
    // if (ntwk_getNetworkInfo(cntxtId) != NULL)
    //     return true;
    // // if fell through, PDP not available... clear network provider
    // memset((providerInfo_t*)g_ltem.providerInfo, 0, sizeof(providerInfo_t));
    // return false;
}


/**
 *	\brief Deactivate PDP Context/APN.
 */
void ntwk_deactivateNetwork(uint8_t cntxtId)
{
    atcmd_setOptions(atcmd__defaultTimeoutMS, S_contextStatusCompleteParser);
    if (ATCMD_awaitLock(atcmd__defaultTimeoutMS))
    {
        if (atcmd_tryInvokeOptions("AT+QIDEACT=%d\r", cntxtId))
        {
            resultCode_t atResult = atcmd_awaitResult();
            if ( atResult == resultCode__success)
                ntwk_getActivePdpContexts();
        }
    }
}


/**
 *	\brief Get count of APN active data contexts from BGx.
 */
uint8_t ntwk_readActiveNetworkCount()
{
    #define IP_QIACT_SZ 8

    uint8_t ntwkIndx = 0;

    if (ATCMD_awaitLock(atcmd__defaultTimeoutMS))
    {
        atcmd_setOptions(atcmd__defaultTimeoutMS, S_contextStatusCompleteParser);
        atcmd_invokeReuseLock("AT+QIACT?");
        resultCode_t atResult = atcmd_awaitResult();

        memset(&((providerInfo_t*)g_ltem.providerInfo)->networks, 0, sizeof(networkInfo_t) * ntwk__pdpContextCnt);    // empty context table for return, if success refill from parsing

        if (atResult != resultCode__success)
        {
            ntwkIndx = 0xFF;
            goto finally;
        }

        if (strlen(ATCMD_getLastResponse()) > IP_QIACT_SZ)
        {
            #define TOKEN_BUF_SZ 16
            char *nextContext;
            char *landmarkAt;
            char *continuePtr;
            uint8_t landmarkSz = IP_QIACT_SZ;
            char tokenBuf[TOKEN_BUF_SZ];

            nextContext = strstr(ATCMD_getLastResponse(), "+QIACT: ");

            // no contexts returned = none active (only active contexts are returned)
            while (nextContext != NULL && ntwkIndx < ntwk__pdpContextCnt)                       // now parse each pdp context entry
            {
                landmarkAt = nextContext;
                networkInfo_t *ntwk = &((providerInfo_t*)g_ltem.providerInfo)->networks[ntwkIndx];

                ntwk->contextId = strtol(landmarkAt + landmarkSz, &continuePtr, 10);
                continuePtr = strchr(++continuePtr, ',');                                       // skip context_state: always 1
                ntwk->protoType = (int)strtol(continuePtr + 1, &continuePtr, 10);
                continuePtr = S_grabToken(continuePtr + 2, '"', tokenBuf, TOKEN_BUF_SZ);
                if (continuePtr != NULL)
                {
                    strncpy(ntwk->ipAddress, tokenBuf, TOKEN_BUF_SZ);
                    ntwk->isActive = true;
                }
                nextContext = strstr(nextContext + landmarkSz, "+QIACT: ");                     // URC hdr is repeated for each context
                ntwkIndx++;
            }
        }
    }
    finally:
        atcmd_close();
        return ntwkIndx;
}


/**
 *	\brief Get network (PDP) information
 */
networkInfo_t *ntwk_getNetworkInfo(uint8_t contextId)
{
    uint8_t ntwkCnt = 0;
    do
    {
        if (ltem_readDeviceState())
        {
            providerInfo_t *provider = g_ltem.providerInfo;                                         // cast to provider pointer
            for (size_t i = 0; i < ntwk__pdpContextCnt; i++)
            {
                if(provider->networks[i].contextId == contextId && provider->networks[i].isActive)
                    return &provider->networks[i];
            }
        }
        ntwkCnt = ntwk_readActiveNetworkCount();
    } while (ntwkCnt > 0);
    return NULL;
}


/** 
 *  \brief Development/diagnostic function to retrieve visible operators from radio.
 */
void ntwk_getOperators(char *operatorList, uint16_t listSz)
{
    // AT+COPS=?

    if (ATCMD_awaitLock(atcmd__defaultTimeoutMS))
    {
        atcmd_setOptions(PERIOD_FROM_SECONDS(180), atcmd__useDefaultOKCompletionParser);

        if (((modemInfo_t*)g_ltem.modemInfo)->imei[0] == 0)
        {
            atcmd_invokeReuseLock("AT+COPS=?");
            if (atcmd_awaitResult() == resultCode__success)
            {
                strncpy(operatorList, ATCMD_getLastResponse() + 9, listSz - 1);
            }
        }
    }
    atcmd_close();
}


#pragma endregion


/* private functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions


/**
 *   \brief Tests for the completion of a network APN context activate action.
 *   \return standard action result integer (http result).
*/
static cmdParseRslt_t S_contextStatusCompleteParser(void *atcmd, const char *response)
{
    return atcmd__defaultResponseParser(&g_ltem, "+QIACT: ", false, NULL, 2, 2, NULL);
}



/**
 *   \brief Get the network operator name and network mode.
 *   \return Struct containing the network operator name (operName) and network mode (ntwkMode).
*/
static providerInfo_t *S_getNetworkProvider()
{
    ASSERT(g_ltem.providerInfo != NULL, srcfile_ltemc_network_c);

    if (strlen(((providerInfo_t*)g_ltem.providerInfo)->name) > 0)
        return (providerInfo_t*)g_ltem.providerInfo;

    if (ATCMD_awaitLock(atcmd__defaultTimeoutMS))
    {
        memset(g_ltem.providerInfo, 0, sizeof(providerInfo_t));
        
        atcmd_setOptions(atcmd__defaultTimeoutMS, atcmd__useDefaultOKCompletionParser);
        atcmd_invokeReuseLock("AT+COPS?");
        if (atcmd_awaitResult() == resultCode__success)
        {
            char *continuePtr;
            continuePtr = strchr(ATCMD_getLastResponse(), '"');
            if (continuePtr != NULL)
            {
                continuePtr = S_grabToken(continuePtr + 1, '"', ((providerInfo_t*)g_ltem.providerInfo)->name, ntwk__providerNameSz);

                uint8_t ntwkMode = (uint8_t)strtol(continuePtr + 1, &continuePtr, 10);
                if (ntwkMode == 8)
                    strcpy(((providerInfo_t*)g_ltem.providerInfo)->iotMode, "M1");
                else
                    strcpy(((providerInfo_t*)g_ltem.providerInfo)->iotMode, "NB1");
            }
        }
        else
        {
            ((providerInfo_t*)g_ltem.providerInfo)->name[0] = 0;
            ((providerInfo_t*)g_ltem.providerInfo)->iotMode[0] = 0;
        }
        atcmd_close();
        return (providerInfo_t*)g_ltem.providerInfo;
    }
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
static char *S_grabToken(char *source, int delimiter, char *tokenBuf, uint8_t tokenBufSz) 
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
