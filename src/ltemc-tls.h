/******************************************************************************
 *  \file ltemc-tls.h
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2021 LooUQ Incorporated.
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
 * TLS/SSL Constants for BGx Devices
 *****************************************************************************/

#ifndef __LTEMC_TLS_H__
#define __LTEMC_TLS_H__

#include "ltemc-streams.h"
#include <lq-diagnostics.h>

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
    tlsVersion_any = 4,                                 // BGx default

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

    tlsCipher_any = 0xFFFF,                             // BGx default

    tlsCipher_default = 0xFFFF
} tlsCipher_t;


/** 
 *  @brief Enum of the options for certificate expiration date/time checking
*/
typedef enum tlsCertExpiration_tag
{
    tlsCertExpiration_check = 0,
    tlsCertExpiration_ignore = 1,                 // BGx default

    tlsCertExpiration_default = 1
} tlsCertExpiration_t;


/** 
 *  @brief Enum of the certification validation options
*/
typedef enum tlsSecurityLevel_tag
{
    tlsSecurityLevel_noAuthentication = 0,              // BGx default
    tlsSecurityLevel_serverAuthentication = 1,
    tlsSecurityLevel_serverClientAuthentication = 2,

    tlsSecurityLevel_default = 0
} tlsSecurityLevel_t;


/** 
 *  @brief Return struct for get TLC information
*/
typedef struct tlsOptions_tag
{
    tlsVersion_t version;
    tlsCipher_t cipher;
    tlsCertExpiration_t certExpCheck;
    tlsSecurityLevel_t securityLevel;
    char trCertPath[80];
} tlsOptions_t;


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus



/** 
 *  @brief Configure the TLS/SSL settings for a context
 *  @details The TLS/SSL context is loosely associated with the protocol context. This package maintains a 1-to-1 map for consistency.
 *  @param contxt [in] TLS/SSL context to configure
 *  @param version [in] TLS/SSL version: 0=SSL-3.0, 1=TLS-1.0, 2=TLS-1.1, 3=TLS-1.2, 4=ALL
 *  @param cipherSuite [in] Cipher suite to use for processing of crypto
 *  @param certExpirationCheck [in] Options for how the certificate's expiration information is processed
 *  @param securityLevel [in] Authentication mode: 0=no auth, 1=server auth, 2=server/client auth
 */
bool tls_configure(socket_t sckt, tlsVersion_t version, tlsCipher_t cipherSuite, tlsCertExpiration_t certExpirationCheck, tlsSecurityLevel_t securityLevel);


/** 
 *  @brief Configure the TLS/SSL settings for a context
 *  @details The TLS/SSL context is loosely associated with the protocol context. This package maintains a 1-to-1 map for consistency.
 *  @param contxt [in] TLS/SSL context to configure
 *  @return TLS options structure with the settings currently applied to the specified context
 */
tlsOptions_t tlsGetOptions(socket_t sckt);


/* Maintenance of customer trustd root (TR) certificates requires the file_ module functionality (under developement Q3-2021)
 *  1) upload certficate to BGx file system 
 *  2) configure TR for context 
 */
// future certificate management support
// void tls_configureTRCertificate(context_t cntxt, const char *certificatePath);


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__LTEMC_TLS_H__ */
