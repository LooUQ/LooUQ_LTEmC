/** ****************************************************************************
  \file 
  \brief Public API for TLS protocol (TLS/SSL) support
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


#define _DEBUG 0                                // set to non-zero value for PRINTF debugging output, 
// debugging output options                     // LTEmC will satisfy PRINTF references with empty definition if not already resolved
#if defined(_DEBUG) && _DEBUG > 0
    asm(".global _printf_float");               // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                        // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG == 2
    #include <jlinkRtt.h>                       // PRINTF debug macro output to J-Link RTT channel
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#endif

#define SRCFILE "TLS"                           // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT
#include "ltemc-tls.h"
#include "ltemc-atcmd.h"


bool tls_configure(uint8_t dataCntxt, tlsVersion_t version, tlsCipher_t cipherSuite, tlsCertExpiration_t certExpirationCheck, tlsSecurityLevel_t securityLevel)
{
    if (atcmd_tryInvoke("AT+QSSLCFG=\"sslversion\",%d,%d", dataCntxt, version))                   // set SSL/TLS version
    {
        if (atcmd_awaitResult() != resultCode__success)                                         // return on failure, continue on success
            return false;
    }


    if (atcmd_tryInvoke("AT+QSSLCFG=\"ciphersuite\",%d,0X%X", dataCntxt, cipherSuite))            // set cipher suite
    {
        if (atcmd_awaitResult() != resultCode__success)                                         // return on failure, continue on success
            return false;
    }

    if (atcmd_tryInvoke("AT+QSSLCFG=\"ignorelocaltime\",%d,%d", dataCntxt, certExpirationCheck))  // set certificate expiration check
    {
        if (atcmd_awaitResult() != resultCode__success)                                         // return on failure, continue on success
            return false;
    }

    if (atcmd_tryInvoke("AT+QSSLCFG=\"seclevel\",%d,%d", dataCntxt, securityLevel))               // set security level, aka what is checked
    {
        if (atcmd_awaitResult() != resultCode__success)                                         // return on failure, continue on success
            return false;
    }

    return true;
}



tlsOptions_t tlsGetOptions(uint8_t dataCntxt)
{
    tlsOptions_t result = {0};

    if (atcmd_tryInvoke("AT+QSSLCFG=\"sslversion\",%d", (uint8_t)dataCntxt))    // get SSL\TLS version
    {   
        if (atcmd_awaitResult() == resultCode__success)
        {
            PRINTF(0, "%s", atcmd_getLastResponse());
            // strncpy(result.version, atResult.response);
        }
        atcmd_close();
    }
    return result;
}


resultCode_t tls_configSni(dataCntxt_t cntxt, bool setting)
{
    resultCode_t rslt = resultCode__internalError;

    if (atcmd_tryInvoke("AT+QSSLCFG=\"sni\",%d,%d", cntxt, setting))    // get SSL\TLS version
    {   
        if (atcmd_awaitResult() == resultCode__success)
        {
            PRINTF(0, "%s", atcmd_getLastResponse());
            // strncpy(result.version, atResult.response);
        }
        atcmd_close();
    }
    return rslt;
}
