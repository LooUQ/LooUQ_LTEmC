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

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


#include "ltemc-types.h"

typedef struct tlsCtrl_tag
{
    tlsVersion_t version;
    tlsCipher_t cipherSuite;
    tlsCertExpiration_t certExpirationCheck;
    tlsSecurityLevel_t securityLevel;
    bool sniEnabled;
} tlsCtrl_t;


/** 
 *  @brief Configure the TLS/SSL settings for a context
 *  @details The TLS/SSL context is loosely associated with the protocol context. This package maintains a 1-to-1 map for consistency.
 *  @param [in] contxt TLS/SSL context to configure
 *  @param [in] version TLS/SSL version: 0=SSL-3.0, 1=TLS-1.0, 2=TLS-1.1, 3=TLS-1.2, 4=ALL
 *  @param [in] cipherSuite Cipher suite to use for processing of crypto
 *  @param [in] certExpirationCheck Options for how the certificate's expiration information is processed
 *  @param [in] securityLevel Authentication mode: 0=no auth, 1=server auth, 2=server/client auth
 */
bool tls_configure(uint8_t sckt, tlsVersion_t version, tlsCipher_t cipherSuite, tlsCertExpiration_t certExpirationCheck, tlsSecurityLevel_t securityLevel);


/** 
 *  @brief Configure a TLS/SSL control block with current settings
 *  @details The TLS/SSL context is loosely associated with the protocol context. LTEmC maintains a 1-to-1 map for consistency.
 *  @param [in] tlsCtrl Pointer to TLS/SSL control block to be configured
 *  @param [in] version TLS/SSL version: 0=SSL-3.0, 1=TLS-1.0, 2=TLS-1.1, 3=TLS-1.2, 4=ALL
 *  @param [in] cipherSuite Cipher suite to use for processing of crypto
 *  @param [in] certExpirationCheck Options for how the certificate's expiration information is processed
 *  @param [in] securityLevel Authentication mode: 0=no auth, 1=server auth, 2=server/client auth
 *  @param [in] sniEnabled Enable Server Name Indication in TLS handshake
 */
void tls_initControl(tlsCtrl_t* tlsCtrl, tlsVersion_t version, tlsCipher_t cipherSuite, tlsCertExpiration_t certExpirationCheck, tlsSecurityLevel_t securityLevel, bool sniEnabled);


/* Maintenance of customer trustd root (TR) certificates requires the file_ module functionality (under developement Q3-2021)
 *  1) upload certficate to BGx file system 
 *  2) configure TR for context 
 */
// future certificate management support
// void tls_configureTRCertificate(context_t cntxt, const char *certificatePath);



/** 
 *  @brief Configure the SNI (Server Name Indication) option for TLS/SSL (default is no SNI).
 *  @param [in] contxt TLS/SSL context to configure
 *  @param [in] turnSniOn If true, enable SNI in TLS handshake
 *  @return TLS options structure with the settings currently applied to the specified context
 */
resultCode_t tls_enableSni(dataCntxt_t cntx, bool enableSNI);


/** 
 *  @brief Apply settings from a TLS/SSL control to a data context.
 *  @param [in] contxt TLS/SSL context to configure
 *  @param [in] tlsCtrl Pointer to a TLS control structure
 *  @return True if application of settings is successful
 */
bool tls_applySettings(dataCntxt_t contxt, tlsCtrl_t* tlsCtrl);


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__LTEMC_TLS_H__ */
