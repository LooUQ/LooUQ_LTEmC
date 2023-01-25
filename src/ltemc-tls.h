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

//#include <lq-diagnostics.h>
#include "ltemc-types.h"


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
bool tls_configure(uint8_t sckt, tlsVersion_t version, tlsCipher_t cipherSuite, tlsCertExpiration_t certExpirationCheck, tlsSecurityLevel_t securityLevel);


/** 
 *  @brief Configure the TLS/SSL settings for a context
 *  @details The TLS/SSL context is loosely associated with the protocol context. This package maintains a 1-to-1 map for consistency.
 *  @param contxt [in] TLS/SSL context to configure
 *  @return TLS options structure with the settings currently applied to the specified context
 */
tlsOptions_t tlsGetOptions(uint8_t sckt);


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
