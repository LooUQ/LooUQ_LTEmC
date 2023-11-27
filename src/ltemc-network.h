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


#ifndef __NETWORK_H__
#define __NETWORK_H__

// #include "ltemc.h"


/** 
 *  \brief Typed numeric constants for network subsystem.
*/
enum ntwk__constants
{
    ntwk__pdpContextCnt = 4,            // varies by carrier: Verizon=2, (Aeris)ATT=3
    ntwk__operatorNameSz = 20,
    ntwk__iotModeNameSz = 11,
    ntwk__pdpProtoSz = 7,
    ntwk__ipAddressSz = 40,
    ntwk__pdpNtwkConfigSz = 60,
    ntwk__scanSeqSz = 12
};


/* Modem/Provider/Network Type Definitions
 * ------------------------------------------------------------------------------------------------------------------------------*/

#define NTWK_PROVIDER_RAT_AUTO "00"     // M1 (eMTC) >> NB-IoT >> GSM
#define NTWK_PROVIDER_RAT_GSM "01"
#define NTWK_PROVIDER_RAT_M1 "02"
#define NTWK_PROVIDER_RAT_NB "03"


/** 
 *  \brief Enum describing the mode the BGx module is using to look for available networks (carriers).
*/
typedef enum ntwkScanMode_tag
{
    ntwkScanMode_auto = 0U,             // BGx is considering either GSM or LTE carrier connections.
    ntwkScanMode_gsmonly = 1U,          // GSM only mode: BGx is filtering visible networks and only considering connections to GSM endpoints.
    ntwkScanMode_lteonly = 3U           // LTE only mode: BGx is filtering visible networks and only considering connections to LTE endpoints.
} ntwkScanMode_t;


/** 
 *  \brief Enum describing the available options for an IoT protocol when connecting to the network.
*/
typedef enum ntwkIotMode_tag
{
    ntwkIotMode_M1 = 0U,                // CAT-M1 only mode: BGx is filtering visible networks and only considering CAT-M1 connections.
    ntwkIotMode_NB = 1U,                // NB-IOT only mode: BGx is filtering visible networks and only considering NB-IOT connections.
    ntwkIotMode_M1NB = 2U               // The BGx will connect to either a CAT-M1 or NB-IOT network.
} ntwkIotMode_t;


/**
 * @brief Protocol types for network PDP (packet data protocol) context
 */
typedef enum pdpProtocol_tag
{
    pdpProtocol_notSet = 0,
    pdpProtocol_IPV4 = 1,
    pdpProtocol_IPV6 = 2,
    pdpProtocol_IPV4V6 = 3,
    pdpProtocol_PPP = 99                    // not supported by LTEmC
} pdpProtocol_t;


/**
 * @brief Authentication methods for packet network (PDP context) where required by network operator
 */
typedef enum pdpCntxtAuthMethods_tag
{
    pdpCntxtAuthMethods_none = 0,
    pdpCntxtAuthMethods_pap = 1,
    pdpCntxtAuthMethods_chap = 2,
    pdpCntxtAuthMethods_papChap = 3
} pdpCntxtAuthMethods_t;


/** 
 *  \brief Struct holding cellular and radio settings.
*/
typedef struct ntwkSettings_tag
{
    char scanSequence[PSZ(ntwk__scanSeqSz)];
    ntwkScanMode_t scanMode;
    ntwkIotMode_t iotMode;
    char pdpNtwkConfig[ntwk__pdpNtwkConfigSz];  // Invoke ready default context config
} ntwkSettings_t;


/** 
 *  \brief Struct representing the state of active PDP contexts (aka: APN or data context).
*/
typedef struct packetNetwork_tag
{
    bool isActive;
    uint8_t pdpContextId;                           // context ID recognized by the carrier (valid are 1 to 16)
    pdpProtocol_t pdpProtocol;                      // IPv4, IPv6, etc.
	char ipAddress[ntwk__ipAddressSz];              // The IP address obtained from the carrier for this context. The IP address of the modem.
} packetNetwork_t;


/** 
 *  \brief Struct respresenting an ACTIVE network carrier/operator.
*/
typedef struct ntwkOperator_tag
{
	char name[PSZ(ntwk__operatorNameSz)];               // Provider name, some carriers may report as 6-digit numeric carrier ID.
	char iotMode[PSZ(ntwk__iotModeNameSz)];             // Network carrier protocol mode: CATM-1 or NB-IOT for BGx.
    uint8_t defaultContext;
    uint8_t pdpCntxtCnt;                                // The number of PDP contexts available
    packetNetwork_t packetNetworks[ntwk__pdpContextCnt];  // Collection of packet network with cell operator. This is typically only 1, but some carriers implement more (ex VZW).
} ntwkOperator_t;





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
 * @brief Configure RAT searching sequence
 * @details Example: scanSequence = "020301" represents: search LTE-M1, then LTE-NB1, then GSM
 * @param [in] scanSequence Character string specifying the RAT scanning order; 00=Automatic[LTE-M1|LTE-NB1|GSM],01=GSM,02=LTE-M1,03=LTE-NB1
*/
void ntwk_setOperatorScanSeq(const char *sequence);


/** 
 * @brief Configure RAT(s) allowed to be searched
 * @param [in] scanMode Enum specifying what cell network to scan; 0=Automatic,1=GSM only,3=LTE only
*/
void ntwk_setOperatorScanMode(ntwkScanMode_t mode);


/** 
 * @brief Configure the network category to be searched under LTE RAT.
 * @param [in] iotMode Enum specifying the LTE LPWAN protocol(s) to scan; 0=LTE M1,1=LTE NB1,2=LTE M1 and NB1
 */
void ntwk_setIotMode(ntwkIotMode_t mode);


/**
 *	@brief Build default data context configuration for modem to use on startup.
 * @param [in] cntxtId The context ID to operate on. Typically 0 or 1
 * @param [in] protoType The PDP protocol IPV4, IPV6, IPV4V6 (both).
 * @param [in] apn The APN name if required by network carrier.
 */
resultCode_t ntwk_setDefaultNetwork(uint8_t pdpContextId, pdpProtocol_t protoType, const char *apn);


/**
 *	@brief Configure PDP Context.
 * @param [in] cntxtId The context ID to operate on. Typically 0 or 1
 * @param [in] protoType The PDP protocol IPV4, IPV6, IPV4V6 (both).
 * @param [in] apn The APN name if required by network carrier.
 */
resultCode_t ntwk_configPdpNetwork(uint8_t pdpContextId, pdpProtocol_t protoType, const char *apn);


/**
 *	@brief Configure PDP Context requiring authentication.
 * @details Only IPV4 is supported 
 * @param [in] pdpContextId The PDP context to work with, typically 0 or 1.
 * @param [in] apn The APN name if required by network carrier.
 * @param [in] userName String with user name
 * @param [in] pw String with password
 * @param [in] authMethod Enum specifying the type of authentication expected by network
 */
resultCode_t ntwk_configPdpNetworkWithAuth(uint8_t pdpContextId, const char *apn, const char *pUserName, const char *pPW, pdpCntxtAuthMethods_t authMethod);


/**
 * @brief Wait for a network operator name and network mode. Can be cancelled in threaded env via g_ltem->cancellationRequest.
 * @param [in] waitSec Number of seconds to wait for a network. Supply 0 for no wait.
 * @return Struct containing the network operator name (operName) and network mode (ntwkMode).
*/
ntwkOperator_t *ntwk_awaitOperator(uint16_t waitSec);


/**
 *	@brief Set the default/data context number for provider. Default is 1 if not overridden here.
 * @param [in] defaultContext The data context to operate on. Typically 0 or 1, up to 15
 */
void ntwk_setOperatorDefaultContext(uint8_t defaultContext);


/**
 * @brief Activate a PDP data context for TCP/IP communications.
 * @note The BG9x supports a maximum of 3 contexts, BG7x supports a maximum of 2. Most network operators support 1 or 2 (VPN).
 * @param [in] cntxtId The PDP context ID to activate
 */
void ntwk_activatePdpContext(uint8_t cntxtId);


/**
 * @brief Deactivate a PDP (TCP/IP data communications) context.
 * @param [in] cntxtId The PDP context ID to deactivate
 */
void ntwk_deactivatePdpContext(uint8_t cntxtId);


/**
 * @brief Returns true if context is ready and updates LTEm internal network information for the context
 * @param [in] cntxtId The PDP context ID to deactivate
 * @return True if the context is active
 */
bool ntwk_getPdpContextState(uint8_t cntxtId);


/**
 * @brief Get current provider information. If not connected to a provider will be an empty providerInfo struct
 * @return Struct containing the network operator name (operName) and network mode (ntwkMode).
*/
ntwkOperator_t *ntwk_getOperatorInfo();


/**
 * @brief Get count of APN active data contexts from BGx.
 * @return Count of active data contexts (BGx max is 3).
 */
uint8_t ntwk_getActiveNetworkCount();


/**
 * @brief Get PDP network information
 * @param [in] cntxtId The PDP context ID/index to retreive.
 * @return Pointer to network (PDP) info in active network table, NULL if context ID not active
 */
packetNetwork_t* ntwk_getPacketNetwork(uint8_t pdpContextId);


/**
 * @brief Get information about the active operator network
 * @return Char array pointer describing operator network characteristics
 */
const char* ntwk_getNetworkInfo();


/**
 * @brief Get current network registration status.
 * @return The current network operator registration status.
 */
resultCode_t ntwk_getRegistrationStatus();


// /**
//  * @brief Set network operator.
//  * @details The characteristics of the selected operator are accessible using the atcmd_getResponse() function.

//  * @param [in] mode Action to be performed, set/clear/set default.
//  * @param [in] format The form for the ntwkOperator parameter value: long, short, numeric.
//  * @param [in] ntwkOperator Operator to select, presented in the "format". Not all modes require/act on this parameter.
//  * @return Current operator selection mode. Note:
//  */
// uint8_t ntwk_setOperator(uint8_t mode, uint8_t format, const char* ntwkOperator);


/**
 * @brief Check network ready condition.
 * @param [in] refresh If true, performs a query with module for current state.
 * @return True, if network is fully established and ready for data transmission.
 */
bool ntwk_isReady(bool refresh);


/** 
 * @brief Development/diagnostic function to retrieve visible providers from radio.
 * @warning This command can take MINUTES to respond! It is generally considered a command used solely for diagnostics.
 * @return Char array-ptr with list of operators (raw format)
 */
const char* NTWKDIAGNOSTICS_getOperators();


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__NETWORK_H__ */
