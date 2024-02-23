/** ***************************************************************************
  @file ltemc-tls.c
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


#include <lq-embed.h>
#define lqLOG_LEVEL lqLOGLEVEL_OFF                                  ///< Logging detail level for this source file.
//#define DISABLE_ASSERTS                                           ///< ASSERT/ASSERT_W enabled by default, can be disabled 
#define LQ_SRCFILE "TLS"                                            ///< create SRCFILE (3 char) MACRO for lq-diagnostics lqASSERT

#include "ltemc-internal.h"
#include "ltemc-tls.h"


// /**
//  * @brief 
//  */
// bool tls_configure(uint8_t dataCntxt, tlsVersion_t version, tlsCipher_t cipherSuite, tlsCertExpiration_t certExpirationCheck, tlsSecurityLevel_t securityLevel)
// {
//     RSLT;
//     if (IS_NOTSUCCESS_RSLT(atcmd_dispatch("AT+QSSLCFG=\"sslversion\",%d,%d", dataCntxt, version)))                  // set SSL/TLS version
//     {
//         return false;
//     }
//     if (IS_NOTSUCCESS_RSLT(atcmd_dispatch("AT+QSSLCFG=\"ciphersuite\",%d,0X%X", dataCntxt, cipherSuite)))           // set cipher suite
//     {
//         return false;
//     }
//     if (IS_NOTSUCCESS_RSLT(atcmd_dispatch("AT+QSSLCFG=\"ignorelocaltime\",%d,%d", dataCntxt, certExpirationCheck))) // set certificate expiration check
//     {
//         return false;
//     }
//     if (IS_NOTSUCCESS_RSLT(atcmd_dispatch("AT+QSSLCFG=\"seclevel\",%d,%d", dataCntxt, securityLevel)))              // set security level, aka what is checked
//     {
//         return false;
//     }

//     return true;
// }


/** 
 *  @brief Configure a TLS/SSL control block with current settings
 */
void tls_initControl(tlsCtrl_t* tlsCtrl, tlsVersion_t version, tlsCipher_t cipher, tlsCertExpiration_t certExpirationCheck, tlsSecurityLevel_t securityLevel, bool sniEnabled)
{
    memset(tlsCtrl, 0, sizeof(tlsCtrl_t));
   
    tlsCtrl->version = version;
    tlsCtrl->cipher = cipher;
    tlsCtrl->certExpirationCheck = certExpirationCheck;
    tlsCtrl->securityLevel = securityLevel;
    tlsCtrl->sniEnabled = sniEnabled;
}


// tlsOptions_t tlsGetOptions(uint8_t dataCntxt)
// {
//     tlsOptions_t tlsOptions = {0};
//     RSLT;
//     if (IS_NOTSUCCESS_RSLT(atcmd_dispatch("AT+QSSLCFG=\"sslversion\",%d", (uint8_t)dataCntxt)))     // get SSL\TLS version
//     {
//         lqLOG_WARN("(tlsGetOptions) options: %s", atcmd_getLastResponse());
//     }
//     return tlsOptions;
// }

/**
 * @brief Enable SNI verification for this data context
 */
resultCode_t tls_enableSni(dataCntxt_t dataCntxt, bool enableSNI)
{
    RSLT;
    if (IS_NOTSUCCESS_RSLT(atcmd_dispatch("AT+QSSLCFG=\"sni\",%d,%d", dataCntxt, enableSNI)))       // set SNI for TLS
    {   
        lqLOG_WARN("(tls_enableSni) options: %s", atcmd_getLastResponse());
    }
    return rslt;
}


/** 
 *  @brief Apply settings from a TLS/SSL control to a data context.
 */
bool tls_applySettings(dataCntxt_t dataCntxt, tlsCtrl_t* tlsCtrl)
{
    RSLT;
    if (IS_NOTSUCCESS_RSLT(atcmd_dispatch("AT+QSSLCFG=\"sslversion\",%d,%d", dataCntxt, tlsCtrl->version)))                     // set SSL/TLS version
        return false;

    if (IS_NOTSUCCESS_RSLT(atcmd_dispatch("AT+QSSLCFG=\"ciphersuite\",%d,0X%X", dataCntxt, tlsCtrl->cipher)))                   // set cipher suite
        return false;

    if (IS_NOTSUCCESS_RSLT(atcmd_dispatch("AT+QSSLCFG=\"ignorelocaltime\",%d,%d", dataCntxt, tlsCtrl->certExpirationCheck)))    // set certificate expiration check
        return false;

    if (IS_NOTSUCCESS_RSLT(atcmd_dispatch("AT+QSSLCFG=\"seclevel\",%d,%d", dataCntxt, tlsCtrl->securityLevel)))                 // set security level, aka what is checked
        return false;

    if (IS_NOTSUCCESS_RSLT(atcmd_dispatch("AT+QSSLCFG=\"sni\",%d,%d", dataCntxt, tlsCtrl->sniEnabled)))                         // set security level, aka what is checked
        return false;

    return true;
}
