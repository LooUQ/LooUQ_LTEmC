/******************************************************************************
 *  \file ltemc-http.h
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
 * HTTP(s) protocol support
 *****************************************************************************/

#ifndef __LTEMC_HTTP_H__
#define __LTEMC_HTTP_H__

#include "ltemc.h"

#define HTTP_URL_SZ 128
#define HTTP_CUSTOMHEADERS_SZ 480


/** 
 *  \brief typedef for the http data receiver function.
*/
typedef void (*httpReceiver_func_t)(void *recvData, uint16_t dataSz);


/** 
 *  \brief HTTP session type.
*/
typedef enum httpSessionType_tag
{
    httpSession_stdHeaders = 0,
    httpSession_customHeaders = 1
} httpSessionType_t;


/** 
 *  \brief HTTP session struct, controls a HTTP session (one or more) of GET and POST requests, without custom headers.
*/
typedef struct httpSessionStandardHdrs_tag
{
    uint8_t sessionType;
    char url[HTTP_URL_SZ];
    httpReceiver_func_t fileRecvr_func;
    uint16_t bufSz;
    char *recvBuf_1;
    char *recvBuf_2;
    tlsOptions_t tlsOptions;
} httpSessionStandardHdrs_t;


/** 
 *  \brief HTTP session struct, controls a HTTP session (one or more) of GET and POST requests, with custom headers.
*/
typedef struct httpSessionCustomHdrs_tag
{
    uint8_t sessionType;
    char url[HTTP_URL_SZ];
    char customHdrs[HTTP_CUSTOMHEADERS_SZ];
    httpReceiver_func_t fileRecvr_func;
    uint16_t bufSz;
    char *recvBuf_1;
    char *recvBuf_2;
    tlsOptions_t tlsOptions;
} httpSessionCustomHdrs_t;


/** 
 *  \brief HTTP session struct union.
*/
typedef union httpSession_tag
{
    httpSessionStandardHdrs_t standardHdrSession;
    httpSessionCustomHdrs_t customHdrSession;
} httpSession_t;


/** 
 *  \brief If using custom headers, bit-map indicating what headers to create for default custom header collection.
*/
typedef enum httpHeaderMap_tag              // bit-map to indicate headers to create for add custom headers, bitwise OR to http_addDefaultHdrs()
{
    httpHeaderMap_accept = 0x0001,
    httpHeaderMap_userAgent = 0x0002,
    httpHeaderMap_connection = 0x0004,
    httpHeaderMap_contentType = 0x0008,
    httpHeaderMap_contentLenght = 0x0010
} httpHeaderMap_t;


#ifdef __cplusplus
extern "C" {
#endif

// Session settings factory methods to create options for a series of GET/POST operations

httpSession_t http_createSession(httpSessionType_t sessionType, char *recvBuf_1, char *recvBuf_2, uint16_t recvBufSz, httpReceiver_func_t fileRecvr_func);

// httpSession_t http_createSession(const char *url, tlsOptions_t tlsOptions, char *recvBuf_1, char *recvBuf_2, uint16_t recvBufSz, httpReceiver_func_t fileRecvr_func);
// httpSessionCustomHdrs_t http_createSessionCustomHdrs(const char *url, char *recvBuf_1, char *recvBuf_2, uint16_t recvBufSz, httpReceiver_func_t fileRecvr_func);

// // Options for HTTP(s) session and helpers for custom headers session
// tlsOptions_t ltem_createTlsOptions(tlsVersion_t version, tlsCipher_t cipher, tlsCertExpiration_t certExpCheck, tlsSecurityLevel_t securityLevel);

void http_addDefaultHdrs(httpSessionCustomHdrs_t session, void *httpSession, uint16_t headerMap);
void http_addBasicAuthHdr(httpSessionCustomHdrs_t session, const char *user, const char *pw);
void http_addContentLengthHdr(httpSessionCustomHdrs_t session, uint16_t contentSz);

resultCode_t http_get(void *httpSession, const char* url);
resultCode_t http_post(void *httpSession, const char* url, const char* postData, uint16_t dataSz);

// // future support for fileSystem destinations, requires file_ module under development
// resultCode_t http_getFile(void *httpSession, const char* url, const char* filename);
// resultCode_t http_postFile(void *httpSession, const char* url, const char* filename);

#ifdef __cplusplus
}
#endif

#endif  // !__LTEMC_HTTP_H__
