/******************************************************************************
 *  \file ltemc-tls.c
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
 * SSL/TLS configuration for data contexts.
 *****************************************************************************/

#define _DEBUG 0                        // set to non-zero value for PRINTF debugging output, 
// debugging output options             // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if defined(_DEBUG) && _DEBUG > 0
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG == 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#endif

#include <lq-types.h>
#include "ltemc-tls.h"
#include "ltemc-atcmd.h"
#include <lq-assert.h>



bool tls_configure(dataContext_t context, tlsVersion_t version, tlsCipher_t cipherSuite, tlsCertExpiration_t certExpirationCheck, tlsSecurityLevel_t securityLevel)
{
    if (version != tlsVersion_doNotChange)                              // set SSL/TLS version
    {
        if (atcmd_tryInvokeDefaults("AT+QSSLCFG=\"sslversion\",%d,%d", context, version))
        {
            if (atcmd_awaitResult() != resultCode__success)             // return on failure, continue on success
                return false;
        }
    }
    // set cipher suite
    if (cipherSuite != tlsCipher_doNotChange)
    {
        if (atcmd_tryInvokeDefaults("AT+QSSLCFG=\"ciphersuite\",%d,0X%X", context, cipherSuite))
        {
            if (atcmd_awaitResult() != resultCode__success)             // return on failure, continue on success
                return false;
        }
    }
    // set certificate expiration check
    if (certExpirationCheck != tlsCertExpiration_doNotChange)
    {
        if (atcmd_tryInvokeDefaults("AT+QSSLCFG=\"ignorelocaltime\",%d,%d", context, certExpirationCheck))
        {
            if (atcmd_awaitResult() != resultCode__success)             // return on failure, continue on success
                return false;
        }
    }

    // set security level
    if (securityLevel != tlsSecurityLevel_doNotChange)
    {
        if (atcmd_tryInvokeDefaults("AT+QSSLCFG=\"seclevel\",%d,%d", context, securityLevel))
        {
            if (atcmd_awaitResult() != resultCode__success)             // return on failure, continue on success
                return false;
        }
    }
    return true;
}



tlsOptions_t tlsGetOptions(dataContext_t context)
{
    tlsOptions_t result = {0};

    if (atcmd_tryInvokeDefaults("AT+QSSLCFG=\"sslversion\",%d", (uint8_t)context))    // get SSL\TLS version
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