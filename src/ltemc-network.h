/** ****************************************************************************
  \file 
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


#ifndef __NETWORK_H__
#define __NETWORK_H__

#include "ltemc.h"


/* ------------------------------------------------------------------------------------------------
 * Network constants, enums, and structures are declared in the LooUQ global lq-network.h header
 * --------------------------------------------------------------------------------------------- */
//#include <lq-network.h>


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
 *  @param [in] scanSequence Character string specifying the RAT scanning order; 00=Automatic[LTE-M1|LTE-NB1|GSM],01=GSM,02=LTE-M1,03=LTE-NB1
*/
void ntwk_setOperatorScanSeq(const char *sequence);


/** 
 *  @brief Configure RAT(s) allowed to be searched
 *  @param [in] scanMode Enum specifying what cell network to scan; 0=Automatic,1=GSM only,3=LTE only
*/
void ntwk_setOperatorScanMode(ntwkScanMode_t mode);


/** 
 *  @brief Configure prefered operator
 *  @param [in] opGsmIdentifier Enum specifying what cell network to scan; 0=Automatic,1=GSM only,3=LTE only
*/
resultCode_t ntwk_setOperator(const char* opGsmIdentifier, ntwkOpAccessTech_t opAccess);


/** 
 *  @brief Configure the network category to be searched under LTE RAT.
 *  @param [in] iotMode Enum specifying the LTE LPWAN protocol(s) to scan; 0=LTE M1,1=LTE NB1,2=LTE M1 and NB1
 */
void ntwk_setIotMode(ntwkIotMode_t mode);


/**
 *	@brief Build default data context configuration for modem to use on startup.
 *  @param [in] cntxtId The context ID to operate on. Typically 0 or 1
 *  @param [in] protoType The PDP protocol IPV4, IPV6, IPV4V6 (both).
 *  @param [in] apn The APN name if required by network carrier.
 */
void ntwk_setDefaultNetwork(uint8_t pdpContextId, pdpProtocol_t protoType, const char *apn);


/**
 *	@brief Apply previously defined PDP Context configuration to device.
 */
void ntwk_applyPdpNetworkConfig();


/**
 *	@brief Configure PDP Context requiring authentication.
 *  @details Only IPV4 is supported 
 *  @param [in] pdpContextId The PDP context to work with, typically 0 or 1.
 *  @param [in] apn The APN name if required by network carrier.
 *  @param [in] userName String with user name
 *  @param [in] pw String with password
 *  @param [in] authMethod Enum specifying the type of authentication expected by network
 */
resultCode_t ntwk_configPdpNetworkWithAuth(uint8_t pdpContextId, const char *apn, const char *pUserName, const char *pPW, pdpCntxtAuthMethods_t authMethod);


/**
 *   @brief Wait for a network operator name and network mode. Can be cancelled in threaded env via g_ltem->cancellationRequest.
 *   @param [in] waitSec Number of seconds to wait for a network. Supply 0 for no wait.
 *   @return Struct containing the network operator name (operName) and network mode (ntwkMode).
*/
operatorInfo_t *ntwk_awaitOperator(uint16_t waitSec);


/**
 *	@brief Set the default/data context number for network operator. Default is 1 if not overridden here.
 *  @param [in] defaultContext The data context to operate on. Typically 0 or 1, up to 15
 */
void ntwk_setOperatorDefaultContext(uint8_t defaultContext);


/**
 *	@brief Deactivate PDP Context/APN.
 *  @param [in] contextId The APN number to operate on.
 */
void ntwk_deactivateNetwork(uint8_t contextId);


/**
 *   @brief Get current provider information. If not connected to a provider will be an empty providerInfo struct
 *   @return Struct containing the network operator name (operName) and network mode (ntwkMode).
*/
operatorInfo_t *ntwk_getOperatorInfo();


/**
 *	@brief Get count of APN active data contexts from BGx.
 *  @return Count of active data contexts (BGx max is 3).
 */
uint8_t ntwk_getActiveNetworkCount();


/**
 *	@brief Get PDP network information
 *  @param [in] cntxtId The PDP context ID/index to retreive.
 *  @return Pointer to network (PDP) info in active network table, NULL if context ID not active
 */
networkInfo_t *ntwk_getNetworkInfo(uint8_t contxtId);


/**
 *	@brief Get current network registration status.
 *  @return The current network operator registration status.
 */
uint8_t ntwk_getRegistrationStatus();


// /**
//  *  @brief Set network operator.
//  *  @details The characteristics of the selected operator are accessible using the atcmd_getResponse() function.

//  *  @param [in] mode Action to be performed, set/clear/set default.
//  *  @param [in] format The form for the ntwkOperator parameter value: long, short, numeric.
//  *  @param [in] ntwkOperator Operator to select, presented in the "format". Not all modes require/act on this parameter.
//  *  @return Current operator selection mode. Note:
//  */
// uint8_t ntwk_setOperator(uint8_t mode, uint8_t format, const char* ntwkOperator);


/**
 *	@brief Check network ready condition.
 *  @return True, if network is fully established and ready for data transmission.
 */
bool ntwk_isReady();


/** 
 *  @brief Development/diagnostic function to retrieve visible providers from radio.
 *  @warning This command can take MINUTES to respond! It is generally considered a command used solely for diagnostics.
 * 
 *  @param [out] operatorList  Pointer to char buffer to return operator list information retrieved from BGx.
 *  @param [in] listSz Length of provided buffer.
 */
void ntwkDiagnostics_getProviders(char *operatorList, uint16_t listSz);


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__NETWORK_H__ */
