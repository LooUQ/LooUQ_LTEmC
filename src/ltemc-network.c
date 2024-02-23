/** ***************************************************************************
  @file 
  @brief Cellular/packet data network support features and services

  @author Greg Terrell, LooUQ Incorporated

  \loouq
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


#include <lq-embed.h>
#define lqLOG_LEVEL lqLOGLEVEL_VRBS                                 ///< Logging detail level for this source file
//#define DISABLE_ASSERTS                                           ///< ASSERT/ASSERT_W enabled by default, can be disabled 
#define LQ_SRCFILE "NWK"                                            ///< create SRCFILE (3 char) MACRO for lq-diagnostics lqASSERT

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "ltemc-network.h"
#include "ltemc-internal.h"

extern ltemDevice_t g_lqLTEM;                                       ///< Global singleton LTEm object

#define MIN(x, y) (((x)<(y)) ? (x):(y))                             ///< Returns the smaller of two numbers
#define STREMPTY(charvar)  (charvar == NULL || charvar[0] == 0 )    ///< Returns true if a character array is NULL or empty


// local static functions
// static void S__clearOperatorInfo();
// static cmdParseRslt_t S__contextStatusCompleteParser(void * atcmd, const char *response);
// static char *S__grabToken(char *source, int delimiter, char *tokenBuf, uint8_t tokenBufSz);


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
        strcpy(g_lqLTEM.modemSettings->scanSequence, scanSequence);
        if (ltem_getDeviceState() == deviceState_ready)
        {
            atcmd_dispatch("AT+QCFG=\"nwscanseq\",%s", scanSequence);
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
        g_lqLTEM.modemSettings->scanMode = scanMode; 
        if (ltem_getDeviceState() == deviceState_ready)
        {
            atcmd_dispatch("AT+QCFG=\"nwscanmode\",%d", scanMode);
        }
   }
}


/** 
 *  @brief Configure the network category to be searched under LTE RAT.
 */
void ntwk_setIotMode(ntwkIotMode_t iotMode)
{
    g_lqLTEM.modemSettings->iotMode = iotMode; 

    if (ltem_getDeviceState() == deviceState_ready)
    {
        atcmd_dispatch("AT+QCFG=\"iotopmode\",%d", iotMode);
    }
}



/**
 *	@brief Initialize BGx Radio Access Technology (RAT) options.
 */
void ntwk_setRatOptions()
{
    ntwk_setOperatorScanSeq(g_lqLTEM.modemSettings->scanSequence);
    ntwk_setOperatorScanMode(g_lqLTEM.modemSettings->scanMode);
    ntwk_setIotMode(g_lqLTEM.modemSettings->iotMode);
}


resultCode_t ntwk_configPdpNetwork(uint8_t pdpContextId, pdpProtocol_t protoType, const char *apn)
{
    ASSERT(g_lqLTEM.ntwkOperator != NULL);                                              // ASSERT g_lqLTEM.ntwkOperator has been initialized
    ASSERT_W(protoType == pdpProtocol_IPV4, "OnlyIPV4SupportedCurrently");              // warn on not IPv4

    g_lqLTEM.modemSettings->pdpContextId = pdpContextId;
    snprintf(g_lqLTEM.modemSettings->pdpNtwkConfig, sizeof(g_lqLTEM.modemSettings->pdpNtwkConfig), "AT+QICSGP=%d,%d,\"%s\"\r", pdpContextId, protoType, apn);

    RSLT = resultCode__accepted;
    if (g_lqLTEM.deviceState == deviceState_ready)
    {
        rslt = atcmd_dispatch(g_lqLTEM.modemSettings->pdpNtwkConfig);
    }
    return rslt;
}


/**
 *	@brief Configure PDP Context requiring authentication
 *  \details This configuration only supports IP4 data contexts
 */
resultCode_t ntwk_configPdpNetworkWithAuth(uint8_t pdpContextId, const char *apn, const char *userName, const char *pw, pdpCntxtAuthMethods_t authMethod)
{
    snprintf(g_lqLTEM.modemSettings->pdpNtwkConfig, sizeof(g_lqLTEM.modemSettings->pdpNtwkConfig), "AT+QICSGP=%d,1,\"%s\",\"%s\",\"%s\",%d", pdpContextId, apn, userName, pw, authMethod);
    return atcmd_dispatch(g_lqLTEM.modemSettings->pdpNtwkConfig);
}


/**
 *	@brief Apply the default PDP context configuration settings to BGx.
 */
void ntwk_applyPpdNetworkConfig()
{
    RSLT;
    if(strlen(g_lqLTEM.modemSettings->pdpNtwkConfig) > 0)
    {
        if (!IS_SUCCESS_RSLT(atcmd_dispatch(g_lqLTEM.modemSettings->pdpNtwkConfig)))
            lqLOG_ERROR("DefaultNtwk Config Failed=%d\r", rslt);
    }
    atcmd_close();
}


const char * ntwk_getNetworkConfig()
{
    return g_lqLTEM.modemSettings->pdpNtwkConfig;
}


/**
 *   @brief Wait for a network operator name and network mode. Can be cancelled in threaded env via g_lqLTEM->cancellationRequest.
*/
ntwkOperator_t* ntwk_awaitOperator(uint16_t waitSec)
{
    ASSERT(g_lqLTEM.ntwkOperator != NULL);          // ASSERT g_lqLTEM.ntwkOperator has been initialized

    uint32_t startMillis, endMillis;
    startMillis = endMillis = lqMillis();
    uint32_t waitMs;
    if (waitSec > 300)
        waitMs = SEC_TO_MS(300);                    // max is 5 minutes
    else if (waitSec < 1)
        waitMs = 250;                               // min is 250ms
    else 
        waitMs = SEC_TO_MS(waitSec);

    do 
    {
        atcmd_configParser("+COPS: ", true, ",", 0, "\r\n", 0);
        if (IS_SUCCESS(atcmd_dispatch("AT+COPS?")))
        {
            // either there is a new operator or no operator; so clear out operator struct
            memset((void*)g_lqLTEM.ntwkOperator->packetNetworks, 0, g_lqLTEM.ntwkOperator->pdpCntxtCnt * sizeof(packetNetwork_t));
            memset((void*)g_lqLTEM.ntwkOperator, 0, sizeof(ntwkOperator_t));

            if (strlen(atcmd_getToken(1)))                                          // format presented, get operator information
            {
                char* operator = atcmd_getToken(2);                                 // operator name/ID presented
                if (strlen(operator))
                {

                    uint8_t toCopy = strlen(operator) - 2;
                    strncpy(g_lqLTEM.ntwkOperator->name, operator + 1, toCopy);
                    char * ntwkMode = atcmd_getToken(3);
                    if (strlen(ntwkMode))
                    {
                        if (ntwkMode[0] == '8')
                            strcpy(g_lqLTEM.ntwkOperator->iotMode, "M1");
                        else if (ntwkMode[0] == '9')
                            strcpy(g_lqLTEM.ntwkOperator->iotMode, "NB1");
                        else
                            strcpy(g_lqLTEM.ntwkOperator->iotMode, "GSM");
                    }
                }
            }
        }
        if (!STREMPTY(g_lqLTEM.ntwkOperator->name))
            break;

        lqDelay(1000);                                                               // this yields, allowing alternate execution
        endMillis = lqMillis();
    } while (endMillis - startMillis < waitMs || g_lqLTEM.cancellationRequest);     // timed out waiting OR global cancellation

    // got PROVIDER, get networks 

    /* NOTE: BGx will not return response for AT+CGPADDR *OVER THE SERIAL PORT*, unless it is suffixed with the contextID
        * This is one of a handfull of commands that exhibit this behavior; AT+CGPADDR works perfectly over the USB AT port.
    */
    if (!STREMPTY(g_lqLTEM.ntwkOperator->name))
    {
        char *pContinue;
        uint8_t ntwkIndx = 0;

        if (IS_SUCCESS(atcmd_dispatch("AT+CGPADDR")))
        {
            g_lqLTEM.ntwkOperator->packetNetworks[ntwkIndx].pdpContextId = strtol(atcmd_getToken(0), NULL, 10);
            g_lqLTEM.ntwkOperator->packetNetworks[ntwkIndx].pdpProtocol = pdpProtocol_IPV4;
            strcpy(g_lqLTEM.ntwkOperator->packetNetworks[ntwkIndx].protoName, "IP");
            strcpy(g_lqLTEM.ntwkOperator->packetNetworks[ntwkIndx].ipAddress, atcmd_getToken(1));
        }
        g_lqLTEM.ntwkOperator->pdpCntxtCnt = 1;                                 // future determination
    }
    return g_lqLTEM.ntwkOperator;
}


/**
 *	@brief Deactivate PDP Context.
 */
void ntwk_activatePdpContext(uint8_t cntxtId)
{
    atcmd_configParser("+QIACT: ", false, ",", 2, NULL, 0);
    atcmd_dispatch("AT+QIACT=%d", cntxtId);
}


/**
 *	@brief Deactivate PDP Context.
 */
void ntwk_deactivatePdpContext(uint8_t cntxtId)
{
    atcmd_configParser("+QIACT: ", false, ",", 2, NULL, 0);
    lqLOG_DBG(0,"(ntwk_deactivatePdpContext) parser configured\r\n");
    atcmd_dispatch("AT+QIDEACT=%d", cntxtId);
    lqLOG_DBG(0,"(ntwk_deactivatePdpContext) parser configured\r\n");
}


/**
 *	@brief Returns true if context is ready and updates LTEm internal network information for the context
 */
bool ntwk_getPdpContextState(uint8_t cntxtId)
{
    return true;
}


/**
 *   @brief Get current operator information. If not connected to a operator will be an empty operatorInfo struct.
 * 
 *   @return ntwkOperator_t pointer to stuct containing the network operator information.
*/
ntwkOperator_t * ntwk_getOperator()
{
    if (g_lqLTEM.ntwkOperator->name[0] == '\0')
        ntwk_awaitOperator(5);
    return g_lqLTEM.ntwkOperator;
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
packetNetwork_t * ntwk_getPacketNetwork(uint8_t pdpContextId)
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
    g_lqLTEM.statics.reportBffr[0] = '\0';                                          // scratch report buffer

    if (IS_SUCCESS(atcmd_dispatch("AT+QNWINFO")))
    {
        char * copyFrom = strstr(g_lqLTEM.atcmd->rawResponse, "+QNWINFO: ");
        if (copyFrom)                          // clean up extra content/EOL
        {
            copyFrom += sizeof("+QNWINFO: ") - 1;
            char* eol = memchr(copyFrom, '\r', atcmd__respBufferSz - 10);
            if (eol)
            {
                memcpy(g_lqLTEM.statics.reportBffr, copyFrom, eol - copyFrom);
            }
        }
    }
    return g_lqLTEM.statics.reportBffr;
}


/**
 * @brief Get current network registration status.
 * @return The current network operator registration status.
 */
resultCode_t ntwk_getRegistrationStatus()
{
    // TODO need a parser wrapper to grab <stat> (pos 2)
    return atcmd_dispatch("AT+CREG?");
}


/**
 * @brief Check network ready condition.
 */
bool ntwk_isReady()
{
    return g_lqLTEM.simReady &&
           g_lqLTEM.ntwkOperator->name[0] != '\0' > 0 &&
           g_lqLTEM.ntwkOperator->packetNetworks[0].ipAddress[0] != '0' &&
           ntwk_signalRaw() != 99;
}


/**
 * @brief 
 * 
 * @return true 
 * @return false 
 */
bool ntwk_validate()
{
    ntwk_awaitOperator(5);
    return ntwk_isReady() && ntwk_signalRaw() != 99;
}


/**
 *  @brief Get the signal strenght as raw value returned from BGx.
 */
uint8_t ntwk_signalRaw()
{
    uint8_t signal = 99;

    atcmd_ovrrdDCmpltTimeout(SEC_TO_MS(2));
    atcmd_configParser("+CSQ: ", true, ",", 0, "OK\r\n", 0);
    if (IS_SUCCESS(atcmd_dispatch("AT+CSQ")))
    {
        char* signalPtr = atcmd_getToken(0);
        lqLOG_VRBS("(ntwk_signalRaw) sgnl: %s\r\n", signalPtr);
        if (strlen(signalPtr))
        {
            signal = strtol(signalPtr, NULL, 10);
        }
        // char *term;
        // char *lastResponse = atcmd_getResponse();
        // term = strstr(atcmd_getResponse(), "+CSQ");
        // signal = strtol(term + 6, NULL, 10);
    }
    return signal;
}


/**
 *  @brief Get the signal strength reported by the LTEm device at a percent
 */
uint8_t ntwk_signalPercent()
{
    double csq;
    uint8_t signal = 0;
    const double csqFactor = 3.23;

    csq = (double)ntwk_signalRaw();
    signal = (csq == 99) ? 0 : (uint8_t)(csq * csqFactor);
    return signal;
}


/**
 *  @brief Get the signal strenght as RSSI (db).
 */
int16_t ntwk_signalRSSI()
{
    const int8_t rssiBase = -113;
    const int8_t rssiRange = 113 - 51;

    uint8_t signalPercent = ntwk_signalPercent();
    return (signalPercent == 0) ? rssiBase : (signalPercent * 0.01 * rssiRange) + rssiBase;
}


/**
 *  @brief Get the signal strength, as a bar count for visualizations, (like on a smartphone)
 * */
uint8_t ntwk_signalBars(uint8_t displayBarCount)
{
    const int8_t barOffset = 20; // adjust point for full-bar percent (20 = full bar count at 80%)

    uint8_t barSpan = 100 / displayBarCount;
    uint8_t signalPercent = MIN(mdmInfo_signalPercent() + barOffset, 100);
    return (uint8_t)(signalPercent / barSpan);
}


void ntwk_configSearchedBands(uint32_t bands)
{
    lteBands_t band;

    uint16_t s = LTE_B85;
}

// /**
//  * @brief Set network operator.
//  * @details The characteristics of the selected operator are accessible using the atcmd_getResponse() function.

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
void ntwkDiagnostics_getOperators(char * operatorList, uint16_t listSz)
{
    /* AT+COPS=? */
    ASSERT_W(false, "ntwkDiagnostics_getOperators() blocks and is SLOW!");

    atcmd_ovrrdDCmpltTimeout(SEC_TO_MS(180));
    if (IS_SUCCESS(atcmd_dispatch("AT+COPS=?")))
    {
        strncpy(operatorList, atcmd_getResponse() + 9, listSz - 1);
    }
}


#pragma endregion


/* private functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions


// static void S__clearOperatorInfo()
// {
//     memset((void*)g_lqLTEM.ntwkOperator->packetNetworks, 0, g_lqLTEM.ntwkOperator->pdpCntxtCnt * sizeof(packetNetwork_t));
//     memset((void*)g_lqLTEM.ntwkOperator, 0, sizeof(ntwkOperator_t));
// }


// /**
//  *   @brief Tests for the completion of a network APN context activate action.
//  *   @return standard action result integer (http result).
// */
// static cmdParseRslt_t S__contextStatusCompleteParser(void *atcmd, const char *response)
// {
//     DPRINT_V(0, "<S__contextStatusCompleteParser()> response=%s\r\n", response);
//     return atcmd_stdResponseParser("+QIACT: ", false, "", 2, 2, NULL, 0);
// }


// /**
//  *  @brief Scans a C-String (char array) for the next delimeted token and null terminates it.
//  * 
//  *  @param [in] source - Original char array to scan.
//  *  @param [in] delimeter - Character separating tokens (passed as integer value).
//  *  @param [out] tokenBuf - Pointer to where token should be copied to.
//  *  @param [in] tokenBufSz - Size of buffer to receive token.
//  * 
//  *  @return Pointer to the location in the source string immediately following the token.
// */
// static char *S__grabToken(char *source, int delimiter, char *tokenBuf, uint8_t tokenBufSz) 
// {
//     char *delimAt;
//     if (source == NULL)
//         return false;

//     delimAt = strchr(source, delimiter);
//     uint8_t tokenSz = delimAt - source;
//     if (tokenSz == 0)
//         return NULL;

//     memset(tokenBuf, 0, tokenBufSz);
//     memcpy(tokenBuf, source, MIN(tokenSz, tokenBufSz-1));
//     return delimAt + 1;
// }

#pragma endregion
