/** ***************************************************************************
  @file 
  @brief Modem protocol security (SSL/TLS) communication functions/services.

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


#ifndef __LTEMC_TLS_H__
#define __LTEMC_TLS_H__



/* SSL/TLS Module Type Definitions
 * ------------------------------------------------------------------------------------------------------------------------------*/

/** 
 *  @brief Enum of available SSL version options for an SSL connection
*/
typedef enum tlsVersion_tag
{
    tlsVersion_none = 255,
    tlsVersion_ssl30 = 0,
    tlsVersion_tls10 = 1,
    tlsVersion_tls11 = 2,
    tlsVersion_tls12 = 3,
    tlsVersion_any = 4,                                             ///< BGx default

    tlsVersion_default = 4
} tlsVersion_t;



/** 
 *  @brief Enum of the available cipher suites for TLS processing
*/
typedef enum tlsCipher_tag
{
    tlsCipher_rsaAes256CbcSha = 0X0035,
    tlsCipher_rsaAes128CbcSha = 0X002F,
    tlsCipher_rsaRc4128Sha = 0x0005,
    tlsCipher_rsaRc4128Md5 = 0x0004,
    tlsCipher_rsa3desEdeCbcSha = 0x000A,
    tlsCipher_rsaAes256CbcSha256 = 0x003D,

    tlsCipher_ecdheRsaRc4128Sha = 0xC011,
    tlsCipher_ecdheRsa3desEdeCbcSha = 0xC012,
    tlsCipher_ecdheRsaAes128CbcSha = 0xC013,
    tlsCipher_ecdheRsaAes256CbcSha = 0xC014,
    tlsCipher_ecdheRsaAes128CbcSha256 = 0xC027,
    tlsCipher_ecdheRsaAes256CbcSha384 = 0xC028,
    tlsCipher_ecdheRsaAes128GcmSha256 = 0xC02F,

    tlsCipher_any = 0xFFFF,                                         ///< BGx default

    tlsCipher_default = 0xFFFF
} tlsCipher_t;


/** 
 *  @brief Enum of the options for certificate expiration date/time checking
*/
typedef enum tlsCertExpiration_tag
{
    tlsCertExpiration_check = 0,
    tlsCertExpiration_ignore = 1,                                   ///< BGx default

    tlsCertExpiration_default = 1
} tlsCertExpiration_t;


/** 
 *  @brief Enum of the certification validation options
*/
typedef enum tlsSecurityLevel_tag
{
    tlsSecurityLevel_noAuthentication = 0,                          //< BGx default
    tlsSecurityLevel_serverAuthentication = 1,
    tlsSecurityLevel_serverClientAuthentication = 2,

    tlsSecurityLevel_default = 0
} tlsSecurityLevel_t;


typedef enum tlsEnableSni_tag
{
    tlsEnableSni_enabled = 1,
    tlsEnableSni_disabled = 0
} tlsEnableSni_t;


//#include "ltemc-types.h"

/**
 * @brief Context settings for TLS security
 */
typedef struct tlsCtrl_tag
{
    tlsVersion_t version;                                   ///< (SSL)TLS version
    tlsCipher_t cipher;                                     ///< Cipher suite available to TLS 
    tlsCertExpiration_t certExpirationCheck;                ///< Should dataCntxt check for server certificate expiration 
    tlsSecurityLevel_t securityLevel;                       ///< Designate what checks for SSL/TLS should complete to validate the server
    bool sniEnabled;                                        ///< TLS uses SNI
} tlsCtrl_t;





#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


// /** 
//  *  @brief Configure the TLS/SSL settings for a context
//  *  @details The TLS/SSL context is loosely associated with the protocol context. This package maintains a 1-to-1 map for consistency.

//  *  @param [in] contxt TLS/SSL context to configure
//  *  @param [in] version TLS/SSL version: 0=SSL-3.0, 1=TLS-1.0, 2=TLS-1.1, 3=TLS-1.2, 4=ALL
//  *  @param [in] cipher Cipher suite to use for processing of crypto
//  *  @param [in] certExpirationCheck Options for how the certificate's expiration information is processed
//  *  @param [in] securityLevel Authentication mode: 0=no auth, 1=server auth, 2=server/client auth
//  */
// bool tls_configure(uint8_t contxt, tlsVersion_t version, tlsCipher_t cipher, tlsCertExpiration_t certExpirationCheck, tlsSecurityLevel_t securityLevel);


/** 
 *  @brief Configure a TLS/SSL control block with current settings
 *  @details The TLS/SSL context is loosely associated with the protocol context. LTEmC maintains a 1-to-1 map for consistency.
 *  @param [in] tlsCtrl Pointer to TLS/SSL control block to be configured
 *  @param [in] version TLS/SSL version: 0=SSL-3.0, 1=TLS-1.0, 2=TLS-1.1, 3=TLS-1.2, 4=ALL
 *  @param [in] cipher Cipher suite to use for processing of crypto
 *  @param [in] certExpirationCheck Options for how the certificate's expiration information is processed
 *  @param [in] securityLevel Authentication mode: 0=no auth, 1=server auth, 2=server/client auth
 *  @param [in] sniEnabled Enable Server Name Indication in TLS handshake
 */
void tls_initControl(tlsCtrl_t* tlsCtrl, tlsVersion_t version, tlsCipher_t cipher, tlsCertExpiration_t certExpirationCheck, tlsSecurityLevel_t securityLevel, tlsEnableSni_t sniEnabled);


/** 
 *  @brief Configure the TLS/SSL settings for a context
 *  @details The TLS/SSL context is loosely associated with the protocol context. LTEmC maintains a 1-to-1 map for consistency.

 *  @param [in] sckt TLS/SSL context to configure
 *  @return TLS options structure with the settings currently applied to the specified context
 */
tlsCtrl_t tlsGetOptions(uint8_t sckt);


/* Maintenance of customer trustd root (TR) certificates requires the file_ module functionality (under developement Q3-2021)
 *  1) upload certficate to BGx file system 
 *  2) configure TR for context 
 */
// future certificate management support
// void tls_configureTRCertificate(context_t cntxt, const char *certificatePath);


/* Release 3.0.3 Changes Below
 * ==============================================================================================*/

/** 
 *  @brief Configure the SNI (Server Name Indication) option for TLS/SSL (default is no SNI).
 * 
 *  @param [in] dataCntxt TLS/SSL context to configure
 *  @param [in] enableSNI If true, enable SNI in TLS handshake
 *  @return TLS options structure with the settings currently applied to the specified context
 */
resultCode_t tls_enableSni(dataCntxt_t dataCntxt, tlsEnableSni_t enableSNI);


/** 
 *  @brief Apply settings from a TLS/SSL control to a data context.
 * 
 *  @param [in] contxt TLS/SSL context to configure
 *  @param [in] tlsCtrl Pointer to a TLS control structure
 *  @return True if application of settings is successful
 */
bool tls_applySettings(dataCntxt_t contxt, tlsCtrl_t* tlsCtrl);


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__LTEMC_TLS_H__ */
