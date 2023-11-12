/** ****************************************************************************
  \file 
  \brief Public API providing HTTP/HTTPS support
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


#define SRCFILE "HTT"                       // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT
#define ENABLE_DIAGPRINT                    // expand DPRINT into debug output
//#define ENABLE_DIAGPRINT_VERBOSE            // expand DPRINT and DPRINT_V into debug output
#define ENABLE_ASSERT
#include <lqdiag.h>

#include "ltemc-internal.h"
#include "ltemc-http.h"

extern ltemDevice_t g_lqLTEM;

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))


/* Local Static Functions
------------------------------------------------------------------------------------------------------------------------- */
// static void S_httpDoWork();
static uint16_t S__parseResponseForHttpStatus(httpCtrl_t *httpCtrl, const char *responseTail);
static uint16_t S__setUrl(const char *host, const char *relative);
static cmdParseRslt_t S__httpGetStatusParser();
static cmdParseRslt_t S__httpPostStatusParser();
static cmdParseRslt_t S__httpReadFileStatusParser();
static resultCode_t S__httpRxHndlr();


/* Public Functions
------------------------------------------------------------------------------------------------------------------------- */

/**
 *	@brief Create a HTTP(s) control structure to manage web communications. 
 */
void http_initControl(httpCtrl_t *httpCtrl, dataCntxt_t dataCntxt, httpRecv_func recvCallback)
{
    ASSERT(httpCtrl != NULL && recvCallback != NULL);
    ASSERT(dataCntxt < dataCntxt__cnt);

    g_lqLTEM.streams[dataCntxt] = httpCtrl;

    memset(httpCtrl, 0, sizeof(httpCtrl_t));
    httpCtrl->dataCntxt = dataCntxt;
    httpCtrl->streamType = streamType_HTTP;
    httpCtrl->appRecvDataCB = recvCallback;
    httpCtrl->dataRxHndlr = S__httpRxHndlr;

    httpCtrl->requestState = httpState_idle;
    httpCtrl->httpStatus = resultCode__unknown;
    httpCtrl->pageCancellation = false;
    httpCtrl->useTls = false;
    httpCtrl->timeoutSec = http__defaultTimeoutBGxSec;
    httpCtrl->defaultBlockSz = bbffr_getCapacity(g_lqLTEM.iop->rxBffr) / 4;
    httpCtrl->cstmHdrs = NULL;
    httpCtrl->cstmHdrsSz = 0;
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
 *	@brief Registers custom headers (char) buffer with HTTP control.
 */
void http_enableCustomHdrs(httpCtrl_t *httpCtrl, char *headerBuf, uint16_t headerBufSz)
{
    ASSERT(strlen(httpCtrl->hostUrl));                                                // host connection initialized
    ASSERT(headerBuf != NULL);                                                        // valid header buffer pointer 
    // Warn on small header buffer, less than likely use for 1 or 2 small headers
    ASSERT_W(headerBufSz > (strlen(httpCtrl->hostUrl) + http__customHdrSmallWarning), "CustomHdr diminutive buffer sz");

    memset(headerBuf, 0, headerBufSz);
    httpCtrl->cstmHdrs = headerBuf;
    httpCtrl->cstmHdrsSz = headerBufSz;
}


/**
 *	@brief Adds common http headers to the custom headers buffer. REQUIRES previous enable of custom headers and buffer registration.
 */
void http_addCommonHdrs(httpCtrl_t *httpCtrl, httpHeaderMap_t headerMap)
{
    ASSERT(httpCtrl->cstmHdrs != NULL);

    bool addedHdrsFit = true;

    if (headerMap & httpHeaderMap_accept > 0 || headerMap == httpHeaderMap_all)
    {
        if (strlen(httpCtrl->cstmHdrs) + 13 < httpCtrl->cstmHdrsSz)                     // 13 = strlen("Accept: */*\r\n")
            strcat(httpCtrl->cstmHdrs, "Accept: */*\r\n");
        else
            addedHdrsFit = false;
    }
    if (headerMap & httpHeaderMap_userAgent > 0 || headerMap == httpHeaderMap_all)
    {
        if (strlen(httpCtrl->cstmHdrs) + 25 < httpCtrl->cstmHdrsSz)
            strcat(httpCtrl->cstmHdrs, "User-Agent: QUECTEL_BGx\r\n");
        else
            addedHdrsFit = false;
    }
    if (headerMap & httpHeaderMap_connection > 0 || headerMap == httpHeaderMap_all)
    {
        if (strlen(httpCtrl->cstmHdrs) + 24 < httpCtrl->cstmHdrsSz)
            strcat(httpCtrl->cstmHdrs, "Connection: Keep-Alive\r\n");
        else
            addedHdrsFit = false;
    }
    if (headerMap & httpHeaderMap_contentType > 0 || headerMap == httpHeaderMap_all)
    {
        if (strlen(httpCtrl->cstmHdrs) + 40 < httpCtrl->cstmHdrsSz)
            strcat(httpCtrl->cstmHdrs, "Content-Type: application/octet-stream\r\n");
        else
            addedHdrsFit = false;
    }

    ASSERT(addedHdrsFit);
}



void http_addCustomHdr(httpCtrl_t *httpCtrl, const char *hdrText)
{
    ASSERT(httpCtrl->cstmHdrs != NULL);
    ASSERT(strlen(httpCtrl->cstmHdrs) + strlen(hdrText) + 2 < httpCtrl->cstmHdrsSz);

    strcat(httpCtrl->cstmHdrs, hdrText);
    strcat(httpCtrl->cstmHdrs, "\r\n");
}


/**
 *	@brief Adds a basic authorization header to the custom headers buffer, requires previous custom headers buffer registration.
 */
void http_addBasicAuthHdr(httpCtrl_t *http, const char *user, const char *pw)
{
    ASSERT(http->cstmHdrs != NULL);

    char toEncode[80];
    char b64str[120];

    strcat(toEncode, user);
    strcat(toEncode, ":");
    strcat(toEncode, pw);
    // endcode to Base64 string
    binToB64(b64str, toEncode, strlen(toEncode));

    ASSERT(strlen(http->cstmHdrs) + 19 + strlen(b64str) < http->cstmHdrsSz);

    strcat(http->cstmHdrs, "Authentication: ");
    strcat(http->cstmHdrs, b64str);
    strcat(http->cstmHdrs, "\r\n");
}


/* ------------------------------------------------------------------------------------------------
 *  Request and Response Section 
 * --------------------------------------------------------------------------------------------- */

/**
 *	@brief Perform HTTP GET operation. Results are internally buffered on the LTEm, see http_read().
 *  -----------------------------------------------------------------------------------------------
 */
resultCode_t http_get(httpCtrl_t *httpCtrl, const char* relativeUrl, bool returnResponseHdrs)
{
    httpCtrl->requestState = httpState_idle;
    httpCtrl->httpStatus = resultCode__unknown;
    strcpy(httpCtrl->requestType, "GET");
    resultCode_t _rslt;

    // if (ATCMD_awaitLock(httpCtrl->timeoutSec))
    // {

        if (returnResponseHdrs)
        {
            if (!atcmd_tryInvoke("AT+QHTTPCFG=\"responseheader\",%d",  (int)(httpCtrl->returnResponseHdrs)))
                return resultCode__conflict;

            if (IS_SUCCESS_RSLT(atcmd_awaitResult()))
                return _rslt;
        }

        if (httpCtrl->useTls)
        {
            if (!atcmd_tryInvoke("AT+QHTTPCFG=\"sslctxid\",%d",  (int)httpCtrl->dataCntxt))
                return resultCode__conflict;

            if (IS_SUCCESS_RSLT(atcmd_awaitResult()))
                return _rslt;
        }

        /* SET URL FOR REQUEST
        * set BGx HTTP URL: AT+QHTTPURL=<urlLength>,timeoutSec  (BGx default timeout is 60, if not specified)
        * wait for CONNECT prompt, then output <URL>, /r/n/r/nOK
        * 
        * NOTE: there is only 1 URL in the BGx at a time
        *---------------------------------------------------------------------------------------------------------------*/

        if (IS_SUCCESS_RSLT(S__setUrl(httpCtrl->hostUrl, relativeUrl)))
        {
            DPRINT(PRNT_WARN, "Failed set URL rslt=%d\r", _rslt);
            return _rslt;
        }

        /* INVOKE HTTP GET METHOD
        * BGx responds with OK immediately upon acceptance of cmd, then later (up to timeout) with "+QHTTPGET: " string
        * After "OK" we switch IOP to data mode and return. S_httpDoWork() handles the parsing of the page response and
        * if successful, the issue of the AT+QHTTPREAD command to start the page data stream
        * 
        * This allows other application tasks to be performed while waiting for page. No LTEm commands can be invoked
        * but non-LTEm tasks like reading sensors can continue.
        *---------------------------------------------------------------------------------------------------------------*/

        /* If custom headers, need to both set flag here and include in request stream below
         */
        if (!atcmd_tryInvoke("AT+QHTTPCFG=\"requestheader\",%d", httpCtrl->cstmHdrs ? 1 : 0))
            return resultCode__conflict;

        if (IS_SUCCESS_RSLT(atcmd_awaitResult()))
            return _rslt;

        atcmd_ovrrdTimeout(SEC_TO_MS(httpCtrl->timeoutSec));
        atcmd_ovrrdParser(S__httpGetStatusParser);

        if (httpCtrl->cstmHdrs)
        {
            char *hostName = strchr(httpCtrl->hostUrl, ':');
            hostName = hostName ? hostName + 3 : httpCtrl->hostUrl;
            DPRINT(PRNT_dMAGENTA, "\r\nHost: %s\r\n", hostName);

            char customRqst[http__getRequestSz];
            snprintf(customRqst, sizeof(customRqst), "%s %s HTTP/1.1\r\nHost: %s\r\n%s\r\n", httpCtrl->requestType, relativeUrl, hostName, httpCtrl->cstmHdrs);
            DPRINT(PRNT_dMAGENTA, "CustomHdrs\r\n");
            DPRINT_V(PRNT_dMAGENTA, "%s\r\n", customRqst);

            atcmd_configDataMode(httpCtrl->dataCntxt, "CONNECT", atcmd_stdTxDataHndlr, customRqst, strlen(customRqst), NULL, true);
            if (!atcmd_tryInvoke("AT+QHTTPGET=%d,%d", httpCtrl->timeoutSec, strlen(customRqst)))
                return resultCode__conflict;
        }
        else
        {
            if (!atcmd_tryInvoke("AT+QHTTPGET=%d", PERIOD_FROM_SECONDS(httpCtrl->timeoutSec)))
                return resultCode__conflict;
        }

        _rslt = atcmd_awaitResult();                                             // wait for "+QHTTPGET trailer (request completed)
        const char* token = atcmd_getToken(1);
        long tokenVal = strtol(token, NULL, 10);

        if (_rslt == resultCode__success && tokenVal == 0)
        {
            httpCtrl->httpStatus = S__parseResponseForHttpStatus(httpCtrl, atcmd_getResponseData());
            if (httpCtrl->httpStatus >= resultCode__success && httpCtrl->httpStatus <= resultCode__successMax)
            {
                httpCtrl->requestState = httpState_requestComplete;                                         // update httpState, got GET/POST response
                DPRINT(PRNT_MAGENTA, "[http_get()] dCntxt:%d, status=%d\r", httpCtrl->dataCntxt, httpCtrl->httpStatus);
            }
        }
        else
        {
            httpCtrl->requestState = httpState_idle;
            httpCtrl->httpStatus = atcmd_getValue();
            DPRINT(PRNT_WARN, "[http_get()] FAILED, status=%d %s\r", httpCtrl->httpStatus, atcmd_getErrorDetail());
        }
        return httpCtrl->httpStatus;
}   // http_get()



/**
 *	@brief Performs a HTTP POST page web request.
 *  -----------------------------------------------------------------------------------------------
 */
resultCode_t http_post(httpCtrl_t *httpCtrl, const char *relativeUrl, bool returnResponseHdrs, const char *postData, uint16_t postDataSz)
{
    httpCtrl->requestState = httpState_idle;
    httpCtrl->httpStatus = resultCode__unknown;
    strcpy(httpCtrl->requestType, "POST");
    resultCode_t _rslt;

    if (returnResponseHdrs)
    {
        if (!atcmd_tryInvoke("AT+QHTTPCFG=\"responseheader\",%d",  (int)(httpCtrl->returnResponseHdrs)))
            return resultCode__conflict;
        if (IS_SUCCESS_RSLT(atcmd_awaitResult()))
            return _rslt;
    }

    if (httpCtrl->useTls)
    {
        if (!atcmd_tryInvoke("AT+QHTTPCFG=\"sslctxid\",%d",  (int)httpCtrl->dataCntxt))
            return resultCode__conflict;
        if (IS_SUCCESS_RSLT(atcmd_awaitResult()))
            return _rslt;
    }

    /* SET URL FOR REQUEST
    * set BGx HTTP URL: AT+QHTTPURL=<urlLength>,timeoutSec  (BGx default timeout is 60, if not specified)
    * wait for CONNECT prompt, then output <URL>, /r/n/r/nOK
    * 
    * NOTE: there is only 1 URL in the BGx at a time
    *---------------------------------------------------------------------------------------------------------------*/

    if (IS_SUCCESS_RSLT(S__setUrl(httpCtrl->hostUrl, relativeUrl)))
    {
        DPRINT(PRNT_WARN, "Failed set URL rslt=%d\r", _rslt);
        return _rslt;
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

    uint16_t httpRequestLen = postDataSz;                                                           // requestLen starts with postDataSz
    httpRequestLen += (httpCtrl->cstmHdrsSz > 0) ? (httpCtrl->cstmHdrsSz + 2) : 0;                  // add the custom headers + 2 char for EOL after hdrs

    atcmd_configDataMode(httpCtrl->dataCntxt, "CONNECT", atcmd_stdTxDataHndlr, postData, postDataSz, NULL, true);
    atcmd_ovrrdTimeout(SEC_TO_MS(httpCtrl->timeoutSec));
    atcmd_ovrrdParser(S__httpPostStatusParser);

    if (httpCtrl->cstmHdrsSz)
    {
        if(!atcmd_tryInvoke("AT+QHTTPPOST=%d,5,%d", httpRequestLen, httpCtrl->timeoutSec))
            return resultCode__conflict;
    }
    else
    {
        if (!atcmd_tryInvoke("AT+QHTTPPOST=%d,5,%d", postDataSz, httpCtrl->timeoutSec))
            return resultCode__conflict;
    }

    if (IS_SUCCESS_RSLT(atcmd_awaitResult()))
    {
        const char* token = atcmd_getToken(1);
        long tokenVal = strtol(token, NULL, 10);

        if (tokenVal == 0)                                   // wait for "+QHTTPPOST trailer: rslt=200, postErr=0
        {
            httpCtrl->httpStatus = S__parseResponseForHttpStatus(httpCtrl, atcmd_getResponseData());
            if (httpCtrl->httpStatus >= resultCode__success && httpCtrl->httpStatus <= resultCode__successMax)
            {
                httpCtrl->requestState = httpState_requestComplete;                                 // update httpState, got GET/POST response
                DPRINT(PRNT_MAGENTA, "PostRqst dCntxt:%d, status=%d\r", httpCtrl->dataCntxt, httpCtrl->httpStatus);
                atcmd_close();
                return _rslt;
            }
        }

        if (httpCtrl->useTls)
        {
            // AT+QHTTPCFG="sslctxid",<httpCtrl->sckt>
            atcmd_invokeReuseLock("AT+QHTTPCFG=\"sslctxid\",%d",  (int)httpCtrl->dataCntxt);
            _rslt = atcmd_awaitResult();
            if (_rslt != resultCode__success)
            {
                return _rslt;
            }
        }

        /* SET URL FOR REQUEST
        * set BGx HTTP URL: AT+QHTTPURL=<urlLength>,timeoutSec  (BGx default timeout is 60, if not specified)
        * wait for CONNECT prompt, then output <URL>, /r/n/r/nOK
        * 
        * NOTE: there is only 1 URL in the BGx at a time
        *---------------------------------------------------------------------------------------------------------------*/

        _rslt = S__setUrl(httpCtrl->hostUrl, relativeUrl);
        if (_rslt != resultCode__success)
        {
            DPRINT(PRNT_WARN, "Failed set URL rslt=%d\r\n", _rslt);
            return _rslt;
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

        uint16_t httpRequestLen = postDataSz;                                                           // requestLen starts with postDataSz
        httpRequestLen += (httpCtrl->cstmHdrsSz > 0) ? (httpCtrl->cstmHdrsSz + 2) : 0;                  // add the custom headers + 2 char for EOL after hdrs

        atcmd_configDataMode(httpCtrl->dataCntxt, "CONNECT", atcmd_stdTxDataHndlr, postData, postDataSz, NULL, true);

        if (httpCtrl->cstmHdrsSz)
        {
            atcmd_invokeReuseLock("AT+QHTTPPOST=%d,5,%d", httpRequestLen, httpCtrl->timeoutSec);
        }
        else
        {
            atcmd_invokeReuseLock("AT+QHTTPPOST=%d,5,%d", postDataSz, httpCtrl->timeoutSec);
        }

        _rslt = atcmd_awaitResultWithOptions(PERIOD_FROM_SECONDS(httpCtrl->timeoutSec), S__httpPostStatusParser);
        if (_rslt == resultCode__success)
        {
            // atcmd_reset(false);                                                                         // clear CONNECT event from atcmd results
            // atcmd_sendCmdData(postData, postDataSz);
            // rslt = atcmd_awaitResultWithOptions(httpCtrl->timeoutSec, S__httpPostStatusParser);
            if (_rslt == resultCode__success && atcmd_getValue() == 0)                                   // wait for "+QHTTPPOST trailer: rslt=200, postErr=0
            {
                httpCtrl->httpStatus = S__parseResponseForHttpStatus(httpCtrl, atcmd_getResponseData());
                if (httpCtrl->httpStatus >= resultCode__success && httpCtrl->httpStatus <= resultCode__successMax)
                {
                    httpCtrl->requestState = httpState_requestComplete;                                 // update httpState, got GET/POST response
                    DPRINT(PRNT_MAGENTA, "PostRqst dCntxt:%d, status=%d\r\n", httpCtrl->dataCntxt, httpCtrl->httpStatus);
                }
            }
            else
            {
                httpCtrl->requestState = httpState_idle;
                httpCtrl->httpStatus = _rslt;
                DPRINT(PRNT_WARN, "Closed failed POST request, status=%d (%s)\r", httpCtrl->httpStatus, atcmd_getErrorDetail());
            }
        }
        else
        {
            httpCtrl->requestState = httpState_idle;
            httpCtrl->httpStatus = _rslt;
            DPRINT(PRNT_WARN, "Closed failed POST request, status=%d (%s)\r", httpCtrl->httpStatus, atcmd_getErrorDetail());
        }
    }
    else
        httpCtrl->httpStatus = resultCode__internalError;

    return httpCtrl->httpStatus;
}  // http_post()



/**
 * @brief Retrieves page results from a previous GET or POST.
 * @return HTTP status code from server
 */
uint16_t http_readPage(httpCtrl_t *httpCtrl)
{
    resultCode_t rslt;

    if (httpCtrl->requestState != httpState_requestComplete)
        return resultCode__preConditionFailed;                                  // readPage() only valid after a completed GET\POST
    
    bBuffer_t* rxBffr = g_lqLTEM.iop->rxBffr;                                   // for better readability
    char* workPtr;

    if (atcmd_tryInvoke("AT+QHTTPREAD=%d", httpCtrl->timeoutSec))
    {
        atcmd_configDataMode(httpCtrl->dataCntxt, "CONNECT", S__httpRxHndlr, NULL, 0, httpCtrl->appRecvDataCB, false);
        // atcmd_setStreamControl("CONNECT", (streamCtrl_t*)httpCtrl);
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
    ASSERT(strlen(filename) < http__readToFileNameSzMax);

    if (httpCtrl->requestState != httpState_requestComplete)
        return resultCode__preConditionFailed;                                  // readPage() only valid after a completed GET\POST
    
    atcmd_ovrrdTimeout(SEC_TO_MS(httpCtrl->timeoutSec));
    atcmd_ovrrdParser(S__httpReadFileStatusParser);
    if (!atcmd_tryInvoke("AT+QHTTPREADFILE=\"%s\",%d", filename, httpCtrl->timeoutSec))
        return resultCode__conflict;

    return atcmd_awaitResult();
}


/**
 * @brief Not currently implemented
 */
void http_cancelPage(httpCtrl_t *httpCtrl)
{
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
    if (strlen(relative) > 0)                                                                   // need to concat relative/query
    {
        strcat(url, relative);

        // char hostSuffix = host[strlen(url) - 1];                                                // being flexible on host suffix and relative prefix when joining 
        // if (hostSuffix != '/' &&                                                                // need to supply '/' to join
        //     relative[0] != '/')                      
        // {
        //     strcat(url, "/");
        //     strcat(url, relative);
        // }
        // else if (hostSuffix == '/' &&                                                           // both have '/', need to trim to join
        //     relative[0] == '/')                      
        // {
        //     strcat(url, relative + 1);                                                          // skip 1st char '/' 
        // }
        // else
        // {
        //     strcat(url, relative);                                                              // host suffix is not '/' and relative prefix is '/'
        // }
    }
    DPRINT(PRNT_dMAGENTA, "URL(%d)=\"%s\" \r\n", strlen(url), url);
    
    atcmd_configDataMode(0, "CONNECT", atcmd_stdTxDataHndlr, url, strlen(url), NULL, false);        // setup for URL dataMode transfer 
    atcmd_tryInvoke("AT+QHTTPURL=%d,5", strlen(url));
    rslt = atcmd_awaitResult();
    return rslt;
}


/**
 * @brief Once the result is obtained, this function extracts the HTTP status value from the response
 */
static uint16_t S__parseResponseForHttpStatus(httpCtrl_t *httpCtrl, const char *response)
{
    char *continueAt = strchr(response, ',');                               // skip ',' and parse http status
    if (continueAt)
    {
        httpCtrl->httpStatus = strtol(++continueAt, &continueAt, 10);
        httpCtrl->pageSize = strtol(++continueAt, &continueAt, 10);         // skip next ',' and parse content length

        httpCtrl->pageRemaining = 0;
        if (httpCtrl->pageSize > 0)
            httpCtrl->pageRemaining = httpCtrl->pageSize;                   // read() will decrement this
    }
    else
        httpCtrl->httpStatus = resultCode__preConditionFailed;
    return httpCtrl->httpStatus;
}

/**
 * @brief Handles the READ data flow from the BGx (via rxBffr) to app
 */
static resultCode_t S__httpRxHndlr()
{
    char wrkBffr[32];
    uint16_t pageRslt = 0;

    httpCtrl_t *httpCtrl = (httpCtrl_t*)ltem_getStreamFromCntxt(g_lqLTEM.atcmd->dataMode.contextKey, streamType_HTTP);
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
            ((httpRecv_func)(*httpCtrl->appRecvDataCB))(httpCtrl->dataCntxt, streamPtr, blockSz, BBFFR_ISFOUND(trailerIndx));
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

#pragma endregion


#pragma Static Response Parsers

/* Note for parsers below:
 * httprspcode is only reported if err is 0, have to search for finale (\r\n) after a preamble (parser speak)
 * --------------------------------------------------------------------------------------------- */

static cmdParseRslt_t S__httpGetStatusParser() 
{
    // +QHTTPGET: <err>[,<httprspcode>[,<content_length>]]
    return atcmd_stdResponseParser("+QHTTPGET: ", true, ",", 0, 1, "\r\n", 0);
}


static cmdParseRslt_t S__httpPostStatusParser() 
{
    // +QHTTPPOST: <err>[,<httprspcode>[,<content_length>]] 
    return atcmd_stdResponseParser("+QHTTPPOST: ", true, ",", 0, 1, "\r\n", 0);
}


static cmdParseRslt_t S__httpReadFileStatusParser() 
{
    // +QHTTPPOST: <err>[,<httprspcode>[,<content_length>]] 
    return atcmd_stdResponseParser("+QHTTPREADFILE: ", true, ",", 0, 0, "\r\n", 0);
}

#pragma endregion