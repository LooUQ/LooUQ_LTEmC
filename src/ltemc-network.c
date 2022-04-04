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
#include <lq-network.h>

#define PROTOCOLS_CMD_BUFFER_SZ 80
#define MIN(x, y) (((x)<(y)) ? (x):(y))


// Modem global (singleton)
extern ltemDevice_t g_ltem;


// local static functions
static resultCode_t S_contextStatusCompleteParser(const char *response, char **endptr);
static providerInfo_t *S_getNetworkProvider();
static char *S_grabToken(char *source, int delimiter, char *tokenBuf, uint8_t tokenBufSz);


/* public tcpip functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions

/**
 *	@brief Initialize the IP network contexts structure.
 */
void ntwk_create()
{
    providerNetworks_t *providerNetworksPtr = (providerNetworks_t*)calloc(1, sizeof(providerNetworks_t));
    ASSERT(providerNetworksPtr != NULL, srcfile_ltemc_network_c);

    providerNetworksPtr->provider = calloc(1, sizeof(providerInfo_t));
    ASSERT(providerNetworksPtr->provider != NULL, srcfile_ltemc_network_c);

    for (size_t i = 0; i < ntwk__pdpContextCnt; i++)
    {   
        networkInfo_t *networkPtr = calloc(1, sizeof(networkInfo_t));
        ASSERT(networkPtr != NULL, srcfile_ltemc_network_c);

        networkPtr->protoType = networkPDPType_IPV4;
        memset(networkPtr->ipAddress, 0, sizeof(networkPtr->ipAddress));
        providerNetworksPtr->networks[i] = networkPtr;
    }
    g_ltem.providerNetworks = providerNetworksPtr;
}


/**
 *  @brief Configure RAT searching sequence
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
 *  @brief Configure RAT(s) allowed to be searched.
*/
void ntwk_setProviderScanMode(ntwkScanMode_t scanMode)
{
    // AT+QCFG="nwscanmode"[,<scanmode>[,<effect>]]
    atcmd_tryInvoke("AT+QCFG=\"nwscanmode\",%d", scanMode);
    atcmd_awaitResult();
}


/** 
 *  @brief Configure the network category to be searched under LTE RAT.
 */
void ntwk_setIotMode(ntwk_iotMode_t iotMode)
{
    atcmd_tryInvoke("AT+QCFG=\"iotopmode\",%d", iotMode);
    atcmd_awaitResult();
}


/**
 *   @brief Wait for a network provider name and network mode. Can be cancelled in threaded env via g_ltem->cancellationRequest.
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
        provider = S_getNetworkProvider();                                              // perform a single provider query (COPS?): process if provider found, else return NULL

        if (provider != NULL)
        {
            PRINTF(dbgColor__dMagenta, "ntwkOp returned in %d ms\r", (endMillis - startMillis) / 1000);
            return provider;
        }
        pDelay(1000);                                                                   // this yields, allowing alternate execution
        endMillis = pMillis();
    } while (endMillis - startMillis < waitDuration || g_ltem.cancellationRequest);     // timed out waiting || global cancellation
    return NULL;
}


/**
 *	@brief Activate PDP Context/APN.
 */
bool ntwk_activateNetwork(uint8_t cntxtId, networkPDPType_t protoType, const char *apn)
{
    if (ntwk_getNetworkInfo(cntxtId) != NULL)
        return true;

    bool pdpActivated = false;
    if (ATCMD_awaitLock(atcmd__defaultTimeoutMS))
    {
        atcmd_setOptions(atcmd__defaultTimeoutMS, atcmd__useDefaultOKCompletionParser);
        atcmd_invokeReuseLock("AT+QICSGP=%d,%d,\"%s\"\r", cntxtId, protoType, apn);

        resultCode_t atResult = atcmd_awaitResult();
        if (atResult == resultCode__success)
        {
            atcmd_setOptions(atcmd__defaultTimeoutMS, S_contextStatusCompleteParser); 
            atcmd_invokeReuseLock("AT+QIACT=%d\r", cntxtId);
            atResult = atcmd_awaitResult();
            if (atResult == resultCode__success)
                pdpActivated = true;
        }
    }
    atcmd_close();

    if (pdpActivated)
        ntwk_readActiveNetworkCount();

    if (ntwk_getNetworkInfo(cntxtId) != NULL)
        return true;

    // if fell through, PDP not available. Clear network provider, next attempt may change network
    memset(((providerNetworks_t*)g_ltem.providerNetworks)->provider, 0, sizeof(providerInfo_t));
    return false;
}

// msgSession msgProto

/**
 *	@brief Activate PDP Context/APN requiring authentication.
 */
bool ntwk_activateNetworkWithAuth(uint8_t cntxtId, networkPDPType_t protoType, const char *pApn, const char *pUserName, const char *pPW, pdpCntxtAuthMethods_t authMethod)
{
    if (ntwk_getNetworkInfo(cntxtId) != NULL)
        return true;

    bool pdpActivated = false;
    if (ATCMD_awaitLock(atcmd__defaultTimeoutMS))
    {
        atcmd_setOptions(atcmd__defaultTimeoutMS, atcmd__useDefaultOKCompletionParser);
        atcmd_invokeReuseLock("AT+QICSGP=%d,%d,\"%s\"\r", cntxtId, protoType, pApn, pUserName, pPW, authMethod);

        resultCode_t atResult = atcmd_awaitResult();
        if (atResult == resultCode__success)
        {
            atcmd_setOptions(atcmd__defaultTimeoutMS, S_contextStatusCompleteParser); 
            atcmd_invokeReuseLock("AT+QIACT=%d\r", cntxtId);
            atResult = atcmd_awaitResult();
            if (atResult == resultCode__success)
                pdpActivated = true;
        }
    }
    atcmd_close();

    if (pdpActivated)
        ntwk_fetchActivePdpCntxts();

    if (ntwk_getNetworkInfo(cntxtId) != NULL)
        return true;

    // if fell through, PDP not available. Clear network operator too, next attempt may change network
    memset(((providerNetworks_t*)g_ltem.providerNetworks)->provider, 0, sizeof(networkInfo_t));
    return false;
}


/**
 *	@brief Deactivate PDP Context/APN.
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
 *	@brief Get count of APN active data contexts from BGx.
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

        memset(&((providerNetworks_t*)g_ltem.providerNetworks)->networks, 0, sizeof(networkInfo_t) * ntwk__pdpContextCnt);    // empty context table for return, if success refill from parsing

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
            while (nextContext != NULL && ntwkIndx < ntwk__pdpContextCnt)                           // now parse each pdp context entry
            {
                landmarkAt = nextContext;

                ((providerNetworks_t*)g_ltem.providerNetworks)->networks[ntwkIndx]->contextId = strtol(landmarkAt + landmarkSz, &continuePtr, 10);
                continuePtr = strchr(++continuePtr, ',');                                           // skip context_state: always 1
                ((providerNetworks_t*)g_ltem.providerNetworks)->networks[ntwkIndx]->protoType = (int)strtol(continuePtr + 1, &continuePtr, 10);
                continuePtr = S_grabToken(continuePtr + 2, '"', tokenBuf, TOKEN_BUF_SZ);
                if (continuePtr != NULL)
                {
                    strncpy(((providerNetworks_t*)g_ltem.providerNetworks)->networks[ntwkIndx]->ipAddress, tokenBuf, TOKEN_BUF_SZ);
                }
                nextContext = strstr(nextContext + landmarkSz, "+QIACT: ");
                ntwkIndx++;
            }
        }
    }
    finally:
        atcmd_close();
        return ntwkIndx;
}


/**
 *	@brief Get network (PDP) information
 */
networkInfo_t *ntwk_getNetworkInfo(uint8_t contextId)
{
    if (ltem_readDeviceState())
    {
        for (size_t i = 0; i < ntwk__pdpContextCnt; i++)
        {
            if(((providerNetworks_t*)g_ltem.providerNetworks)->networks[i]->contextId == contextId)
                return &((providerNetworks_t*)g_ltem.providerNetworks)->networks[i];
        }
    }
    return NULL;
}


/** 
 *  @brief Development/diagnostic function to retrieve visible operators from radio.
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
 *   @brief Tests for the completion of a network APN context activate action.
 * 
 *   @return standard action result integer (http result).
*/
static resultCode_t S_contextStatusCompleteParser(const char *response, char **endptr)
{
    return atcmd_defaultResultParser(response, "+QIACT: ", false, 2, "OK\r\n", endptr);
}



/**
 *   @brief Get the network operator name and network mode.
 * 
 *   @return Struct containing the network operator name (operName) and network mode (ntwkMode).
*/
static providerInfo_t *S_getNetworkProvider()
{
    if (strlen(((providerNetworks_t*)g_ltem.providerNetworks)->provider->name) > 0)
        return ((providerNetworks_t*)g_ltem.providerNetworks)->provider;

    if (ATCMD_awaitLock(atcmd__defaultTimeoutMS))
    {
        atcmd_setOptions(atcmd__defaultTimeoutMS, atcmd__useDefaultOKCompletionParser);
        atcmd_invokeReuseLock("AT+COPS?");
        if (atcmd_awaitResult() == resultCode__success)
        {
            char *continuePtr;
            continuePtr = strchr(ATCMD_getLastResponse(), '"');
            if (continuePtr != NULL)
            {
                continuePtr = S_grabToken(continuePtr + 1, '"', ((providerNetworks_t*)g_ltem.providerNetworks)->provider->name, ntwk__providerNameSz);

                uint8_t ntwkMode = (uint8_t)strtol(continuePtr + 1, &continuePtr, 10);
                if (ntwkMode == 8)
                    strcpy(((providerNetworks_t*)g_ltem.providerNetworks)->provider->iotMode, "M1");
                else
                    strcpy(((providerNetworks_t*)g_ltem.providerNetworks)->provider->iotMode, "NB1");
            }
        }
        else
        {
            ((providerNetworks_t*)g_ltem.providerNetworks)->provider->name[0] = 0;
            ((providerNetworks_t*)g_ltem.providerNetworks)->provider->iotMode[0] = 0;
        }
        atcmd_close();
        return ((providerNetworks_t*)g_ltem.providerNetworks)->provider;
    }
}


/**
 *  @brief Scans a C-String (char array) for the next delimeted token and null terminates it.
 * 
 *  @param source [in] - Original char array to scan.
 *  @param delimeter [in] - Character separating tokens (passed as integer value).
 *  @param tokenBuf [out] - Pointer to where token should be copied to.
 *  @param tokenBufSz [in] - Size of buffer to receive token.
 * 
 *  @return Pointer to the location in the source string immediately following the token.
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
