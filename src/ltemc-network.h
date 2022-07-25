/******************************************************************************
 *  \file ltemc-network.h
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

#ifndef __NETWORK_H__
#define __NETWORK_H__

#include "ltemc.h"

/* ------------------------------------------------------------------------------------------------
 * Network constants, enums, and structures are declared in the LooUQ global lq-network.h header
 * --------------------------------------------------------------------------------------------- */
#include <lq-network.h>


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

//const char *ltem_ltemcVersion();

/**
 *	\brief Initialize the IP network contexts structure.
 */
void ntwk_create();


/**
 *  \brief Configure RAT searching sequence
 *  \details Example: scanSequence = "020301" represents: search LTE-M1, then LTE-NB1, then GSM
 *  \param [in] scanSequence Character string specifying the RAT scanning order; 00=Automatic[LTE-M1|LTE-NB1|GSM],01=GSM,02=LTE-M1,03=LTE-NB1
*/
void ntwk_setProviderScanSeq(const char *sequence);


/** 
 *  \brief Configure RAT(s) allowed to be searched
 *  \param [in] scanMode Enum specifying what cell network to scan; 0=Automatic,1=GSM only,3=LTE only
*/
void ntwk_setProviderScanMode(ntwkScanMode_t mode);


/** 
 *  \brief Configure the network category to be searched under LTE RAT.
 *  \param [in] iotMode Enum specifying the LTE LPWAN protocol(s) to scan; 0=LTE M1,1=LTE NB1,2=LTE M1 and NB1
 */
void ntwk_setIotMode(ntwk_iotMode_t mode);


/**
 *   \brief Wait for a network operator name and network mode. Can be cancelled in threaded env via g_ltem->cancellationRequest.
 *   \param [in] waitDurSeconds Number of seconds to wait for a network. Supply 0 for no wait.
 *   \return Struct containing the network operator name (operName) and network mode (ntwkMode).
*/
providerInfo_t *ntwk_awaitProvider(uint16_t waitDurSeconds);


/**
 *	\brief Set the default/data context number for provider. Default is 1 if not overridden here.
 *  \param [in] defaultContext The data context to operate on. Typically 0 or 1, up to 15
 */
void ntwk_setProviderDefaultContext(uint8_t defaultContext);


/**
 *	\brief Activate PDP Context/APN.
 *  \param [in] cntxtId The context ID to operate on. Typically 0 or 1
 *  \param [in] protoType The PDP protocol IPV4, IPV6, IPV4V6 (both).
 *  \param [in] apn The APN name if required by network carrier.
 */
networkInfo_t *ntwk_activateNetwork(uint8_t cntxtId, networkPDPType_t protoType, const char *apn);


/**
 *	\brief Activate PDP Context/APN requiring authentication.
 *  \param [in] cntxtId The APN to operate on. Typically 0 or 1
 *  \param [in] protoType The PDP protocol IPV4, IPV6, IPV4V6 (both).
 *  \param [in] apn The APN name if required by network carrier.
 *  \param [in] userName String with user name
 *  \param [in] pw String with password
 *  \param [in] authMethod Enum specifying the type of authentication expected by network
 */
networkInfo_t *ntwk_activateNetworkWithAuth(uint8_t cntxtId, networkPDPType_t protoType, const char *apn, const char *pUserName, const char *pPW, pdpCntxtAuthMethods_t authMethod);

/**
 *	\brief Deactivate PDP Context/APN.
 *  \param [in] cntxtId The APN number to operate on.
 */
void ntwk_deactivateNetwork(uint8_t contxtId);


/**
 *	\brief Get count of APN active data contexts from BGx.
 *  \return Count of active data contexts (BGx max is 3).
 */
uint8_t ntwk_getActiveNetworkCount();


/**
 *	\brief Get PDP network information
 *  \param [in] cntxtId The PDP context ID/index to retreive.
 *  \return Pointer to network (PDP) info in active network table, NULL if context ID not active
 */
networkInfo_t *ntwk_getNetworkInfo(uint8_t contxtId);


/** 
 *  \brief Development/diagnostic function to retrieve visible providers from radio.
 *  \details This command can take MINUTES to respond! It is generally considered a command used solely for diagnostics.
 *  \param [in,out] operatorList - Pointer to char buffer to return operator list information retrieved from BGx.
 *  \param [in] listSz - Length of provided buffer.
 */
void ntwk_getProviders(char *operatorList, uint16_t listSz);


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__NETWORK_H__ */
