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


/** 
 *  \brief Typed numeric constants for network subsystem.
*/
enum ntwk__constants
{
    ntwk__pdpContextCnt = 2,
    ntwk__operatorNameSz = 20
};

/** 
 *  \brief Enum describing the mode the BGx module is using to look for available networks (carriers).
*/
typedef enum ntwkScanMode_tag
{
    ntwkScanMode_auto = 0U,         ///< BGx is considering either GSM or LTE carrier connections.
    ntwkScanMode_gsmonly = 1U,      ///< GSM only mode: BGx is filtering visible networks and only considering connections to GSM endpoints.
    ntwkScanMode_lteonly = 3U       ///< LTE only mode: BGx is filtering visible networks and only considering connections to LTE endpoints.
} ntwkScanMode_t;


/** 
 *  \brief Enum describing the available options for an IoT protocol when connecting to the network.
*/
typedef enum ntwkIotMode_tag
{
    ntwkIotMode_m1 = 0U,            ///< CAT-M1 only mode: BGx is filtering visible networks and only considering CAT-M1 connections.
    ntwkIotMode_nb1 = 1U,           ///< NB-IOT only mode: BGx is filtering visible networks and only considering NB-IOT connections.
    ntwkIotMode_m1nb1 = 2U          ///< The BGx will connect to either a CAT-M1 or NB-IOT network.
} ntwk_iotMode_t;


/** 
 *  \brief Enum of the two available PDP contexts for BGx provided by network carriers.
*/
typedef enum pdpCntxtIpType_tag
{
    pdpCntxtProtocolType_IPV4 = 1,              ///< IP v4, 32-bit address (ex: 192.168.37.52)
    pdpCntxtProtocolType_IPV6 = 2,              ///< IP v6, 128-bit address (ex: 2001:0db8:0000:0000:0000:8a2e:0370:7334)
    pdpCntxtProtocolType_IPV4V6 = 3,
    pdpCntxtProtocolType_PPP = 9
} pdpCntxtProtocolType_t;


typedef enum pdpCntxtAuthMethods_tag
{
    pdpCntxtAuthMethods_none = 0,
    pdpCntxtAuthMethods_pap = 1,
    pdpCntxtAuthMethods_chap = 2,
    pdpCntxtAuthMethods_papChap = 3
} pdpCntxtAuthMethods_t;


#define NTWKOPERATOR_OPERNAME_SZ 29
#define NTWKOPERATOR_NTWKMODE_SZ 11


/** 
 *  \brief Struct respresenting an ACTIVE network carrier/operator.
*/
typedef struct networkOperator_tag
{
	char operName[29];                  ///< Carrier name, some carriers may report as 6-digit numeric carrier ID.
	char ntwkMode[11];                  ///< Network carrier protocol mode: CATM-1 or NB-IOT for BGx.
} networkOperator_t;

#define PDPCONTEXT_APNNAME_SZ 21
#define PDPCONTEXT_IPADDRESS_SZ 16

/** 
 *  \brief Struct representing the state of active PDP contexts (aka: APN or data context).
*/
typedef struct pdpCntxt_tag
{
    uint8_t contextId;                  ///< context ID recognized by the carrier (valid are 1 to 16)
    pdpCntxtProtocolType_t protoType;   ///< IPv4 or IPv6
	char ipAddress[40];                 ///< The IP address obtained from the carrier for this context. The IP address of the modem.
} pdpCntxt_t;


/** 
 *  \brief Struct representing the full connectivity with a connected network carrier.
*/
typedef struct network_tag
{
    networkOperator_t *networkOperator;             ///< Network operator name and protocol
    pdpCntxt_t pdpCntxts[ntwk__pdpContextCnt];      ///< Collection of contexts with network carrier. This is typically only 1, but some carriers implement more (ex VZW).
} network_t;


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

//const char *ltem_ltemcVersion();

/**
 *	@brief Initialize the IP network contexts structure.
 */
void ntwk_create();


/**
 *  @brief Configure RAT searching sequence
 *  @details Example: scanSequence = "020301" represents: search LTE-M1, then LTE-NB1, then GSM
 *  @param scanSequence [in] - Character string specifying the RAT scanning order; 00=Automatic[LTE-M1|LTE-NB1|GSM],01=GSM,02=LTE-M1,03=LTE-NB1
*/
void ntwk_setNwScanSeq(const char *sequence);


/** 
 *  @brief Configure RAT(s) allowed to be searched
 *  @param scanMode [in] - Enum specifying what cell network to scan; 0=Automatic,1=GSM only,3=LTE only
*/
void ntwk_setNwScanMode(ntwkScanMode_t mode);


/** 
 *  @brief Configure the network category to be searched under LTE RAT.
 *  @param iotMode [in] - Enum specifying the LTE LPWAN protocol(s) to scan; 0=LTE M1,1=LTE NB1,2=LTE M1 and NB1
 */
void ntwk_setIotOpMode(ntwk_iotMode_t mode);


/**
 *   @brief Wait for a network operator name and network mode. Can be cancelled in threaded env via g_ltem->cancellationRequest.
 *   @param waitDurSeconds [in] Number of seconds to wait for a network. Supply 0 for no wait.
 *   @return Struct containing the network operator name (operName) and network mode (ntwkMode).
*/
networkOperator_t ntwk_awaitOperator(uint16_t waitDurSeconds);


/**
 *	@brief Activate PDP Context/APN.
 *  @param cntxtId [in] - The APN to operate on. Typically 0 or 1
 *  @param protoType [in] - The PDP protocol IPV4, IPV6, IPV4V6 (both).
 *  @param pApn [in] - The APN name if required by network carrier.
 */
bool ntwk_activatePdpContext(uint8_t cntxtId, pdpCntxtProtocolType_t protoType, const char *pApn);


/**
 *	@brief Activate PDP Context/APN requiring authentication.
 *  @param cntxtId [in] - The APN to operate on. Typically 0 or 1
 *  @param protoType [in] - The PDP protocol IPV4, IPV6, IPV4V6 (both).
 *  @param pApn [in] - The APN name if required by network carrier.
 *  @param pUserName [in] - String with user name
 *  @param pPW [in] - String with password
 *  @param authMethod [in] - Enum specifying the type of authentication expected by network
 */
bool ntwk_activatePdpContextWithAuth(uint8_t cntxtId, pdpCntxtProtocolType_t protoType, const char *pApn, const char *pUserName, const char *pPW, pdpCntxtAuthMethods_t authMethod);

/**
 *	@brief Deactivate PDP Context/APN.
 *  @param cntxtId [in] - The APN number to operate on.
 */
void ntwk_deactivatePdpContext(uint8_t contxtId);


/**
 *	@brief Get count of APN active data contexts from BGx.
 * 
 *  @return Count of active data contexts (BGx max is 3).
 */
uint8_t ntwk_fetchActivePdpCntxts();


/**
 *	@brief Get PDP Context/APN information
 *  @param cntxtId [in] - The PDP context (APN) to retreive.
 *  @return Pointer to PDP context info in active context table, NULL if not active
 */
pdpCntxt_t *ntwk_getPdpCntxtInfo(uint8_t contxtId);


/** 
 *  @brief Development/diagnostic function to retrieve visible operators from radio.
 *  @details This command can take MINUTES to respond! It is generally considered a command used solely for diagnostics.
 *  @param operatorList [in/out] - Pointer to char buffer to return operator list information retrieved from BGx.
 *  @param listSz [in] - Length of provided buffer.
 */
void ntwk_getOperators(char *operatorList, uint16_t listSz);


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__NETWORK_H__ */
