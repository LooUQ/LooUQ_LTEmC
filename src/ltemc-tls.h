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


#ifndef __LTEMC_TLS_H__
#define __LTEMC_TLS_H__

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


/* Release 3.0.3 Changes Below
 * ==============================================================================================*/

resultCode_t tls_configSni(dataCntxt_t cntx, bool setting);



#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__LTEMC_TLS_H__ */
