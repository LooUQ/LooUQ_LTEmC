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
 *	@brief Configure PDP Context.
 *  @param [in] cntxtId The context ID to operate on. Typically 0 or 1
 *  @param [in] protoType The PDP protocol IPV4, IPV6, IPV4V6 (both).
 *  @param [in] apn The APN name if required by network carrier.
 *  
 */
void *ntwk_configureNetwork(uint8_t pdpContextId, const char *protoType, const char *apn);


/* Deferred Implementation: Cannot find a network provider requiring authentication and Quectel doesn't support beyond IPV4
*/
// /**
//  *	@brief Configure PDP Context requiring authentication.
//  *  @param [in] pdpContextId The PDP context to work with, typically 0 or 1.
//  *  @param [in] protoType The PDP protocol IPV4, IPV6, IPV4V6 (both).
//  *  @param [in] apn The APN name if required by network carrier.
//  *  @param [in] userName String with user name
//  *  @param [in] pw String with password
//  *  @param [in] authMethod Enum specifying the type of authentication expected by network
//  */
// networkInfo_t *ntwk_configureNetworkWithAuth(uint8_t pdpContextId, pdpProtocolType_t protoType, const char *apn, const char *pUserName, const char *pPW, pdpCntxtAuthMethods_t authMethod);


/**
 *   @brief Wait for a network operator name and network mode. Can be cancelled in threaded env via g_ltem->cancellationRequest.
 *   @param [in] waitSec Number of seconds to wait for a network. Supply 0 for no wait.
 *   @return Struct containing the network operator name (operName) and network mode (ntwkMode).
*/
providerInfo_t *ntwk_awaitProvider(uint16_t waitSec);


// /**
//  *	@brief Set the default/data context number for provider. Default is 1 if not overridden here.
//  *  @param [in] defaultContext The data context to operate on. Typically 0 or 1, up to 15
//  */
// void ntwk_setProviderDefaultContext(uint8_t defaultContext);


/**
 *	@brief Deactivate PDP Context/APN.
 *  @param [in] contextId The APN number to operate on.
 */
void ntwk_deactivateNetwork(uint8_t contextId);




/**
 *   @brief Get current provider information. If not connected to a provider will be an empty providerInfo struct
 *   @return Struct containing the network operator name (operName) and network mode (ntwkMode).
*/
providerInfo_t *ntwk_getProviderInfo();


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


/**
 *	@brief Set network operator.
 *  @details The characteristics of the selected operator are accessible using the atcmd_getResponse() function.

 *  @param [in] mode Action to be performed, set/clear/set default.
 *  @param [in] format The form for the ntwkOperator parameter value: long, short, numeric.
 *  @param [in] ntwkOperator Operator to select, presented in the "format". Not all modes require/act on this parameter.
 *  @return Current operator selection mode. Note:
 */
uint8_t ntwk_setOperator(uint8_t mode, uint8_t format, const char* ntwkOperator);


/** 
 *  @brief Development/diagnostic function to retrieve visible providers from radio.
 *  @warning This command can take MINUTES to respond! It is generally considered a command used solely for diagnostics.
 * 
 *  @param [out] operatorList  Pointer to char buffer to return operator list information retrieved from BGx.
 *  @param [in] listSz Length of provided buffer.
 */
void ntwkDIAG_getProviders(char *operatorList, uint16_t listSz);


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__NETWORK_H__ */
