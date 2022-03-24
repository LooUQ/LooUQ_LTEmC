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

#define PROTOCOLS_CMD_BUFFER_SZ 80
#define MIN(x, y) (((x)<(y)) ? (x):(y))


extern ltemDevice_t g_ltem;

// local static functions
static resultCode_t s_contextStatusCompleteParser(const char *response, char **endptr);
static networkOperator_t s_getNetworkOperator();
static char *S_grabToken(char *source, int delimiter, char *tokenBuf, uint8_t tokenBufSz);


/* public tcpip functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions

/**
 *	@brief Initialize the IP network contexts structure.
 */
void ntwk_create()
{
    network_t *networkPtr = (network_t*)calloc(1, sizeof(network_t));
    ASSERT(networkPtr != NULL, srcfile_ltemc_network_c);

    networkPtr->networkOperator = calloc(1, sizeof(networkOperator_t));
    pdpCntxt_t *context = calloc(ntwk__pdpContextCnt, sizeof(pdpCntxt_t));
    ASSERT(context != NULL, srcfile_ltemc_network_c);

    for (size_t i = 0; i < sizeof(networkPtr->pdpCntxts)/sizeof(pdpCntxt_t); i++)
    {   
        networkPtr->pdpCntxts[i].protoType = pdpCntxtProtocolType_IPV4;
        memset(networkPtr->pdpCntxts[i].ipAddress, 0, sizeof(networkPtr->pdpCntxts[i].ipAddress));
    }
    g_ltem.network = networkPtr;
}


/**
 *  @brief Configure RAT searching sequence
*/
void ntwk_setNwScanSeq(const char* scanSequence)
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
void ntwk_setNwScanMode(ntwkScanMode_t scanMode)
{
    // AT+QCFG="nwscanmode"[,<scanmode>[,<effect>]]
    atcmd_tryInvoke("AT+QCFG=\"nwscanmode\",%d", scanMode);
    atcmd_awaitResult();
}


/** 
 *  @brief Configure the network category to be searched under LTE RAT.
 */
void ntwk_setIotOpMode(ntwk_iotMode_t iotMode)
{
    atcmd_tryInvoke("AT+QCFG=\"iotopmode\",%d", iotMode);
    atcmd_awaitResult();
}


/**
 *   @brief Wait for a network operator name and network mode. Can be cancelled in threaded env via g_ltem->cancellationRequest.
*/
networkOperator_t ntwk_awaitOperator(uint16_t waitDurSeconds)
{
    networkOperator_t ntwk;
    uint32_t startMillis, endMillis;
    startMillis = endMillis = pMillis();
    uint32_t waitDuration = (waitDurSeconds > 300) ? 300000 : waitDurSeconds * 1000;    // max is 5 minutes

    do 
    {
        ntwk = s_getNetworkOperator();
        if (*ntwk.operName != 0)
            break;
        pDelay(1000);                               // this yields, allowing alternate execution
        endMillis = pMillis();

    //       timed out waiting                      || global cancellation
    } while (endMillis - startMillis < waitDuration || g_ltem.cancellationRequest);
    PRINTF(dbgColor__dMagenta, "ntwkOp returned in %d ms\r", (endMillis - startMillis) / 1000);
    return ntwk;
}


/**
 *	@brief Activate PDP Context/APN.
 */
bool ntwk_activatePdpContext(uint8_t cntxtId, pdpCntxtProtocolType_t protoType, const char *pApn)
{
    if (ntwk_getPdpCntxtInfo(cntxtId) != NULL)
        return true;

    bool pdpActivated = false;
    if (ATCMD_awaitLock(atcmd__defaultTimeoutMS))
    {
        atcmd_setOptions(atcmd__defaultTimeoutMS, atcmd__useDefaultOKCompletionParser);
        atcmd_invokeReuseLock("AT+QICSGP=%d,%d,\"%s\"\r", cntxtId, protoType, pApn);

        resultCode_t atResult = atcmd_awaitResult();
        if (atResult == resultCode__success)
        {
            atcmd_setOptions(atcmd__defaultTimeoutMS, s_contextStatusCompleteParser); 
            atcmd_invokeReuseLock("AT+QIACT=%d\r", cntxtId);
            atResult = atcmd_awaitResult();
            if (atResult == resultCode__success)
                pdpActivated = true;
        }
    }
    atcmd_close();

    if (pdpActivated)
        ntwk_fetchActivePdpCntxts();

    if (ntwk_getPdpCntxtInfo(cntxtId) != NULL)
        return true;

    // if fell through, PDP not available. Clear network operator too, next attempt may change network
    memset(((network_t*)g_ltem.network)->networkOperator, 0, sizeof(networkOperator_t));
    return false;
}


/**
 *	@brief Activate PDP Context/APN requiring authentication.
 */
bool ntwk_activatePdpContextWithAuth(uint8_t cntxtId, pdpCntxtProtocolType_t protoType, const char *pApn, const char *pUserName, const char *pPW, pdpCntxtAuthMethods_t authMethod)
{
    if (ntwk_getPdpCntxtInfo(cntxtId) != NULL)
        return true;

    bool pdpActivated = false;
    if (ATCMD_awaitLock(atcmd__defaultTimeoutMS))
    {
        atcmd_setOptions(atcmd__defaultTimeoutMS, atcmd__useDefaultOKCompletionParser);
        atcmd_invokeReuseLock("AT+QICSGP=%d,%d,\"%s\"\r", cntxtId, protoType, pApn, pUserName, pPW, authMethod);

        resultCode_t atResult = atcmd_awaitResult();
        if (atResult == resultCode__success)
        {
            atcmd_setOptions(atcmd__defaultTimeoutMS, s_contextStatusCompleteParser); 
            atcmd_invokeReuseLock("AT+QIACT=%d\r", cntxtId);
            atResult = atcmd_awaitResult();
            if (atResult == resultCode__success)
                pdpActivated = true;
        }
    }
    atcmd_close();

    if (pdpActivated)
        ntwk_fetchActivePdpCntxts();

    if (ntwk_getPdpCntxtInfo(cntxtId) != NULL)
        return true;

    // if fell through, PDP not available. Clear network operator too, next attempt may change network
    memset(((network_t*)g_ltem.network)->networkOperator, 0, sizeof(networkOperator_t));
    return false;
}


/**
 *	@brief Deactivate PDP Context/APN.
 */
void ntwk_deactivatePdpContext(uint8_t cntxtId)
{
    atcmd_setOptions(atcmd__defaultTimeoutMS, s_contextStatusCompleteParser);
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
uint8_t ntwk_fetchActivePdpCntxts()
{
    #define IP_QIACT_SZ 8

    uint8_t apnIndx = 0;

    if (ATCMD_awaitLock(atcmd__defaultTimeoutMS))
    {
        atcmd_setOptions(atcmd__defaultTimeoutMS, s_contextStatusCompleteParser);
        atcmd_invokeReuseLock("AT+QIACT?");
        resultCode_t atResult = atcmd_awaitResult();

        memset(&((network_t*)g_ltem.network)->pdpCntxts, 0, sizeof(pdpCntxt_t) * ntwk__pdpContextCnt);    // empty context table for return, if success refill from parsing
        // for (size_t i = 0; i < NTWK__pdpContextCnt; i++)         // empty context table return and if success refill from parsing
        // {
        //     ((network_t*)g_ltem.network)->pdpCntxts[i].contextId = 0;
        //     ((network_t*)g_ltem.network)->pdpCntxts[i].ipAddress[0] = 0;
        // }

        if (atResult != resultCode__success)
        {
            apnIndx = 0xFF;
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
            while (nextContext != NULL && apnIndx < ntwk__pdpContextCnt)                            // now parse each pdp context entry
            {
                landmarkAt = nextContext;
                ((network_t*)g_ltem.network)->pdpCntxts[apnIndx].contextId = strtol(landmarkAt + landmarkSz, &continuePtr, 10);
                continuePtr = strchr(++continuePtr, ',');                                           // skip context_state: always 1

                ((network_t*)g_ltem.network)->pdpCntxts[apnIndx].protoType = (int)strtol(continuePtr + 1, &continuePtr, 10);
                continuePtr = S_grabToken(continuePtr + 2, '"', tokenBuf, TOKEN_BUF_SZ);
                if (continuePtr != NULL)
                {
                    strncpy(((network_t*)g_ltem.network)->pdpCntxts[apnIndx].ipAddress, tokenBuf, TOKEN_BUF_SZ);
                }
                nextContext = strstr(nextContext + landmarkSz, "+QIACT: ");
                apnIndx++;
            }
        }
    }
    finally:
        atcmd_close();
        return apnIndx;
}


/**
 *	@brief Get PDP Context/APN information
 */
pdpCntxt_t *ntwk_getPdpCntxtInfo(uint8_t cntxtId)
{
    if (ltem_chkHwReady())
    {
        for (size_t i = 0; i < ntwk__pdpContextCnt; i++)
        {
            if(((network_t*)g_ltem.network)->pdpCntxts[i].contextId != 0)
                return &((network_t*)g_ltem.network)->pdpCntxts[i];
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
static resultCode_t s_contextStatusCompleteParser(const char *response, char **endptr)
{
    return atcmd_defaultResultParser(response, "+QIACT: ", false, 2, "OK\r\n", endptr);
}



/**
 *   @brief Get the network operator name and network mode.
 * 
 *   @return Struct containing the network operator name (operName) and network mode (ntwkMode).
*/
static networkOperator_t s_getNetworkOperator()
{
    if (*((network_t*)g_ltem.network)->networkOperator->operName != 0)
        return *((network_t*)g_ltem.network)->networkOperator;

    if (!ATCMD_awaitLock(atcmd__defaultTimeoutMS))
        goto finally;

    atcmd_setOptions(atcmd__defaultTimeoutMS, atcmd__useDefaultOKCompletionParser);
    atcmd_invokeReuseLock("AT+COPS?");
    if (atcmd_awaitResult() == resultCode__success)
    {
        char *continuePtr;
        continuePtr = strchr(ATCMD_getLastResponse(), '"');
        if (continuePtr != NULL)
        {
            continuePtr = S_grabToken(continuePtr + 1, '"', ((network_t*)g_ltem.network)->networkOperator->operName, ntwk__operatorNameSz);

            uint8_t ntwkMode = (uint8_t)strtol(continuePtr + 1, &continuePtr, 10);
            if (ntwkMode == 8)
                strcpy(((network_t*)g_ltem.network)->networkOperator->ntwkMode, "M1");
            else
                strcpy(((network_t*)g_ltem.network)->networkOperator->ntwkMode, "NB1");
        }
    }
    else
    {
        ((network_t*)g_ltem.network)->networkOperator->operName[0] = 0;
        ((network_t*)g_ltem.network)->networkOperator->ntwkMode[0] = 0;
    }

    finally:
        atcmd_close();
        return *((network_t*)g_ltem.network)->networkOperator;
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
