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
#define LOG_LEVEL LOGLEVEL_DBG
//#define DISABLE_ASSERTS                   // ASSERT/ASSERT_W enabled by default, can be disabled 
#define LQ_SRCFILE "HTT"                    // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT

#define ENABLE_DIAGPRINT                    // expand DPRINT into debug output
//#define ENABLE_DIAGPRINT_VERBOSE            // expand DPRINT and DPRINT_V into debug output
//#define ENABLE_ASSERT
// #include <lqdiag.h>

#include "ltemc-internal.h"
#include "ltemc-http.h"

extern ltemDevice_t g_lqLTEM;

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))


/* Local Static Functions
------------------------------------------------------------------------------------------------------------------------- */
static resultCode_t S__httpGET(httpCtrl_t *httpCtrl, const char* relativeUrl, httpRequest_t* customRequest);
static resultCode_t S__httpPOST(httpCtrl_t *httpCtrl, const char *relativeUrl, httpRequest_t* request, const char *postData, uint16_t postDataSz);

static uint16_t S__setUrl(const char *host, const char *relative);
// static cmdParseRslt_t S__httpGetStatusParser();
// static cmdParseRslt_t S__httpPostStatusParser();
// static cmdParseRslt_t S__httpReadFileStatusParser();
// static cmdParseRslt_t S__httpPostFileStatusParser();
// static uint16_t S__parseResponseForHttpStatus(httpCtrl_t *httpCtrl, const char *responseTail);
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
    {
        httpCtrl->hostPort = httpCtrl->useTls ? 443 : 80;                  // if hostPort default, set from URL prefix
    }
}


/**
 * @brief Creates a base HTTP request that can be appended with custom headers.
 */
httpRequest_t http_createRequest(httpRequestType_t reqstType, const char* hostUrl, const char* relativeUrl, char* reqstBffr, uint16_t reqstBffrSz)
{
    ASSERT(strlen(hostUrl));
    ASSERT(strlen(relativeUrl));

    httpRequest_t httpReqst = { .buffer = reqstBffr, .buffersz = reqstBffrSz, .contentLen = 0, .headersLen = 0 };
    memset(reqstBffr, 0, reqstBffrSz);

    if (COMPARES(memcmp(hostUrl, "http", 4)) || COMPARES(memcmp(hostUrl, "HTTP", 4)))         // allow for proto in host URL
    {
        hostUrl = memchr(hostUrl, ':', strlen(hostUrl)) + 3;
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

    strcat(reqstBffr, relativeUrl);
    strcat(reqstBffr, " HTTP/1.1\r\nHost: ");
    strcat(reqstBffr, hostUrl);
    strcat(reqstBffr, "\r\n");

    httpReqst.buffer == reqstBffr;
    httpReqst.headersLen = strlen(httpReqst.buffer);                                      // update object
    return httpReqst;
}


/**
 * @brief Adds common HTTP headers to a custom headers buffer.
 */
void http_addCommonHdrs(httpRequest_t* httpReqst, httpHeaderMap_t headerMap)
{
    ASSERT(headerMap > 0);
    ASSERT(httpReqst->contentLen == 0);                                                     // headers section still open to additions
    ASSERT(*(httpReqst->buffer + httpReqst->headersLen - 2) == '\r');                       // existing request ends in \r\n
    ASSERT(httpReqst->headersLen + http__commonHeadersSz < httpReqst->buffersz);            // "all" headers could fit

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
    ASSERT(httpReqst->contentLen == 0);                                                     // headers section still open to additions
    ASSERT(*(httpReqst->buffer + httpReqst->headersLen - 2) == '\r');                       // existing request ends in \r\n

    strcat(toEncode, user);
    strcat(toEncode, ":");
    strcat(toEncode, pw);
    binToB64(b64str, toEncode, strlen(toEncode));                                           // endcode credentials to Base64 string
    ASSERT(httpReqst->headersLen + strlen(b64str) + 20 < httpReqst->buffersz);              // "Authentication: " + "\r\n" = length 20

    strcat(httpReqst->buffer, "Authentication: ");
    strcat(httpReqst->buffer, b64str);
    strcat(httpReqst->buffer, "\r\n");                                                      // new header ends in correct EOL
    httpReqst->headersLen = strlen(httpReqst->buffer);                                      // update object
}


void http_addHeader(httpRequest_t* httpReqst, const char * keyValue)
{
    ASSERT(keyValue != NULL);
    ASSERT(strnstr(keyValue, ": ", http__maxHeaderKeySz) != NULL);                          // check for HTTP header key/value delimiter, assume key length <= 40
    ASSERT(httpReqst->contentLen == 0);                                                     // headers section still open to additions
    ASSERT(*(httpReqst->buffer + httpReqst->headersLen - 2) == '\r');                       // existing request ends in \r\n

    uint8_t newHdrSz = strlen(keyValue);
    bool missingEOL = false;
    if (memchr(keyValue, '\r', newHdrSz) == NULL)
    {
        newHdrSz += 2;                                                                      // need add EOL (\r\n)
        missingEOL = true;
    }
    ASSERT(httpReqst->headersLen + newHdrSz < httpReqst->buffersz);                         // new header fits
    
    strcat(httpReqst->buffer, keyValue);
    if (missingEOL)
        strcat(httpReqst->buffer, "\r\n");                                                  // add missing EOL
    httpReqst->headersLen = strlen(httpReqst->buffer);                                      // update object
}


/**
 * @brief Helper to compose a generic header and add it to the headers collection being composed.
 */
void http_addHeaderKeyValue(httpRequest_t* httpReqst, const char *key, const char *value)
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


void http_closeHeaders(httpRequest_t* httpReqst, uint32_t contentLength)
{
    if (httpReqst->contentLen == 0)                                                         // finalize/close headers to additional changes
    {
        char wrkBffr[40];
        snprintf(wrkBffr, sizeof(wrkBffr), "Content-Length: %d\r\n\r\n", contentLength);
        strcat(httpReqst->buffer, wrkBffr);
        httpReqst->headersLen = strlen(httpReqst->buffer);
    }
}


/**
 * @brief Helper to compose a generic header and add it to the headers collection being composed.
 */
uint16_t http_addPostData(httpRequest_t* httpReqst, const char *postData, uint16_t postDataSz)
{
    ASSERT(postData != NULL);
    ASSERT(*(httpReqst->buffer + httpReqst->headersLen - 2) == '\r');                       // existing request ends in \r\n

    if (httpReqst->contentLen == 0)                                                         // finalize/close headers to additional changes
    {
        strcat(httpReqst->buffer, "Content-Length:     0\r\n\r\n");
        httpReqst->headersLen = strlen(httpReqst->buffer);
    }

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
    return S__httpGET(httpCtrl, relativeUrl, NULL);
}


/**
 *	@brief Performs a custom (headers) GET request.
 *  -----------------------------------------------------------------------------------------------
 */
resultCode_t http_getCustomRequest(httpCtrl_t *httpCtrl, const char* relativeUrl, httpRequest_t* customRequest)
{
    return S__httpGET(httpCtrl, relativeUrl, customRequest);
}


/**
 *	@brief Performs HTTP GET web request.
 *  -----------------------------------------------------------------------------------------------
 */
static resultCode_t S__httpGET(httpCtrl_t *httpCtrl, const char* relativeUrl, httpRequest_t* customRequest)
{
    httpCtrl->requestState = httpState_idle;
    httpCtrl->httpStatus = resultCode__unknown;
    strcpy(httpCtrl->requestType, "GET");
    resultCode_t rslt;

    ASSERT(httpCtrl != NULL && httpCtrl->streamType == streamType_HTTP);

    if (atcmd_awaitLock(httpCtrl->timeoutSec))
    {
        atcmd_invokeReuseLock("AT+QHTTPCFG=\"responseheader\",%d",  (int)(httpCtrl->responseHdrs));
        rslt = atcmd_awaitResult();
        if (rslt != resultCode__success)
        {
            atcmd_close();
            return rslt;
        }

        if (httpCtrl->useTls)
        {
            // AT+QHTTPCFG="sslctxid",<httpCtrl->sckt>
            atcmd_invokeReuseLock("AT+QHTTPCFG=\"sslctxid\",%d",  (int)httpCtrl->dataCntxt);
            rslt = atcmd_awaitResult();
            if (rslt != resultCode__success)
            {
                atcmd_close();
                return rslt;
            }
        }

        /* SET URL FOR REQUEST
        * set BGx HTTP URL: AT+QHTTPURL=<urlLength>,timeoutSec  (BGx default timeout is 60, if not specified)
        * wait for CONNECT prompt, then output <URL>, /r/n/r/nOK
        * 
        * NOTE: there is only 1 URL in the BGx at a time
        *---------------------------------------------------------------------------------------------------------------*/

        rslt = S__setUrl(httpCtrl->hostUrl, relativeUrl);
        if (rslt != resultCode__success)
        {
            DPRINT(PRNT_WARN, "Failed set URL rslt=%d\r\n", rslt);
            atcmd_close();
            return rslt;
        }

        /* INVOKE HTTP GET METHOD
        * BGx responds with OK immediately upon acceptance of cmd, then later (up to timeout) with "+QHTTPGET: " string
        * After "OK" we switch IOP to data mode and return. S_httpDoWork() handles the parsing of the page response and
        * if successful, the issue of the AT+QHTTPREAD command to start the page data stream
        * 
        * This allows other application tasks to be performed while waiting for page. No LTEm commands can be invoked
        * but non-LTEm tasks like reading sensors can continue.
        *---------------------------------------------------------------------------------------------------------------*/

        uint8_t reqstHdrs = (customRequest != NULL && customRequest->headersLen > 0);
        atcmd_invokeReuseLock("AT+QHTTPCFG=\"requestheader\",%d", reqstHdrs);
        rslt = atcmd_awaitResult();
        if (rslt != resultCode__success)
        {
            atcmd_close();
            return rslt;
        }

        if (reqstHdrs)                                                                                                  // custom HTTP GET request (custom headers)
        {
            strcat(customRequest->buffer, "Content-Length: 0\r\n\r\n");
            customRequest->headersLen = strlen(customRequest->buffer);

            // char dbgBffr[128] = {0};
            // memcpy(dbgBffr, customRequest->buffer, 127);
            // DPRINT(PRNT_MAGENTA, "(S__httpGet) pre-GET reqst bffr %s\r\n", dbgBffr);

            atcmd_configDataMode(httpCtrl, "CONNECT\r\n", atcmd_txHndlrDefault, customRequest->buffer, customRequest->headersLen, NULL, true);
            atcmd_invokeReuseLock("AT+QHTTPGET=%d,%d", httpCtrl->timeoutSec, customRequest->headersLen);
        }
        else                                                                                                            // default HTTP GET request
        {
            atcmd_invokeReuseLock("AT+QHTTPGET=%d", PERIOD_FROM_SECONDS(httpCtrl->timeoutSec));
        }

        atcmd_ovrrdTimeout(PERIOD_FROM_SECONDS(httpCtrl->timeoutSec));
        //atcmd_ovrrdParser(S__httpGetStatusParser);
        atcmd_configParser("+QHTTPGET: ", true, ",", 0, "\r\n", 0);

        rslt = atcmd_awaitResult();                                                                                     // wait for "+QHTTPGET trailer (request completed)

        // char dbgBffr[128] = {0};
        // memcpy(dbgBffr, customRequest->buffer, 127);
        // DPRINT(PRNT_MAGENTA, "(S__httpGet) postGET reqst bffr %s\r\n", dbgBffr);
        // DPRINT(PRNT_MAGENTA, "(S__httpGet) rslt=%d, httpVal=%s, statusCode=%s, response=%s\r\n", rslt, atcmd_getToken(0), atcmd_getToken(1), atcmd_getRawResponse());

        DPRINT_V(PRNT_dMAGENTA, "token_0: %s, token_1: %s\r\n", atcmd_getToken(0), atcmd_getToken(1));
        if (rslt == resultCode__success && (strlen(atcmd_getToken(0)) > 0 && atcmd_getToken(0)[0] == '0'))
        {
            httpCtrl->httpStatus = strtol(atcmd_getToken(1), NULL, 10);
            if (httpCtrl->httpStatus >= resultCode__success && httpCtrl->httpStatus <= resultCode__successMax)
            {
                httpCtrl->requestState = httpState_requestComplete;                                                     // update httpState, got GET/POST response
                DPRINT(PRNT_MAGENTA, "GetRqst dCntxt:%d, status=%d\r\n", httpCtrl->dataCntxt, httpCtrl->httpStatus);
            }
        }
        else
        {
            httpCtrl->requestState = httpState_idle;
            httpCtrl->httpStatus = rslt;
            DPRINT(PRNT_WARN, "Closed failed GET request, status=%d %s\r\n", httpCtrl->httpStatus, atcmd_getErrorDetail());
        }
        atcmd_close();
        return httpCtrl->httpStatus;
    }
    return resultCode__timeout;
}   /* http_get() */


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

    httpCtrl->requestState = httpState_idle;
    httpCtrl->httpStatus = resultCode__unknown;
    strcpy(httpCtrl->requestType, "POST");
    resultCode_t rslt;

    if (atcmd_awaitLock(httpCtrl->timeoutSec))
    {
        atcmd_invokeReuseLock("AT+QHTTPCFG=\"responseheader\",%d",  (int)(httpCtrl->responseHdrs));
        rslt = atcmd_awaitResult();
        if (rslt != resultCode__success)
        {
            atcmd_close();
            return rslt;
        }

        if (httpCtrl->useTls)
        {
            // AT+QHTTPCFG="sslctxid",<httpCtrl->sckt>
            atcmd_invokeReuseLock("AT+QHTTPCFG=\"sslctxid\",%d",  (int)httpCtrl->dataCntxt);
            rslt = atcmd_awaitResult();
            if (rslt != resultCode__success)
            {
                atcmd_close();
                return rslt;
            }
        }

        /* SET URL FOR REQUEST
        * set BGx HTTP URL: AT+QHTTPURL=<urlLength>,timeoutSec  (BGx default timeout is 60, if not specified)
        * wait for CONNECT prompt, then output <URL>, /r/n/r/nOK
        * 
        * NOTE: there is only 1 URL in the BGx at a time
        *---------------------------------------------------------------------------------------------------------------*/

        rslt = S__setUrl(httpCtrl->hostUrl, relativeUrl);
        if (rslt != resultCode__success)
        {
            DPRINT(PRNT_WARN, "Failed set URL rslt=%d\r\n", rslt);
            atcmd_close();
            return rslt;
        }

        atcmd_invokeReuseLock("AT+QHTTPCFG=\"requestheader\",%d", customRequest != NULL);
        rslt = atcmd_awaitResult();
        if (rslt != resultCode__success)
        {
            atcmd_close();
            return rslt;
        }

        if (customRequest != NULL)
        {
            // fixup content-length header value
            char contentLengthVal[6];
            snprintf(contentLengthVal, sizeof(contentLengthVal), "%5d", customRequest->contentLen);
            char* contentLengthPtr = customRequest->buffer + customRequest->headersLen - 9;      // backup over the /r/n/r/n and content-length value
            memcpy(contentLengthPtr, contentLengthVal, 5);

            uint16_t dataLen = customRequest->headersLen + customRequest->contentLen;
            atcmd_configDataMode(httpCtrl, "CONNECT\r\n", atcmd_txHndlrDefault, customRequest->buffer, dataLen, NULL, true);
            atcmd_invokeReuseLock("AT+QHTTPPOST=%d,5,%d", dataLen, httpCtrl->timeoutSec);
        }
        else
        {
            uint16_t dataLength = strlen(postData);
            atcmd_configDataMode(httpCtrl, "CONNECT\r\n", atcmd_txHndlrDefault, postData, dataLength, NULL, true);
            atcmd_invokeReuseLock("AT+QHTTPPOST=%d,5,%d", strlen(postData), httpCtrl->timeoutSec);
        }
        
        atcmd_ovrrdTimeout(PERIOD_FROM_SECONDS(httpCtrl->timeoutSec));
        // atcmd_ovrrdParser(S__httpPostStatusParser);
        atcmd_configParser("+QHTTPPOST: ", true, ",", 0, "\r\n", 0);
        rslt = atcmd_awaitResult();
        if (rslt == resultCode__success)
        {
            // DPRINT(PRNT_MAGENTA, "(S__httpPOST) rslt=%d, httpVal=%s, statusCode=%s, response=%s\r\n", rslt, atcmd_getToken(0), atcmd_getToken(1), atcmd_getResponse());

            DPRINT_V(PRNT_dMAGENTA, "token_0: %s, token_1:%s \r\n", atcmd_getToken(0), atcmd_getToken(1));
            if (rslt == resultCode__success && (strlen(atcmd_getToken(0)) > 0 && atcmd_getToken(0)[0] == '0'))
            {
                httpCtrl->httpStatus = strtol(atcmd_getToken(1), NULL, 10);
                if (httpCtrl->httpStatus >= resultCode__success && httpCtrl->httpStatus <= resultCode__successMax)
                {
                    httpCtrl->requestState = httpState_requestComplete;                                 // update httpState, got GET/POST response
                    DPRINT(PRNT_MAGENTA, "PostRqst dCntxt:%d, status=%d\r\n", httpCtrl->dataCntxt, httpCtrl->httpStatus);
                }
            }
            else
            {
                httpCtrl->requestState = httpState_idle;
                httpCtrl->httpStatus = rslt;
                DPRINT(PRNT_WARN, "Closed failed POST request, status=%d (%s)\r\n", httpCtrl->httpStatus, atcmd_getErrorDetail());
            }
        }
        else
            httpCtrl->httpStatus = resultCode__internalError;
        atcmd_close();
        return httpCtrl->httpStatus;
    }   // awaitLock()

    return resultCode__timeout;
}  // http_post()


/**
 *	@brief Sends contents of a file (LTEM filesystem) as POST to remote.
 */
uint16_t http_postFile(httpCtrl_t *httpCtrl, const char *relativeUrl, const char* filename)
{
    httpCtrl->requestState = httpState_idle;
    httpCtrl->httpStatus = resultCode__unknown;
    strcpy(httpCtrl->requestType, "POST");
    resultCode_t rslt;

    if (atcmd_awaitLock(httpCtrl->timeoutSec))
    {
        atcmd_invokeReuseLock("AT+QHTTPCFG=\"responseheader\",%d",  (int)(httpCtrl->responseHdrs));

        rslt = atcmd_awaitResult();
        if (rslt != resultCode__success)
        {
            atcmd_close();
            return rslt;
        }

        if (httpCtrl->useTls)
        {
            atcmd_invokeReuseLock("AT+QHTTPCFG=\"sslctxid\",%d",  (int)httpCtrl->dataCntxt);
            rslt = atcmd_awaitResult();
            if (rslt != resultCode__success)
            {
                atcmd_close();
                return rslt;
            }
        }

        /* SET URL FOR REQUEST
        * set BGx HTTP URL: AT+QHTTPURL=<urlLength>,timeoutSec  (BGx default timeout is 60, if not specified)
        * wait for CONNECT prompt, then output <URL>, /r/n/r/nOK
        * 
        * NOTE: there is only 1 URL in the BGx at a time
        *---------------------------------------------------------------------------------------------------------------*/

        rslt = S__setUrl(httpCtrl->hostUrl, relativeUrl);
        if (rslt != resultCode__success)
        {
            DPRINT(PRNT_WARN, "Failed set URL rslt=%d\r\n", rslt);
            atcmd_close();
            return rslt;
        }

        /* POST file IS a "custom" request, need set flag for custom request/headers
         */
        atcmd_invokeReuseLock("AT+QHTTPCFG=\"requestheader\",1");
        rslt = atcmd_awaitResult();
        if (rslt != resultCode__success)
        {
            atcmd_close();
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
        atcmd_reset(false);                                                                             // reset atCmd control struct WITHOUT clearing lock
        atcmd_invokeReuseLock("AT+QHTTPPOSTFILE=\"%s\",15", filename);

        atcmd_ovrrdTimeout(PERIOD_FROM_SECONDS(httpCtrl->timeoutSec));
        // atcmd_ovrrdParser(S__httpPostFileStatusParser);
        atcmd_configParser("+QHTTPPOSTFILE: ", true, ",", 0, "\r\n", 0);
        rslt = atcmd_awaitResult();

        if (rslt == resultCode__success)
        {
            ASSERT(atcmd_getToken(0) != NULL);
            ASSERT(atcmd_getToken(1) != NULL);
            // DPRINT(PRNT_MAGENTA, "(http_postFile) rslt=%d, httpVal=%s, statusCode=%s, response=%s\r\n", rslt, atcmd_getToken(0), atcmd_getToken(1), atcmd_getResponse());

            DPRINT_V(PRNT_dMAGENTA, "token_0: %s, token_1: %s\r\n", atcmd_getToken(0), atcmd_getToken(1));
            if (rslt == resultCode__success && (strlen(atcmd_getToken(0)) > 0 && atcmd_getToken(0)[0] == '0'))
            {
                httpCtrl->httpStatus = strtol(atcmd_getToken(1), NULL, 10);
                if (httpCtrl->httpStatus >= resultCode__success && httpCtrl->httpStatus <= resultCode__successMax)
                {
                    httpCtrl->requestState = httpState_requestComplete;                                 // update httpState, got GET/POST response
                    DPRINT(PRNT_MAGENTA, "Post(file) Request dCntxt:%d, status=%d\r\n", httpCtrl->dataCntxt, httpCtrl->httpStatus);
                }
            }
            else
            {
                httpCtrl->requestState = httpState_idle;
                httpCtrl->httpStatus = rslt;
                DPRINT(PRNT_WARN, "Closed failed POST(file) request, status=%d (%s)\r\n", httpCtrl->httpStatus, atcmd_getErrorDetail());
            }
        }
        else
            httpCtrl->httpStatus = resultCode__internalError;
        atcmd_close();
        return httpCtrl->httpStatus;
    }   // awaitLock()

    return resultCode__timeout;
}  // http_postFile()


/**
 * @brief Retrieves page results from a previous GET or POST.
 * @return HTTP status code from server
 */
uint16_t http_readPage(httpCtrl_t *httpCtrl)
{
    ASSERT(httpCtrl != NULL && httpCtrl->dataRxHndlr != NULL);

    resultCode_t rslt;

    if (httpCtrl->requestState != httpState_requestComplete)
        return resultCode__preConditionFailed;                                  // readPage() only valid after a completed GET\POST
    
    bBuffer_t* rxBffr = g_lqLTEM.iop->rxBffr;                                   // for better readability
    char* workPtr;

    atcmd_configDataMode(httpCtrl, "CONNECT\r\n", S__httpRxHandler, NULL, 0, httpCtrl->appRcvrCB, false);
    if (atcmd_tryInvoke("AT+QHTTPREAD=%d", httpCtrl->timeoutSec))
    {
        return atcmd_awaitResult();                                             // dataHandler will be invoked by atcmd module and return a resultCode
    }
    return resultCode__conflict;
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
    
    if (atcmd_tryInvoke("AT+QHTTPREADFILE=\"%s\",%d", filename, http__readToFileInterPcktTimeoutSec))
    {
        atcmd_ovrrdTimeout(SEC_TO_MS(http__readToFileTimeoutSec));
        // atcmd_ovrrdParser(S__httpReadFileStatusParser);
        atcmd_configParser("+QHTTPREADFILE: ", true, ",", 0, "\r\n", 0);
        resultCode_t rslt = atcmd_awaitResult();
        if (IS_SUCCESS(rslt))
        {
            if (strlen(atcmd_getRawResponse()) > sizeof("AT+QHTTPREADFILE: 0") && *atcmd_getResponse() == '0')
                return resultCode__success;
            else
                return resultCode__internalError;
        }
        return rslt;
    }
    return resultCode__conflict;
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
static resultCode_t S__setUrl(const char *host, const char *relative)
{
    uint16_t rslt;
    bool urlSet = false;
    char url[240] = {0};
    
    strcpy(url, host);
    if (strlen(relative) > 0)                                                                       // need to concat relative/query
    {
        strcat(url, relative);
    }
    DPRINT(PRNT_dMAGENTA, "URL(%d)=%s", strlen(url), url);
    DPRINT(PRNT_dMAGENTA, "\r\n");                                                                  // separate line-end if URL truncates in trace
    
    atcmd_configDataMode(0, "CONNECT\r\n", atcmd_txHndlrDefault, url, strlen(url), NULL, false);    // setup for URL dataMode transfer 
    atcmd_invokeReuseLock("AT+QHTTPURL=%d,5", strlen(url));
    rslt = atcmd_awaitResult();
    return rslt;
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


// /**
//  * @brief Once the result is obtained, this function extracts the HTTP status value from the response
//  */
// static uint16_t S__parseResponseForHttpStatus(httpCtrl_t *httpCtrl, const char *response)
// {
//     char *continueAt = strchr(response, ',');                               // skip ',' and parse http status
//     if (continueAt)
//     {
//         httpCtrl->httpStatus = strtol(++continueAt, &continueAt, 10);
//         httpCtrl->pageSize = strtol(++continueAt, &continueAt, 10);         // skip next ',' and parse content length

//         httpCtrl->pageRemaining = 0;
//         if (httpCtrl->pageSize > 0)
//             httpCtrl->pageRemaining = httpCtrl->pageSize;                   // read() will decrement this
//     }
//     else
//         httpCtrl->httpStatus = resultCode__preConditionFailed;
//     return httpCtrl->httpStatus;
// }

/**
 * @brief Handles the READ data flow from the BGx (via rxBffr) to app
 */
static resultCode_t S__httpRxHandler()
{
    char wrkBffr[32];
    uint16_t pageRslt = 0;

    httpCtrl_t *httpCtrl = (httpCtrl_t*)g_lqLTEM.atcmd->dataMode.streamCtrl;
    ASSERT(httpCtrl != NULL);                                                                           // ASSERT data mode and stream context are consistent

    uint8_t popCnt = bbffr_find(g_lqLTEM.iop->rxBffr, "\r", 0, 0, false);
    if (BBFFR_ISNOTFOUND(popCnt))
    {
        return resultCode__internalError;
    }
    
    bbffr_pop(g_lqLTEM.iop->rxBffr, wrkBffr, popCnt + 2);                                               // pop CONNECT phrase for parsing data length
    DPRINT(PRNT_CYAN, "httpPageRcvr() stream started\r\n");

    memset(wrkBffr, 0, sizeof(wrkBffr));                                                                // need clean wrkBffr for trailer parsing
    uint32_t readStart = pMillis();
    do
    {
        uint16_t occupiedCnt = bbffr_getOccupied(g_lqLTEM.iop->rxBffr);
        bool readTimeout = pMillis() - readStart > httpCtrl->timeoutSec;
        uint16_t trailerIndx = bbffr_find(g_lqLTEM.iop->rxBffr, "\r\nOK\r\n\r\n", 0, 0, false);
        uint16_t reqstBlockSz = MIN(trailerIndx, httpCtrl->defaultBlockSz);

        if (bbffr_getOccupied(g_lqLTEM.iop->rxBffr) >= reqstBlockSz)                                        // sufficient read content ready
        {
            char* streamPtr;
            uint16_t blockSz = bbffr_popBlock(g_lqLTEM.iop->rxBffr, &streamPtr, reqstBlockSz);              // get address from rxBffr
            DPRINT(PRNT_CYAN, "httpPageRcvr() ptr=%p blkSz=%d isFinal=%d\r\n", streamPtr, blockSz, BBFFR_ISFOUND(trailerIndx));

            // forward to application
            ((httpAppRcvr_func)(*httpCtrl->appRcvrCB))(httpCtrl->dataCntxt, streamPtr, blockSz, BBFFR_ISFOUND(trailerIndx));
            bbffr_popBlockFinalize(g_lqLTEM.iop->rxBffr, true);                                             // commit POP
        }

        if (BBFFR_ISFOUND(trailerIndx))
        {
            // parse trailer for status 
            uint8_t offset = strlen(wrkBffr);
            bbffr_pop(g_lqLTEM.iop->rxBffr, wrkBffr + offset, sizeof(wrkBffr) - offset);

            if (strchr(wrkBffr, '\n'))                                                                      // wait for final /r/n in wrkBffr
            {
                char* suffix = strstr(wrkBffr, "+QHTTPREAD: ") + sizeof("+QHTTPREAD: ");
                uint16_t errVal = strtol(suffix, NULL, 10);
                if (errVal == 0)
                {
                    return resultCode__success;
                }
                else
                {
                    pageRslt = errVal;
                    // to be translated like file results
                    return pageRslt;
                }
            }
        }
    } while (true);
}


void http_logWriteRequest(httpRequest_t* httpReqst, uint16_t bffrSz)
{
    const char* workPtr = httpReqst->buffer;
    uint32_t cLen = httpReqst->contentLen;
    char lineBffr[128];

    emLog_printf(PRNT_dMAGENTA, "HTTP Request\r\n------------------------------");

    char* hdrEOL = strnstr(httpReqst, "\r\n\r\n", bffrSz);
    char* eol;
    uint8_t lineSz;

    if (hdrEOL != NULL)
    {
        while (workPtr < hdrEOL)
        {
            eol = (char*)memchr(workPtr, '\r', sizeof(lineBffr));
            lineSz = MIN(eol - workPtr, sizeof(lineBffr)-1);

            strncpy(lineBffr, workPtr, lineSz);
            emLog_printf(" - %s\r\n", lineBffr);

            if (eol == hdrEOL)
            {
                workPtr = eol + 4;
                for (size_t i = 0; i < 5; i++)             // output up to 5 lines
                {

                    lineSz = MIN(cLen, sizeof(lineBffr)-1);
                    strncpy(lineBffr, workPtr, lineSz);
                    emLog_printf(PRNT_dMAGENTA, " > %s\r\n", lineBffr);
                    cLen -= lineSz;
                }
            }
            workPtr = eol + 2;                              // next header line
        }
    }
    else
        emLog_printf(PRNT_dMAGENTA, "Malformed request: no header separator\r\n");

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