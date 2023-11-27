/** ***************************************************************************
  @file 
  @brief Cellular/packet data network support features and services

  @author Greg Terrell, LooUQ Incorporated

  \loouq

  @warning Internal dependencies, changes only as directed by LooUQ staff.

-------------------------------------------------------------------------------

LooUQ-LTEmC // Software driver for the LooUQ LTEm series cellular modems.
Copyright (C) 2017-2023 LooUQ Incorporated

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
Also add information on how to contact you by electronic and paper mail.

**************************************************************************** */


#define SRCFILE "NWK"                       // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT
#define ENABLE_DIAGPRINT                    // expand DPRINT into debug output
//#define ENABLE_DIAGPRINT_VERBOSE            // expand DPRINT and DPRINT_V into debug output
#define ENABLE_ASSERT
#include <lqdiag.h>

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "ltemc-iTypes.h"
#include "ltemc-network.h"

extern ltemDevice_t g_lqLTEM;


#define PROTOCOLS_CMD_BUFFER_SZ 80
#define MIN(x, y) (((x)<(y)) ? (x):(y))
#define STREMPTY(charvar)  (charvar == NULL || charvar[0] == 0 )


// local static functions
static cmdParseRslt_t S__contextStatusCompleteParser(void);
static char *S__grabToken(char *source, int delimiter, char *tokenBuf, uint8_t tokenBufSz);
static void S__clearOperatorInfo();


/* public tcpip functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions

/**
 *	@brief Initialize the IP network contexts structure.
 */
void ntwk_create()
{
    ntwkOperator_t *ntwkOperatorPtr = (ntwkOperator_t*)calloc(1, sizeof(ntwkOperator_t));
    ASSERT(ntwkOperatorPtr != NULL);
    g_lqLTEM.ntwkOperator = ntwkOperatorPtr;
}


/**
 *	@brief Build default data context configuration for modem to use on startup.
 */
resultCode_t ntwk_setDefaultNetwork(uint8_t pdpContextId, pdpProtocol_t protoType, const char *apn)
{
    return ntwk_configPdpNetwork(pdpContextId, protoType, apn);
}


/**
 *  @brief Configure RAT searching sequence
*/
void ntwk_setOperatorScanSeq(const char* scanSequence)
{
    /*AT+QCFG="nwscanseq"[,<scanseq>[,effect]]
    */
    if (strlen(scanSequence) > 0)
    {
        strcpy(g_lqLTEM.ntwkSettings->scanSequence, scanSequence);
        if (ltem_getDeviceState() == deviceState_ready)
        {
            if (!ATCMD_tryInvoke("AT+QCFG=\"nwscanseq\",%s", scanSequence))
                return;
            ATCMD_awaitResult();
        }
    }
}


/** 
 *  @brief Configure RAT(s) allowed to be searched.
*/
void ntwk_setOperatorScanMode(ntwkScanMode_t scanMode)
{
   /* AT+QCFG="nwscanmode"[,<scanmode>[,<effect>]]
    */
   if (strstr(ltem_getModuleType(), "BG9") != NULL)                         // BG96, BG95 only
   {
        g_lqLTEM.ntwkSettings->scanMode = scanMode; 
        if (ltem_getDeviceState() == deviceState_ready)
        {
            if (!ATCMD_tryInvoke("AT+QCFG=\"nwscanmode\",%d", scanMode))
                return;
            ATCMD_awaitResult();
        }
   }
}


/** 
 *  @brief Configure the network category to be searched under LTE RAT.
 */
void ntwk_setIotMode(ntwkIotMode_t iotMode)
{
    /* AT+QCFG="iotopmode",<mode>
    */
    g_lqLTEM.ntwkSettings->iotMode = iotMode; 
    if (ltem_getDeviceState() == deviceState_ready)
    {
        if (!ATCMD_tryInvoke("AT+QCFG=\"iotopmode\",%d", iotMode))
            return;
        ATCMD_awaitResult();
    }
}



/**
 *	@brief Initialize BGx Radio Access Technology (RAT) options.
 */
void ntwk_setRatOptions()
{
    ntwk_setOperatorScanSeq(g_lqLTEM.ntwkSettings->scanSequence);
    ntwk_setOperatorScanMode(g_lqLTEM.ntwkSettings->scanMode);
    ntwk_setIotMode(g_lqLTEM.ntwkSettings->iotMode);
}


resultCode_t ntwk_configPdpNetwork(uint8_t pdpContextId, pdpProtocol_t protoType, const char *apn)
{
    ASSERT(g_lqLTEM.ntwkOperator != NULL);                                              // ASSERT g_lqLTEM.ntwkOperator has been initialized
    ASSERT_W(protoType == pdpProtocol_IPV4, "OnlyIPV4SupportedCurrently");              // warn on not IPv4

    snprintf(g_lqLTEM.ntwkSettings->pdpNtwkConfig, sizeof(g_lqLTEM.ntwkSettings->pdpNtwkConfig), "AT+QICSGP=%d,%d,\"%s\"\r", pdpContextId, protoType, apn);
    if (g_lqLTEM.deviceState == deviceState_ready)
    {
        NTWK_applyPpdNetworkConfig();
    }
    return resultCode__accepted;
}


/**
 *	@brief Configure PDP Context requiring authentication
 *  \details This configuration only supports IP4 data contexts
 */
resultCode_t ntwk_configPdpNetworkWithAuth(uint8_t pdpContextId, const char *apn, const char *userName, const char *pw, pdpCntxtAuthMethods_t authMethod)
{
    resultCode_t rslt;

    snprintf(g_lqLTEM.ntwkSettings->pdpNtwkConfig, sizeof(g_lqLTEM.ntwkSettings->pdpNtwkConfig), "AT+QICSGP=%d,1,\"%s\",\"%s\",\"%s\",%d", pdpContextId, apn, userName, pw, authMethod);

    if(!ATCMD_tryInvoke(g_lqLTEM.ntwkSettings->pdpNtwkConfig))
        return resultCode__locked;

    return ATCMD_awaitResult();
}


/**
 *	@brief Apply the a PDP context configuration settings to BGx.
 */
void NTWK_applyPpdNetworkConfig()
{
    if(strlen(g_lqLTEM.ntwkSettings->pdpNtwkConfig) > 0)
    {
        if (ATCMD_tryInvoke(g_lqLTEM.ntwkSettings->pdpNtwkConfig))
        {
            resultCode_t _rslt;
            if (IS_NOTSUCCESS__RSLT(ATCMD_awaitResult()))
            {
                DPRINT(PRNT_CYAN, "DefaultNtwk Config Failed=%d\r", _rslt);
            }
        }
    }
}


/**
 *   @brief Wait for a network operator name and network mode. Can be cancelled in threaded env via g_lqLTEM->cancellationRequest.
*/
ntwkOperator_t* ntwk_awaitOperator(uint16_t waitSec)
{
    ASSERT(g_lqLTEM.ntwkOperator != NULL);                                  // ASSERT g_lqLTEM.ntwkOperator struct has been initialized

    uint32_t startMillis, endMillis;
    startMillis = endMillis = pMillis();
    uint32_t waitMs = (waitSec > 300) ? 300000 : waitSec * 1000;            // max is 5 minutes

    S__clearOperatorInfo();
    do 
    {
        if (!ATCMD_tryInvoke("AT+COPS?"))                                   // get PROVIDER cellular carrier
            return g_lqLTEM.ntwkOperator;

        if (IS_SUCCESS(ATCMD_awaitResult()))
        {
            char *pContinue;
            pContinue = strchr(ATCMD_getResponseData(), '"');
            if (pContinue != NULL)
            {
                pContinue = S__grabToken(pContinue + 1, '"', g_lqLTEM.ntwkOperator->name, ntwk__operatorNameSz);

                uint8_t ntwkMode = (uint8_t)strtol(pContinue + 1, &pContinue, 10);
                if (ntwkMode == 8)
                    strcpy(g_lqLTEM.ntwkOperator->iotMode, "M1");
                else if (ntwkMode == 9)
                    strcpy(g_lqLTEM.ntwkOperator->iotMode, "NB1");
                else
                    strcpy(g_lqLTEM.ntwkOperator->iotMode, "GSM");
            }
        }
        if (!STREMPTY(g_lqLTEM.ntwkOperator->name))
            break;

        pDelay(1000);                                                                   // this yields, allowing alternate execution
        endMillis = pMillis();
    } while (endMillis - startMillis < waitMs || g_lqLTEM.cancellationRequest);         // timed out waiting OR global cancellation


    // got PROVIDER, get packet network information

    if (!STREMPTY(g_lqLTEM.ntwkOperator->name))                                         // parse for packetNetworks
    {
        char *pContinue;
        uint8_t ntwkIndx = 0;

        ATCMD_ovrrdTimeout(SEC_TO_MS(20));

        if (!ATCMD_tryInvoke("AT+QIACT?"))
            return NULL;

        if (IS_SUCCESS(ATCMD_awaitResult()))
        {
            pContinue = strstr(ATCMD_getResponseData(), "+QIACT: ");
            while (pContinue != NULL && ntwkIndx < ntwk__pdpContextCnt)
            {
                g_lqLTEM.ntwkOperator->packetNetworks[ntwkIndx].pdpContextId = strtol(pContinue + 8, &pContinue, 10);
                g_lqLTEM.ntwkOperator->packetNetworks[ntwkIndx].isActive = *(++pContinue) == '1';
                // only supported protocol now is IPv4, alias IP
                g_lqLTEM.ntwkOperator->packetNetworks[ntwkIndx].pdpProtocol = pdpProtocol_IPV4;
                pContinue = strstr(pContinue, "+QIACT: ");
                if (pContinue == NULL)
                    break;
                ntwkIndx++;
            }
        }

        // get IP addresses
        /* NOTE: BGx will not return response for AT+CGPADDR *OVER THE SERIAL PORT*, unless it is suffixed with the contextID
        * This is one of a handfull of commands that exhibit this behavior; AT+CGPADDR works perfectly over the USB AT port.
        */

        for (size_t i = 0; i <= ntwkIndx; i++)
        {
            if (g_lqLTEM.ntwkOperator->packetNetworks[i].isActive)
            {
                ATCMD_tryInvoke("AT+CGPADDR=%d", g_lqLTEM.ntwkOperator->packetNetworks[i].pdpContextId);
                if (ATCMD_awaitResult() == resultCode__success)
                {
                    pContinue = strstr(ATCMD_getResponseData(), "+CGPADDR: ");
                    pContinue = strchr(pContinue + 10, ',') + 1;
                    char *pLineEnd = strchr(pContinue, '\r');
                    strncpy(g_lqLTEM.ntwkOperator->packetNetworks[i].ipAddress, pContinue, MIN(pLineEnd - pContinue, ntwk__ipAddressSz));
                }
            }
            else
            {
                strcpy(g_lqLTEM.ntwkOperator->packetNetworks[i].ipAddress, "0.0.0.0");
            }
        }
        g_lqLTEM.ntwkOperator->pdpCntxtCnt = ++ntwkIndx;
    }

    return g_lqLTEM.ntwkOperator;
}


/**
 *	@brief Deactivate PDP Context.
 */
void ntwk_activatePdpContext(uint8_t cntxtId)
{
    ATCMD_ovrrdParser(S__contextStatusCompleteParser);
    if (ATCMD_tryInvoke("AT+QIACT=%d", cntxtId))
    {
        ATCMD_awaitResult();
    }
}


/**
 *	@brief Deactivate PDP Context.
 */
void ntwk_deactivatePdpContext(uint8_t cntxtId)
{
    if (!ATCMD_tryInvoke("AT+QIDEACT=%d", cntxtId))
    {
        ATCMD_awaitResult();
    }
}


/**
 *	@brief Returns true if context is ready and updates LTEm internal network information for the context
 */
bool ntwk_getPdpContextState(uint8_t cntxtId)
{
    return true;
}


/**
 *   @brief Get current operator information. If not connected to a operator will be an empty operatorInfo struct
 *   @return Struct containing the network operator name (operName) and network mode (ntwkMode).
*/
ntwkOperator_t *ntwk_getOperatorInfo()
{
    if (strlen(g_lqLTEM.ntwkOperator->name))
        return g_lqLTEM.ntwkOperator;
    return NULL;
}


/**
 *	@brief Get count of APN active data contexts from BGx.
 */
uint8_t ntwk_getActiveContextCount()
{
    return g_lqLTEM.ntwkOperator->pdpCntxtCnt;
}


/**
 *	@brief Get network (PDP) information
 */
packetNetwork_t *ntwk_getPacketNetwork(uint8_t pdpContextId)
{
    for (size_t i = 0; i < g_lqLTEM.ntwkOperator->pdpCntxtCnt; i++)
    {
        if (g_lqLTEM.ntwkOperator->packetNetworks[i].pdpContextId == pdpContextId)
        {
            return &g_lqLTEM.ntwkOperator->packetNetworks[i];
        }
    }
    return NULL;
}

/**
 * @brief Get information about the active operator network
 * @return Char array pointer describing operator network characteristics
 */
const char* ntwk_getNetworkInfo()
{
    if (!ATCMD_tryInvoke("AT+QNWINFO"))
        return "";

    if (IS_SUCCESS(ATCMD_awaitResult()))
    {
        if (strstr(g_lqLTEM.atcmd->rawResponse, "+QNWINFO: "))                          // clean up extra content/EOL
        {
            void* eol = memchr(g_lqLTEM.atcmd->rawResponse, '\r', atcmd__respBufferSz - 10);
            if (eol)
            {
                memcpy(g_lqLTEM.statics.reportBffr, g_lqLTEM.atcmd->rawResponse, (eol - (void*)g_lqLTEM.atcmd->rawResponse));
                return g_lqLTEM.statics.reportBffr + 10;
            }
        }
    }
    return NULL;
}


/**
 *	@brief Get current network registration status.
 * @return The current network operator registration status.
 */
resultCode_t ntwk_getRegistrationStatus()
{
    if (!ATCMD_tryInvoke("AT+CREG?"))
        return resultCode__locked;

    // TODO need a parser wrapper to grab <stat> (pos 2)
    resultCode_t rslt = ATCMD_awaitResult();
    return 0;
}


/**
 * @brief Check network ready condition.
 */
bool ntwk_isReady(bool refresh)
{
    if (refresh)
        ntwk_awaitOperator(0);

    return strlen(g_lqLTEM.ntwkOperator->name) > 0;
}



// /**
//  * @brief Set network operator.
//  * @details The characteristics of the selected operator are accessible using the ATCMD_getResponseData() function.

//  * @param [in] mode Action to be performed, set/clear/set default.
//  * @param [in] format The form for the ntwkOperator parameter value: long, short, numeric.
//  * @param [in] ntwkOperator Operator to select, presented in the "format". Not all modes require/act on this parameter.
//  * @return Current operator selection mode. Note:
//  */
// uint8_t ntwk_setOperator(uint8_t mode, uint8_t format, const char* ntwkOperator)
// {
// }


/** 
 *  @brief Development/diagnostic function to retrieve visible operators from cell radio.
 */
const char* NTWKDIAGNOSTICS_getOperators()
{
    /* AT+COPS=? */
    ASSERT_W(false, "NTWKDIAGNOSTICS_getOperators() blocks and is SLOW!");

    memset(g_lqLTEM.statics.reportBffr, 0, ltemSz__reportsBffrSz);

    memset(g_lqLTEM.statics.reportBffr, 0, ltemSz__reportsBffrSz);
    if (ATCMD_tryInvoke("AT+COPS=?"))
    {
        ATCMD_ovrrdTimeout(SEC_TO_MS(240));
        if (IS_SUCCESS(ATCMD_awaitResult()))
        {
            strncpy(g_lqLTEM.statics.reportBffr, ATCMD_getResponseData() + 9, MIN(ltemSz__reportsBffrSz, ATCMD_getResponseData() - 9));
        }
    }
    return g_lqLTEM.statics.reportBffr;
}


#pragma endregion


/* private functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions


static void S__clearOperatorInfo()
{
    memset((void*)g_lqLTEM.ntwkOperator->packetNetworks, 0, g_lqLTEM.ntwkOperator->pdpCntxtCnt * sizeof(packetNetwork_t));
    memset((void*)g_lqLTEM.ntwkOperator, 0, sizeof(ntwkOperator_t));
}


/**
 *   @brief Tests for the completion of a network APN context activate action.
 *   @return standard action result integer (http result).
*/
static cmdParseRslt_t S__contextStatusCompleteParser(void)
{
    return ATCMD_stdResponseParser("+QIACT: ", false, "", 2, 2, NULL, 0);
}


/**
 *  @brief Scans a C-String (char array) for the next delimeted token and null terminates it.
 * 
 *  @param [in] source - Original char array to scan.
 *  @param [in] delimeter - Character separating tokens (passed as integer value).
 *  @param [out] tokenBuf - Pointer to where token should be copied to.
 *  @param [in] tokenBufSz - Size of buffer to receive token.
 * 
 *  @return Pointer to the location in the source string immediately following the token.
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
