/** ***************************************************************************
  @file 
  @brief Modem HTTP(S) communication features/services.

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
#define lqLOG_LEVEL lqLOGLEVEL_VRBS
//#define DISABLE_ASSERTS                                   // ASSERT/ASSERT_W enabled by default, can be disabled 
#define LQ_SRCFILE "HTT"                                    // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT

#include "ltemc-internal.h"
#include "ltemc-http.h"

extern ltemDevice_t g_lqLTEM;

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))


/* Local Static Functions
------------------------------------------------------------------------------------------------------------------------- */
static resultCode_t S__httpGET(httpCtrl_t *httpCtrl, const char* relativeUrl, httpRequest_t* customRequest);
static resultCode_t S__httpPOST(httpCtrl_t *httpCtrl, const char *relativeUrl, httpRequest_t* request, const char *postData, uint16_t postDataSz);

static inline uint16_t S__setUrl(const char *host, const char *relative);
static resultCode_t S__httpRxHandler();


/* Public Functions
------------------------------------------------------------------------------------------------------------------------- */

/**
 *	@brief Create a HTTP(s) control structure to manage web communications. 
 */
void http_initControl(httpCtrl_t *httpCtrl, dataCntxt_t dataCntxt, httpAppRcvr_func rcvrCallback)
{
    ASSERT(httpCtrl != NULL);
    ASSERT(dataCntxt < dataCntxt__cnt);

    g_lqLTEM.streams[dataCntxt] = httpCtrl;

    memset(httpCtrl, 0, sizeof(httpCtrl_t));
    httpCtrl->dataCntxt = dataCntxt;
    httpCtrl->streamType = streamType_HTTP;
    httpCtrl->appRcvrCB = (appRcvr_func)rcvrCallback;
    httpCtrl->dataRxHndlr = S__httpRxHandler;

    httpCtrl->requestState = httpState_idle;
    httpCtrl->httpStatus = resultCode__unknown;
    httpCtrl->pageCancellation = false;
    httpCtrl->useTls = false;
    httpCtrl->timeoutSec = http__defaultTimeoutBGxSec;
    httpCtrl->defaultBlockSz = bbffr_getCapacity(g_lqLTEM.iop->rxBffr) / 4;
    // httpCtrl->cstmHdrsBffr = NULL;
    // httpCtrl->cstmHdrsBffrSz = 0;
    httpCtrl->httpStatus = 0xFFFF;
}


/**
 *	@brief Set host connection characteristics. 
 */
void http_setConnection(httpCtrl_t *httpCtrl, const char *hostUrl, uint16_t hostPort)
{
    ASSERT(strncmp(hostUrl, "HTTP", 4) == 0 || strncmp(hostUrl, "http", 4) == 0);
    ASSERT(hostPort == 0 || hostPort >= 80);

    strncpy(httpCtrl->hostUrl, hostUrl, sizeof(httpCtrl->hostUrl));

    httpCtrl->useTls = ((httpCtrl->hostUrl)[4] == 'S' || (httpCtrl->hostUrl)[4] == 's');
    if (hostPort == 0)
        httpCtrl->hostPort = httpCtrl->useTls ? 443 : 80;                  // if hostPort not specified, set from URL prefix
    else
        httpCtrl->hostPort = hostPort;
}


/**
 * @brief Creates a base HTTP request that can be appended with custom headers.
 * @note This function is ASSERT heavy; attempting to prevent buffer overflow issues 
 */
httpRequest_t http_createRequest(httpRequestType_t reqstType, const char* hostUrl, const char* relativeUrl, char* reqstBffr, uint16_t reqstBffrSz)
{
    ASSERT(reqstType == httpRequestType_GET || reqstType == httpRequestType_POST);
    ASSERT(hostUrl != NULL);
    ASSERT(relativeUrl != NULL);

    uint16_t hostUrlLen = strlen(hostUrl);
    uint16_t relativeUrlLen = strlen(relativeUrl);
    ASSERT(hostUrlLen > http__minUrlSz);

    /* Check for a viable request composition buffer. Need room for the overhead of the top and host lines, the combined URL lengths
     * the combined lengths of the "common" headers inserted by the add common headers function and at least 1 custom header.
     */
    //ASSERT(hostUrlLen + relativeUrlLen + http__requestBaseSz + http__standardHeadersSz + http__maxHeaderKeySz < reqstBffrSz);

    httpRequest_t httpReqst = { .buffer = reqstBffr, .buffersz = reqstBffrSz, .contentLen = 0, .headersLen = 0 };
    memset(reqstBffr, 0, reqstBffrSz);

    if (COMPARES(memcmp(hostUrl, "http", 4)) || COMPARES(memcmp(hostUrl, "HTTP", 4)))           // allow for protocol to be supplied in host URL
    {
        hostUrl = memchr(hostUrl, ':', strlen(hostUrl)) + 3;                                    // if supplied, skip over
    }

    if (reqstType == httpRequestType_GET)
        strcat(reqstBffr, "GET ");
    else if (reqstType == httpRequestType_POST)
        strcat(reqstBffr, "POST ");
    else
    {
        strcat(reqstBffr, "INVALID_TYPE");
        return httpReqst;
    }

    if (strlen(relativeUrl))                                                                    // now create relative path                                           
        strcat(reqstBffr, relativeUrl);
    else
        strcat(reqstBffr, "/");

    strcat(reqstBffr, " HTTP/1.1\r\nHost: ");                                                   // complete the base request: 1st two lines
    strcat(reqstBffr, hostUrl);
    strcat(reqstBffr, "\r\n");

    httpReqst.buffer == reqstBffr;
    httpReqst.headersLen = strlen(httpReqst.buffer);                                            // update request object
    return httpReqst;
}


/**
 * @brief Adds common HTTP headers to a custom headers buffer.
 */
void http_addStandardHeaders(httpRequest_t* httpReqst, httpHeaderMap_t headerMap)
{
    ASSERT(headerMap > 0);
    ASSERT(httpReqst->contentLen == 0);                                                     // headers section is still open to additions
    ASSERT(*(httpReqst->buffer + httpReqst->headersLen - 2) == '\r');                       // existing request ends in \r\n
    ASSERT(httpReqst->headersLen + http__standardHeadersSz + 4 < httpReqst->buffersz);      // "all" headers could fit + 2 EOLs (POST) possible

    if (headerMap & httpHeaderMap_accept > 0 || headerMap == httpHeaderMap_all)
    {
        strcat(httpReqst->buffer, "Accept: */*\r\n");                                       // 13 = "Accept: */*\r\n" 
    }
    if (headerMap & httpHeaderMap_userAgent > 0 || headerMap == httpHeaderMap_all)
    {
        strcat(httpReqst->buffer, "User-Agent: QUECTEL_MODULE\r\n");                        // 28 = "User-Agent: QUECTEL_BGx\r\n"
    }
    if (headerMap & httpHeaderMap_connection > 0 || headerMap == httpHeaderMap_all)
    {
        strcat(httpReqst->buffer, "Connection: Keep-Alive\r\n");                            // 24 = "Connection: Keep-Alive\r\n"
    }
    if (headerMap & httpHeaderMap_contentType > 0 || headerMap == httpHeaderMap_all)
    {
        strcat(httpReqst->buffer, "Content-Type: application/octet-stream\r\n");            // 40 = "Content-Type: application/octet-stream\r\n"
    }
    httpReqst->headersLen = strlen(httpReqst->buffer);                                      // update object
}


/**
 * @brief Adds a basic authorization header to a headers buffer.
 */
void http_addBasicAuthHdr(httpRequest_t* httpReqst, const char *user, const char *pw)
{
    char toEncode[80];
    char b64str[120];

    ASSERT(user != NULL);
    ASSERT(pw != NULL);
    ASSERT(httpReqst->contentLen == 0);                                                         // headers section still open to additions
    ASSERT(*(httpReqst->buffer + httpReqst->headersLen - 2) == '\r');                           // existing request ends in \r\n

    strcat(toEncode, user);
    strcat(toEncode, ":");
    strcat(toEncode, pw);
    binToB64(b64str, toEncode, strlen(toEncode));                                               // endcode credentials to Base64 string
    ASSERT(httpReqst->headersLen + strlen(b64str) + http__authHeaderSz < httpReqst->buffersz);  // "Authentication: " + "\r\n" = length 20

    strcat(httpReqst->buffer, "Authentication: ");
    strcat(httpReqst->buffer, b64str);
    strcat(httpReqst->buffer, "\r\n");                                                          // new header ends in correct EOL
    httpReqst->headersLen = strlen(httpReqst->buffer);                                          // update object
}


void http_addHeader(httpRequest_t* httpReqst, const char * keyAndValue)
{
    ASSERT(httpReqst->contentLen == 0);                                                     // headers section still open to additions
    ASSERT(*(httpReqst->buffer + httpReqst->headersLen - 2) == '\r');                       // existing request ends in \r\n
    ASSERT(keyAndValue != NULL);
    ASSERT(strnstr(keyAndValue, ": ", http__maxHeaderKeySz) != NULL);                       // keyAndValue is valid header (contains delimeter)

    uint8_t newHdrSz = strlen(keyAndValue);
    bool missingEOL = false;
    if (memchr(keyAndValue, '\r', newHdrSz) == NULL)
    {
        newHdrSz += 2;                                                                      // need add EOL (\r\n)
        missingEOL = true;
    }
    ASSERT(httpReqst->headersLen + newHdrSz < httpReqst->buffersz);                         // new header fits
    
    strcat(httpReqst->buffer, keyAndValue);
    if (missingEOL)
        strcat(httpReqst->buffer, "\r\n");                                                  // add missing EOL
    httpReqst->headersLen = strlen(httpReqst->buffer);                                      // update object
}


/**
 * @brief Helper to compose a generic header and add it to the headers collection being composed.
 */
void http_addHeaderKeyAndValue(httpRequest_t* httpReqst, const char *key, const char *value)
{
    ASSERT(key != NULL);
    ASSERT(value != NULL);
    ASSERT(httpReqst->contentLen == 0);                                                     // headers section still open to additions
    ASSERT(*(httpReqst->buffer + httpReqst->headersLen - 2) == '\r');                       // existing request ends in \r\n

    uint8_t newHdrSz = strlen(key) + 2 + strlen(value) + 2;                                 // <key>: <val>\r\n
    ASSERT(httpReqst->headersLen + newHdrSz < httpReqst->buffersz);                         // new header fits

    strcat(httpReqst->buffer, key);
    strcat(httpReqst->buffer, ": ");
    strcat(httpReqst->buffer, value);
    strcat(httpReqst->buffer, "\r\n");                                                      // new header ends in correct EOL
    httpReqst->headersLen = strlen(httpReqst->buffer);                                      // update object
}


/**
 * @brief Finalize/close headers to additional header additions.
 * @note BGx requires that the Content-Length header is the last one in the header section of a custom request.
 * 
 * @param httpReqst The custom request object to close headers on.
 */
void http_closeHeaders(httpRequest_t* httpReqst)
{
    if (httpReqst->contentLen == 0)                                                         // if no content yet: either a GET or POST without data so far
    {
        uint16_t searchStart = httpReqst->headersLen - 30;                                  // check for header
        char * lenHeaderPtr = strnstr(httpReqst->buffer + searchStart, "Content-Length:", 30);
        if (lenHeaderPtr == NULL)                                                           // no Content-Length header, so add (always last header for BGx)
        {
            strcat(httpReqst->buffer, "Content-Length:     0\r\n\r\n");                     // add placeholder, POST will update actual length at invoke
            httpReqst->headersLen = strlen(httpReqst->buffer);
        }
    }
}


void http_updateContentLength(httpRequest_t* httpReqst, uint16_t contentLength)
{
    char contentLengthFld[6];
    char * contentLengthPtr = strnstr(httpReqst->buffer, "Content-Length: ", httpReqst->headersLen);

    httpReqst->contentLen = contentLength;

    snprintf(contentLengthFld, sizeof(contentLengthFld), "%5d", contentLength);             // fixup content-length header value 
    memcpy(contentLengthPtr + 16, contentLengthFld, 5);
}


/**
 * @brief Helper to compose a generic header and add it to the headers collection being composed.
 */
uint16_t http_addPostData(httpRequest_t* httpReqst, const char *postData, uint16_t postDataSz)
{
    ASSERT(postData != NULL);
    ASSERT(*(httpReqst->buffer + httpReqst->headersLen - 2) == '\r');                       // existing request ends in \r\n

    http_closeHeaders(httpReqst);

    uint16_t availableSz = httpReqst->buffersz - (httpReqst->headersLen + httpReqst->contentLen);
    uint16_t copySz = MIN(availableSz, postDataSz);
    memcpy(httpReqst->buffer + httpReqst->headersLen + httpReqst->contentLen, postData, copySz);
    httpReqst->contentLen += copySz;                                                        // update reqst object content offset
    return postDataSz - copySz;                                                             // 0=it all fit, otherwise how many got dropped
}


/* ------------------------------------------------------------------------------------------------
 *  Request and Response Section 
 * --------------------------------------------------------------------------------------------- */

/**
 *	@brief Perform HTTP GET request. 
 *  -----------------------------------------------------------------------------------------------
 */
resultCode_t http_get(httpCtrl_t *httpCtrl, const char* relativeUrl)
{
    ASSERT(relativeUrl != NULL);

    return S__httpGET(httpCtrl, relativeUrl, NULL);
}


/**
 *	@brief Performs a custom (headers) GET request.
 *  -----------------------------------------------------------------------------------------------
 */
resultCode_t http_getCustomRequest(httpCtrl_t *httpCtrl, httpRequest_t* customRequest)
{
    ASSERT(customRequest->contentLen == 0);

    return S__httpGET(httpCtrl, NULL, customRequest);
}


/**
 *	@brief Performs HTTP GET web request.
 *  -----------------------------------------------------------------------------------------------
 */
static resultCode_t S__httpGET(httpCtrl_t *httpCtrl, const char* relativeUrl, httpRequest_t* customRequest)
{
    ASSERT(httpCtrl != NULL && httpCtrl->streamType == streamType_HTTP);
    ASSERT(relativeUrl != NULL || customRequest != NULL);                       // must include relative URL (can be empty) -OR- customRequest
    ASSERT(relativeUrl == NULL || customRequest == NULL);                       // but not both

    httpCtrl->requestState = httpState_idle;
    httpCtrl->httpStatus = resultCode__unknown;
    strcpy(httpCtrl->requestType, "GET");

    RSLT;
    if (IS_NOTSUCCESS_RSLT(atcmd_dispatch("AT+QHTTPCFG=\"responseheader\",%d",  (int)(httpCtrl->responseHdrs))))
    {
        return rslt;
    }

    if (httpCtrl->useTls)
    {
        if (IS_NOTSUCCESS_RSLT(atcmd_dispatch("AT+QHTTPCFG=\"sslctxid\",%d",  (int)httpCtrl->dataCntxt)))
        {
            return rslt;
        }
    }

    uint8_t reqstHdrs = (uint8_t)customRequest != NULL;
    if (IS_NOTSUCCESS_RSLT(atcmd_dispatch("AT+QHTTPCFG=\"requestheader\",%d", reqstHdrs)))
    {
        return rslt;
    }

    /* SET URL FOR REQUEST
    * set BGx HTTP URL: AT+QHTTPURL=<urlLength>,timeoutSec  (BGx default timeout is 60, if not specified)
    * wait for CONNECT prompt, then output <URL>, /r/n/r/nOK
    * 
    * NOTE: there is only ONE URL in the BGx at a time
    *---------------------------------------------------------------------------------------------------------------*/

    char * urlEndPtr = NULL;
    if (customRequest)
    {
        urlEndPtr = strchr(customRequest->buffer + 5, ' ');
        *urlEndPtr = '\0';                                                      // make relativeUrl (within reqst) NULL term'd
        relativeUrl = customRequest->buffer + 5;
    }
    if (IS_NOTSUCCESS_RSLT(S__setUrl(httpCtrl->hostUrl, relativeUrl)))
    {
        lqLOG_WARN("Failed set URL rslt=%d\r\n", rslt);
        return rslt;
    }
    if (urlEndPtr)                                                              // restore request to original
        *urlEndPtr = ' ';


    /* INVOKE HTTP GET METHOD
    * BGx responds with OK immediately upon acceptance of cmd, then later (up to timeout) with "+QHTTPGET: " string
    * After "OK" we switch IOP to data mode and return. S_httpDoWork() handles the parsing of the page response and
    * if successful, the issue of the AT+QHTTPREAD command to start the page data stream
    * 
    * This allows other application tasks to be performed while waiting for page. No LTEm commands can be invoked
    * but non-LTEm tasks like reading sensors can continue.
    *---------------------------------------------------------------------------------------------------------------*/

    atcmd_ovrrdTimeout(PERIOD_FROM_SECONDS(httpCtrl->timeoutSec));
    atcmd_configParser("+QHTTPGET: ", true, ",", 0, "\r\n", 0);

    if (reqstHdrs)                                                              // custom HTTP GET request
    {
        http_closeHeaders(customRequest);
        atcmd_configDataMode(httpCtrl, "CONNECT\r\n", atcmd_txHndlrDefault, customRequest->buffer, customRequest->headersLen, NULL, true);
        rslt = atcmd_dispatch("AT+QHTTPGET=%d,%d", httpCtrl->timeoutSec, customRequest->headersLen);
    }
    else                                                                                                            // default HTTP GET request
    {
        rslt = atcmd_dispatch("AT+QHTTPGET=%d", PERIOD_FROM_SECONDS(httpCtrl->timeoutSec));
    }

    httpCtrl->requestState = httpState_idle;
    char * httpError = atcmd_getToken(0);
    if (IS_SUCCESS(rslt) && (strlen(httpError) > 0 && *httpError == '0'))
    {
        char * httpRslt = atcmd_getToken(1);
        lqLOG_VRBS("ReqstRslt:%d, HTTP-Rslt:%s\r\n", rslt, httpRslt);
        httpCtrl->httpStatus = strtol(httpRslt, NULL, 10);
        if (IS_SUCCESSRANGE(httpCtrl->httpStatus))
        {
            httpCtrl->requestState = httpState_requestComplete;                                                     // update httpState, got GET/POST response
            lqLOG_INFO("GetRqst dCntxt:%d, status=%d\r\n", httpCtrl->dataCntxt, httpCtrl->httpStatus);
        }
    }
    else
    {
        httpCtrl->httpStatus = resultCode__extendedCodesBase + rslt;
        lqLOG_WARN("Closed failed GET request, status=%d\r\n", httpCtrl->httpStatus);
    }
    return httpCtrl->httpStatus;
}   // http_get()


/**
 *	@brief Performs a HTTP POST page web request.
 *  -----------------------------------------------------------------------------------------------
 */
resultCode_t http_post(httpCtrl_t *httpCtrl, const char *relativeUrl, const char *postData, uint16_t postDataSz)
{
    return S__httpPOST(httpCtrl, relativeUrl, NULL, postData, postDataSz);
}


/**
 *	@brief Performs a HTTP POST page web request.
 *  -----------------------------------------------------------------------------------------------
 */
resultCode_t http_postCustomRequest(httpCtrl_t *httpCtrl, const char* relativeUrl, httpRequest_t* customRequest)
{
    return S__httpPOST(httpCtrl, relativeUrl, customRequest, NULL, 0);
}


/**
 *	@brief Performs a HTTP POST page web request.
 *  -----------------------------------------------------------------------------------------------
 */
resultCode_t S__httpPOST(httpCtrl_t *httpCtrl, const char *relativeUrl, httpRequest_t* customRequest, const char *postData, uint16_t postDataSz)
{
    ASSERT(httpCtrl != NULL && httpCtrl->streamType == streamType_HTTP);
    ASSERT(relativeUrl != NULL || customRequest != NULL);                       // must include relative URL (can be empty) -OR- customRequest
    ASSERT(relativeUrl == NULL || customRequest == NULL);                       // but not both

    httpCtrl->requestState = httpState_idle;
    httpCtrl->httpStatus = resultCode__unknown;
    strcpy(httpCtrl->requestType, "POST");

    RSLT;
    if (IS_SUCCESS_RSLT(atcmd_dispatch("AT+QHTTPCFG=\"responseheader\",%d",  (int)httpCtrl->responseHdrs)))
    {
        return rslt;
    }

    if (httpCtrl->useTls)
    {
        if (IS_SUCCESS_RSLT(atcmd_dispatch("AT+QHTTPCFG=\"sslctxid\",%d",  (int)httpCtrl->dataCntxt)))
        {
            return rslt;
        }
    }

    uint8_t reqstHdrs = (uint8_t)customRequest != NULL;
    if (IS_NOTSUCCESS_RSLT(atcmd_dispatch("AT+QHTTPCFG=\"requestheader\",%d", reqstHdrs)))
    {
        return rslt;
    }

    /* SET URL FOR REQUEST
    * set BGx HTTP URL: AT+QHTTPURL=<urlLength>,timeoutSec  (BGx default timeout is 60, if not specified)
    * wait for CONNECT prompt, then output <URL>, /r/n/r/nOK
    * 
    * NOTE: there is only 1 URL in the BGx at a time
    *---------------------------------------------------------------------------------------------------------------*/

    char * urlEndPtr = NULL;
    if (customRequest)
    {
        urlEndPtr = strchr(customRequest->buffer + 5, ' ');
        *urlEndPtr = '\0';                                                                      // make relativeUrl (within reqst) NULL term'd
        relativeUrl = customRequest->buffer + 5;
    }
    if (IS_NOTSUCCESS_RSLT(S__setUrl(httpCtrl->hostUrl, relativeUrl)))
    {
        lqLOG_WARN("Failed set URL rslt=%d\r\n", rslt);
        return rslt;
    }
    if (urlEndPtr)                                                                              // restore request to original
        *urlEndPtr = ' ';

    atcmd_ovrrdTimeout(PERIOD_FROM_SECONDS(httpCtrl->timeoutSec));
    atcmd_configParser("+QHTTPPOST: ", true, ",", 0, "\r\n", 0);

    if (customRequest)
    {
        http_closeHeaders(customRequest);                                                       // should already be here but just in case (will not be double added)
        char contentLengthFld[6];
        snprintf(contentLengthFld, sizeof(contentLengthFld), "%5d", customRequest->contentLen); // fixup content-length header value 
        char* contentLengthPtr = customRequest->buffer + customRequest->headersLen - 9;         // backup over the /r/n/r/n and content-length value
        memcpy(contentLengthPtr, contentLengthFld, 5);

        uint16_t dataLen = customRequest->headersLen + customRequest->contentLen;
        atcmd_configDataMode(httpCtrl, "CONNECT\r\n", atcmd_txHndlrDefault, customRequest->buffer, dataLen, NULL, true);
        rslt = atcmd_dispatch("AT+QHTTPPOST=%d,5,%d", dataLen, httpCtrl->timeoutSec);
    }
    else
    {
        uint16_t dataLength = strlen(postData);
        atcmd_configDataMode(httpCtrl, "CONNECT\r\n", atcmd_txHndlrDefault, postData, dataLength, NULL, true);
        rslt = atcmd_dispatch("AT+QHTTPPOST=%d,5,%d", strlen(postData), httpCtrl->timeoutSec);
    }

    httpCtrl->requestState = httpState_idle;
    char * httpError = atcmd_getToken(0);
    if (IS_SUCCESS(rslt) && (strlen(httpError) > 0 && *httpError == '0'))
    {
        char * httpRslt = atcmd_getToken(1);
        lqLOG_VRBS("ReqstRslt:%d, HTTP-Rslt:%s\r\n", rslt, httpRslt);
        httpCtrl->httpStatus = strtol(httpRslt, NULL, 10);
        if (IS_SUCCESSRANGE(httpCtrl->httpStatus))
        {
            httpCtrl->requestState = httpState_requestComplete;                                                     // update httpState, got GET/POST response
            lqLOG_INFO("GetRqst dCntxt:%d, status=%d\r\n", httpCtrl->dataCntxt, httpCtrl->httpStatus);
        }
    }
    else
    {
        httpCtrl->httpStatus = resultCode__extendedCodesBase + rslt;
        lqLOG_WARN("Closed failed GET request, status=%d\r\n", httpCtrl->httpStatus);
    }
    return httpCtrl->httpStatus;
}  // http_post()


/**
 *	@brief Sends contents of a file (LTEM filesystem) as POST to remote.
 */
resultCode_t http_postFile(httpCtrl_t *httpCtrl, const char *relativeUrl, const char* filename, bool customHeaders)
{
    httpCtrl->requestState = httpState_idle;
    httpCtrl->httpStatus = resultCode__unknown;
    strcpy(httpCtrl->requestType, "POST");

    RSLT;
    if (IS_NOTSUCCESS_RSLT(atcmd_dispatch("AT+QHTTPCFG=\"contextid\",1")))
    {
        return rslt;
    }

    if (IS_NOTSUCCESS_RSLT(atcmd_dispatch("AT+QHTTPCFG=\"responseheader\",%d",  (int)(httpCtrl->responseHdrs))))
    {
        return rslt;
    }

    if (httpCtrl->useTls)
    {
        if (IS_NOTSUCCESS_RSLT(atcmd_dispatch("AT+QHTTPCFG=\"sslctxid\",%d",  (int)httpCtrl->dataCntxt)))
        {
            return rslt;
        }
    }

    /* POST file may or may not include a custom request, set flag for request + body in file
     */
    if (IS_NOTSUCCESS_RSLT(atcmd_dispatch("AT+QHTTPCFG=\"requestheader\",%d", (int)customHeaders)))
    {
        return rslt;
    }

    /* SET URL FOR REQUEST
    * set BGx HTTP URL: AT+QHTTPURL=<urlLength>,timeoutSec  (BGx default timeout is 60, if not specified)
    * wait for CONNECT prompt, then output <URL>, /r/n/r/nOK
    * 
    * NOTE: there is only 1 URL in the BGx at a time
    *---------------------------------------------------------------------------------------------------------------*/

    if (IS_NOTSUCCESS_RSLT(S__setUrl(httpCtrl->hostUrl, relativeUrl)))
    {
        lqLOG_WARN("Failed set URL rslt=%d\r\n", rslt);
        return rslt;
    }

    /* INVOKE HTTP ** POST ** METHOD
    * BGx responds with OK immediately upon acceptance of cmd, then later (up to timeout) with "+QHTTPPOST: " string
    * After "OK" we switch IOP to data mode and return. S_httpDoWork() handles the parsing of the page response and
    * if successful, the issue of the AT+QHTTPREAD command to start the page data stream
    * 
    * This allows other application tasks to be performed while waiting for page. No LTEm commands can be invoked
    * but non-LTEm tasks like reading sensors can continue.
    *---------------------------------------------------------------------------------------------------------------*/

    atcmd_ovrrdTimeout(PERIOD_FROM_SECONDS(httpCtrl->timeoutSec));
    atcmd_configParser("+QHTTPPOSTFILE: ", true, ",", 0, "\r\n", 0);

    if (IS_SUCCESS_RSLT(atcmd_dispatch("AT+QHTTPPOSTFILE=\"%s\"", filename)))
    {
        lqLOG_VRBS("(http_postFile) timeout=%d\r\n", httpCtrl->timeoutSec);
        lqLOG_VRBS("(http_postFile) rslt=%d\r\n", rslt);
        lqLOG_VRBS("(http_postFile) response=%s\r\n", atcmd_getRawResponse());

        httpCtrl->requestState = httpState_idle;
        char * httpError = atcmd_getToken(0);
        if (IS_SUCCESS(rslt) && (strlen(httpError) > 0 && *httpError == '0'))
        {
            char * httpRslt = atcmd_getToken(1);
            lqLOG_VRBS("ReqstRslt:%d, HTTP-Rslt:%s\r\n", rslt, httpRslt);
            httpCtrl->httpStatus = strtol(httpRslt, NULL, 10);
            if (IS_SUCCESSRANGE(httpCtrl->httpStatus))
            {
                httpCtrl->requestState = httpState_requestComplete;                                                     // update httpState, got GET/POST response
                lqLOG_INFO("GetRqst dCntxt:%d, status=%d\r\n", httpCtrl->dataCntxt, httpCtrl->httpStatus);
            }
        }
        else
        {
            httpCtrl->httpStatus = resultCode__extendedCodesBase + rslt;
            lqLOG_WARN("Failed POST (file) request, status=%d\r\n", httpCtrl->httpStatus);
        }
    }
    return httpCtrl->httpStatus;
}  // http_postFile()


/**
 * @brief Retrieves page results from a previous GET or POST.
 * @return HTTP status code from server
 */
uint16_t http_readPage(httpCtrl_t *httpCtrl)
{
    ASSERT(httpCtrl != NULL && httpCtrl->dataRxHndlr != NULL);

    if (httpCtrl->requestState != httpState_requestComplete)
        return resultCode__preConditionFailed;                                  // readPage() only valid after a completed GET\POST
    
    atcmd_configDataMode(httpCtrl, "CONNECT\r\n", S__httpRxHandler, NULL, 0, httpCtrl->appRcvrCB, false);
    return atcmd_dispatch("AT+QHTTPREAD=%d", httpCtrl->timeoutSec);
}


/**
 * @brief Read HTTP page to BGx file system
 * @return HTTP status code from server
 */
uint16_t http_readPageToFile(httpCtrl_t *httpCtrl, const char* filename)
{
    ASSERT(httpCtrl != NULL && httpCtrl->streamType == streamType_HTTP);
    ASSERT(strlen(filename) < http__readToFileNameSzMax);

    if (httpCtrl->requestState != httpState_requestComplete)
        return resultCode__preConditionFailed;                                  // readPage() only valid after a completed GET\POST

    RSLT;

    atcmd_ovrrdTimeout(SEC_TO_MS(http__readToFileTimeoutSec));
    atcmd_configParser("+QHTTPREADFILE: ", true, ",", 0, "\r\n", 0);

    if (IS_SUCCESS_RSLT(atcmd_dispatch("AT+QHTTPREADFILE=\"%s\",%d", filename, http__readToFileInterPcktTimeoutSec)))
    {
        if (strlen(atcmd_getRawResponse()) > sizeof("AT+QHTTPREADFILE: 0") && *atcmd_getResponse() == '0')
            return resultCode__success;
        else
            return resultCode__internalError;
    }
    return rslt;
}


/**
 * @brief Clear state for a request to abandon read
 */
void http_cancelPage(httpCtrl_t *httpCtrl)
{
    ASSERT(false);                  // not implemented
}


#pragma endregion


#pragma region Static Functions
/*-----------------------------------------------------------------------------------------------*/

/**
 * @brief Helper function to create a URL from host and relative parts.
 */
static inline resultCode_t S__setUrl(const char *host, const char *relative)
{
    char url[http__maxUrlSz] = {0};
    
    strcpy(url, host);
    if (strlen(relative) > 0)                                                                       // append relative
    {
        if (*(host + strlen(host) - 1) != '/' && *relative != '/')                                  // need / separator, if not provided
            strcat(url, "/");
        strcat(url, relative);
    }
    lqLOG_VRBS("URL(%d)=%s", strlen(url), url);
    lqLOG_VRBS("\r\n");                                                                             // separate line-end, encase URL truncates in trace
    
    atcmd_configDataMode(0, "CONNECT\r\n", atcmd_txHndlrDefault, url, strlen(url), NULL, false);    // setup for URL dataMode transfer 
    return atcmd_dispatch("AT+QHTTPURL=%d,5", strlen(url));
}


/**
 * @brief Translate a module specific HTTP error code into a standard HTTP response code.
 * 
 * @param [in] extendedResultCode BGx HTTP error code.
 * @return resultCode_t Translated standard HTTP response code.
 */
resultCode_t http_translateExtended(uint16_t extendedResultCode)
{
    if (extendedResultCode < resultCode__extendedCodesBase)
        return extendedResultCode;
        
    switch (extendedResultCode)
    {
        case 1705:
        case 1730:
            return resultCode__badRequest;          // 400

        case 1711:
        case 1712:
        case 1713:
        case 1714:
            return resultCode__notFound;            // 404

        case 1702:
        case 1726:
        case 1727:
        case 1728:
            return resultCode__timeout;             // 408

        case 1703:
        case 1704:
            return resultCode__conflict;            // 409

        default:
            return resultCode__internalError;       // 500
    }
}


/**
 * @brief Handles the READ data flow from the BGx (via rxBffr) to app
 */
static resultCode_t S__httpRxHandler()
{
    char wrkBffr[32];
    uint16_t pageRslt = 0;

    httpCtrl_t *httpCtrl = (httpCtrl_t*)g_lqLTEM.atcmd->dataMode.streamCtrl;
    ASSERT(httpCtrl != NULL);                                                                                   // ASSERT data mode and stream context are consistent

    uint8_t popCnt = bbffr_find(g_lqLTEM.iop->rxBffr, "\r", 0, 0, false);
    if (BBFFR_ISNOTFOUND(popCnt))
    {
        return resultCode__internalError;
    }
    
    bbffr_pop(g_lqLTEM.iop->rxBffr, wrkBffr, popCnt + 2);                                                       // pop CONNECT phrase for parsing data length
    lqLOG_VRBS("(S__httpRxHandler) stream started\r\n");

    bool dataClosed = false;
    uint32_t readStart = pMillis();
    do
    {
        uint16_t occupiedCnt = bbffr_getOccupied(g_lqLTEM.iop->rxBffr);
        bool readTimeout = pMillis() - readStart > httpCtrl->timeoutSec;
        uint16_t trailerIndx = bbffr_find(g_lqLTEM.iop->rxBffr, "\r\nOK\r\n\r\n", 0, 0, false);
        uint16_t reqstBlockSz = MIN(trailerIndx, httpCtrl->defaultBlockSz);                                     // trailerIndx 65535, if not found

        if (bbffr_getOccupied(g_lqLTEM.iop->rxBffr) >= reqstBlockSz && !dataClosed)                             // buffer has intermediate block or end-of-data
        {
            char* streamPtr;
            uint16_t blockSz = bbffr_popBlock(g_lqLTEM.iop->rxBffr, &streamPtr, reqstBlockSz);                  // get address from rxBffr
            dataClosed = BBFFR_ISFOUND(trailerIndx);

            lqLOG_VRBS("(S__httpRxHandler) ptr=%p blkSz=%d isFinal=%d\r\n", streamPtr, blockSz, dataClosed);

            ((httpAppRcvr_func)(*httpCtrl->appRcvrCB))(httpCtrl->dataCntxt, streamPtr, blockSz, dataClosed);    // forward to application

            bbffr_popBlockFinalize(g_lqLTEM.iop->rxBffr, true);                                                 // commit POP
            bbffr_skipTail(g_lqLTEM.iop->rxBffr, sizeof("\r\nOK\r\n\r\n") - 1);
        }

        if (dataClosed)                                                                                         // found start of trailer
        {
            uint16_t eolAt = bbffr_find(g_lqLTEM.iop->rxBffr, "\r\n", 0, 0, false);                             // EOL signals result code is available
            if (BBFFR_ISFOUND(eolAt))
            {
                ASSERT(eolAt < sizeof(wrkBffr));
                memset(wrkBffr, 0, sizeof(wrkBffr));                                                            // need clean wrkBffr for trailer parsing
                bbffr_pop(g_lqLTEM.iop->rxBffr, wrkBffr, eolAt);

                char* trailerPtr = strnstr(wrkBffr, "+QHTTPREAD: ", sizeof(wrkBffr));
                if (trailerPtr)
                {
                    trailerPtr += sizeof("+QHTTPREAD: ") - 1;
                    uint16_t errVal = strtol(trailerPtr, NULL, 10);

                    errVal = (errVal == 0) ? 200 : errVal;
                    return errVal;
                }
                return resultCode__internalError;
            }
        }
    } while (true);
}


void http_writeRequestToLog(httpRequest_t* httpReqst)
{
    const char* workPtr = httpReqst->buffer;
    uint32_t cLen = httpReqst->contentLen;
    char lineBffr[128];

    lqLog_printf(PRNT_dMAGENTA, "HTTP Request\r\n------------------------------");

    char* hdrEOL = strstr(httpReqst, "\r\n\r\n");
    char* eol;
    uint8_t lineSz;

    if (hdrEOL != NULL)
    {
        while (workPtr < hdrEOL)
        {
            eol = (char*)memchr(workPtr, '\r', sizeof(lineBffr));
            lineSz = MIN(eol - workPtr, sizeof(lineBffr)-1);
            strncpy(lineBffr, workPtr, lineSz);
            lqLog_printf(" - %s\r\n", lineBffr);
            if (eol == hdrEOL)
            {
                workPtr = eol + 4;
                for (size_t i = 0; i < 5; i++)             // output up to 5 lines
                {
                    lineSz = MIN(cLen, sizeof(lineBffr)-1);
                    strncpy(lineBffr, workPtr, lineSz);
                    lqLog_printf(PRNT_dMAGENTA, " > %s\r\n", lineBffr);
                    cLen -= lineSz;
                }
            }
            workPtr = eol + 2;                              // next header line
        }
    }
    else
        lqLog_printf(PRNT_dMAGENTA, "Malformed request: no header separator\r\n");
}

#pragma endregion


#pragma Static Response Parsers

/* Note for parsers below:
 * httprspcode is only reported if err is 0, have to search for finale (\r\n) after a preamble (parser speak)
 * --------------------------------------------------------------------------------------------- */

// static cmdParseRslt_t S__httpGetStatusParser() 
// {
//     // +QHTTPGET: <err>[,<httprspcode>[,<content_length>]]
//     return atcmd_stdResponseParser("+QHTTPGET: ", true, ",", 0, 1, "\r\n", 0);
// }


// static cmdParseRslt_t S__httpPostStatusParser() 
// {
//     // +QHTTPPOST: <err>[,<httprspcode>[,<content_length>]] 
//     return atcmd_stdResponseParser("+QHTTPPOST: ", true, ",", 0, 1, "\r\n", 0);
// }


// static cmdParseRslt_t S__httpReadFileStatusParser() 
// {
//     // +QHTTPPOST: <err>[,<httprspcode>[,<content_length>]] 
//     return atcmd_stdResponseParser("+QHTTPREADFILE: ", true, ",", 0, 1, "\r\n", 0);
// }


// static cmdParseRslt_t S__httpPostFileStatusParser() 
// {
//     // +QHTTPPOST: <err>[,<httprspcode>[,<content_length>]] 
//     return atcmd_stdResponseParser("+QHTTPPOSTFILE: ", true, ",", 0, 1, "\r\n", 0);
// }

#pragma endregion