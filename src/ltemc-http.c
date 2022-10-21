/******************************************************************************
 *  \file ltemc-http.c
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
 * HTTP(S) Request Support (GET\POST)
 *****************************************************************************/

#define _DEBUG 0                        // set to non-zero value for PRINTF debugging output, 
// debugging output options             // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if defined(_DEBUG) && _DEBUG > 0
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG == 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #define PRINTF(c_,f_,__VA_ARGS__...) do { rtt_printf(c_, (f_), ## __VA_ARGS__); } while(0)
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#endif

#include "ltemc-http.h"
#include "ltemc-internal.h"
#include <lq-str.h>

#define MIN(x, y) (((x) < (y)) ? (x) : (y))


extern ltemDevice_t g_lqLTEM;

/* Local Static Functions
------------------------------------------------------------------------------------------------------------------------- */
// static void S_httpDoWork();
static uint16_t S_parseRequestResponse(httpCtrl_t *httpCtrl, const char *responseTail);
static uint16_t S_setUrl(const char *url, uint16_t timeoutSec);
static cmdParseRslt_t S_httpGetStatusParser(const char *response, char **endptr);
static cmdParseRslt_t S_httpPostStatusParser(const char *response, char **endptr);


/* Public Functions
------------------------------------------------------------------------------------------------------------------------- */

/**
 *	@brief Create a HTTP(s) control structure to manage web communications. 
 */
void http_initControl(httpCtrl_t *httpCtrl, socket_t sckt, char *recvBuf, uint16_t recvBufSz, httpRecvFunc_t recvCallback)
{
    ASSERT(httpCtrl != NULL && recvBuf != NULL && recvCallback != NULL, srcfile_ltemc_http_c);
    ASSERT(sckt < socket__cnt, srcfile_ltemc_sckt_c);

    memset(httpCtrl, 0, sizeof(httpCtrl_t));

    httpCtrl->ctrlMagic = streams__ctrlMagic;
    httpCtrl->sckt = sckt;
    httpCtrl->protocol = protocol_http;
    httpCtrl->requestState = httpState_idle;
    httpCtrl->httpStatus = resultCode__unknown;
    httpCtrl->pageCancellation = false;
    httpCtrl->useTls = ((httpCtrl->urlHost)[4] == 'S' || (httpCtrl->urlHost)[4] == 's');

    uint16_t bufferSz = IOP_initRxBufferCtrl(&(httpCtrl->recvBufCtrl), recvBuf, recvBufSz);

    ASSERT_W(recvBufSz == bufferSz, srcfile_ltemc_http_c, "Http RxBufSz not *of 128b");
    ASSERT(bufferSz > 64, srcfile_ltemc_http_c);

    httpCtrl->dataRecvCB = recvCallback;
    httpCtrl->cstmHdrs = NULL;
    httpCtrl->cstmHdrsSz = 0;
    httpCtrl->httpStatus = 0xFFFF;

    // LTEM_registerDoWorker(S_httpDoWork);                                // register the HTTP background worker with ltem_doWork()
}

/**
 *	@brief Set host connection characteristics. 
 *  @param httpCtrl [in] HTTP control structure pointer, struct defines parameters of communications with web server.
 *  @param hostURL [in] The HOST address of the web server URL.
 *  @param hostPort [in] The port number for the host web server.
 */
void http_setConnection(httpCtrl_t *httpCtrl, const char* hostUrl, uint16_t hostPort)
{
    ASSERT(strncmp(hostUrl, "HTTP", 4) == 0 || strncmp(hostUrl, "http", 4) == 0, srcfile_ltemc_http_c);

    strncpy(httpCtrl->urlHost, hostUrl, sizeof(httpCtrl->urlHost));
    httpCtrl->hostPort = hostPort;
}


/**
 *	@brief Registers custom headers (char) buffer with HTTP control.
 */
void http_enableCustomHdrs(httpCtrl_t *httpCtrl, char *headerBuf, uint16_t headerBufSz)
{
    ASSERT(httpCtrl->ctrlMagic == streams__ctrlMagic, srcfile_ltemc_http_c);
    ASSERT(headerBuf != NULL, srcfile_ltemc_http_c);
    ASSERT(headerBufSz > 0, srcfile_ltemc_http_c);

    memset(headerBuf, 0, headerBufSz);
    httpCtrl->cstmHdrs = headerBuf;
    httpCtrl->cstmHdrsSz = headerBufSz;
}


/**
 *	@brief Adds standard set of http headers to the custom headers buffer, requires previous custom headers enable and buffer registration.
 */
void http_addDefaultHdrs(httpCtrl_t *httpCtrl, httpHeaderMap_t headerMap)
{
    ASSERT(httpCtrl->ctrlMagic == streams__ctrlMagic, srcfile_ltemc_http_c);
    ASSERT(httpCtrl->cstmHdrs != NULL, srcfile_ltemc_http_c);

    if (headerMap & httpHeaderMap_accept > 0 || headerMap == httpHeaderMap_all)
    {
        if (strlen(httpCtrl->cstmHdrs) + 13 < httpCtrl->cstmHdrsSz)
            strcat(httpCtrl->cstmHdrs, "Accept: */*\r\n");
    }
    if (headerMap & httpHeaderMap_userAgent > 0 || headerMap == httpHeaderMap_all)
    {
        if (strlen(httpCtrl->cstmHdrs) + 25 < httpCtrl->cstmHdrsSz)
            strcat(httpCtrl->cstmHdrs, "User-Agent: QUECTEL_BGx\r\n");
    }
    if (headerMap & httpHeaderMap_connection > 0 || headerMap == httpHeaderMap_all)
    {
        if (strlen(httpCtrl->cstmHdrs) + 24 < httpCtrl->cstmHdrsSz)
            strcat(httpCtrl->cstmHdrs, "Connection: Keep-Alive\r\n");
    }
    if (headerMap & httpHeaderMap_contentType > 0 || headerMap == httpHeaderMap_all)
    {
        if (strlen(httpCtrl->cstmHdrs) + 40 < httpCtrl->cstmHdrsSz)
            strcat(httpCtrl->cstmHdrs, "Content-Type: application/octet-stream\r\n");
    }
}



void http_addCustomHdr(httpCtrl_t *httpCtrl, const char *hdrText)
{
    ASSERT(httpCtrl->ctrlMagic == streams__ctrlMagic, srcfile_ltemc_http_c);
    ASSERT(httpCtrl->cstmHdrs != NULL, srcfile_ltemc_http_c);
    ASSERT(strlen(httpCtrl->cstmHdrs) + strlen(hdrText) + 2 < httpCtrl->cstmHdrsSz, srcfile_ltemc_http_c);

    strcat(httpCtrl->cstmHdrs, hdrText);
    strcat(httpCtrl->cstmHdrs, "\r\n");
}


/**
 *	@brief Adds a basic authorization header to the custom headers buffer, requires previous custom headers buffer registration.
 */
void http_addBasicAuthHdr(httpCtrl_t *http, const char *user, const char *pw)
{
    ASSERT(http->ctrlMagic == streams__ctrlMagic, srcfile_ltemc_http_c);
    ASSERT(http->cstmHdrs != NULL, srcfile_ltemc_http_c);

    char toEncode[80];
    char b64str[120];

    strcat(toEncode, user);
    strcat(toEncode, ":");
    strcat(toEncode, pw);
    // endcode to Base64 string
    binToB64(b64str, toEncode, strlen(toEncode));

    if (strlen(http->cstmHdrs) + 19 + strlen(b64str) < http->cstmHdrsSz)
    {
        strcat(http->cstmHdrs, "Authentication: ");
        strcat(http->cstmHdrs, b64str);
        strcat(http->cstmHdrs, "\r\n");
    }
}


/* ------------------------------------------------------------------------------------------------
 *  Request and Response Section 
 * --------------------------------------------------------------------------------------------- */

/**
 *	@brief Perform HTTP GET operation. Results are internally buffered on the LTEm, see http_read().
 */
resultCode_t http_get(httpCtrl_t *httpCtrl, const char* relativeUrl, bool returnResponseHdrs, uint8_t timeoutSec)
{
    ASSERT(httpCtrl->ctrlMagic == streams__ctrlMagic, srcfile_ltemc_http_c);

    httpCtrl->requestState = httpState_idle;
    httpCtrl->requestType = 'G';
    httpCtrl->httpStatus = resultCode__unknown;

    timeoutSec = (timeoutSec == 0) ? http__defaultTimeoutBGxSec : timeoutSec;
    // uint32_t timeoutMS = timeoutSec * 1000;
    resultCode_t atResult;

    atcmd_setOptions(atcmd__defaultTimeoutMS, atcmd__useDefaultOKCompletionParser);
    if (ATCMD_awaitLock(PERIOD_FROM_SECONDS(timeoutSec)))
    {
        if (returnResponseHdrs)
        {
            atcmd_invokeReuseLock("AT+QHTTPCFG=\"responseheader\",%d",  (int)(httpCtrl->returnResponseHdrs));
            atResult = atcmd_awaitResult();
            if (atResult != resultCode__success)
            {
                atcmd_close();
                return atResult;
            }
        }

        if (httpCtrl->useTls)
        {
            // AT+QHTTPCFG="sslctxid",<httpCtrl->sckt>
            atcmd_invokeReuseLock("AT+QHTTPCFG=\"sslctxid\",%d",  (int)httpCtrl->sckt);
            atResult = atcmd_awaitResult();
            if (atResult != resultCode__success)
            {
                atcmd_close();
                return atResult;
            }
        }

        /* SET URL FOR REQUEST
        * set BGx HTTP URL: AT+QHTTPURL=<urlLength>,timeoutSec  (BGx default timeout is 60, if not specified)
        * wait for CONNECT prompt, then output <URL>, /r/n/r/nOK
        * 
        * NOTE: there is only 1 URL in the BGx at a time
        *---------------------------------------------------------------------------------------------------------------*/
        // if (strlen(relativeUrl) > 0 && relativeUrl[0] != '/')
        //     return resultCode__badRequest;

        char url[240];                                                              // need to get length of URL prior to cmd
        strcpy(url, &httpCtrl->urlHost);
        if (strlen(relativeUrl) > 0 && relativeUrl[0] != '/')
            strcat(url, "/");
        strcat(url, relativeUrl);
        PRINTF(dbgColor__dGreen, "URL(%d)=%s \r", strlen(url), url);

        if ((atResult = S_setUrl(url, timeoutSec)) != resultCode__success)
        {
            PRINTF(dbgColor__warn, "Failed set URL (%d)\r", atResult);
            atcmd_close();
            return atResult;
        }

        /* INVOKE HTTP GET METHOD
        * BGx responds with OK immediately upon acceptance of cmd, then later (up to timeout) with "+QHTTPGET: " string
        * After "OK" we switch IOP to data mode and return. S_httpDoWork() handles the parsing of the page response and
        * if successful, the issue of the AT+QHTTPREAD command to start the page data stream
        * 
        * This allows other application tasks to be performed while waiting for page. No LTEm commands can be invoked
        * but non-LTEm tasks like reading sensors can continue.
        *---------------------------------------------------------------------------------------------------------------*/
        ATCMD_reset(false);                                                         // reset atCmd control struct WITHOUT clearing lock
        atcmd_setOptions(PERIOD_FROM_SECONDS(timeoutSec), S_httpGetStatusParser);

        char httpRequestCmd[20];
        snprintf(httpRequestCmd, sizeof(httpRequestCmd), "AT+QHTTPGET=%d", timeoutSec);
        atcmd_sendCmdData(httpRequestCmd, strlen(httpRequestCmd), "\r");               

        atResult = atcmd_awaitResult();                                         // wait for "+QHTTPGET trailer
        if (atResult == resultCode__success)
        {
            httpCtrl->httpStatus = S_parseRequestResponse(httpCtrl, atcmd_getLastResponse());
            if (httpCtrl->httpStatus >= resultCode__success && httpCtrl->httpStatus <= resultCode__successMax)
                httpCtrl->requestState = httpState_requestComplete;                 // update httpState, got GET/POST response
        }
        else
        {
            httpCtrl->requestState = httpState_idle;
            httpCtrl->httpStatus = atResult;
            PRINTF(dbgColor__warn, "Closed failed GET request, status=%d\r", httpCtrl->httpStatus);
        }
        atcmd_close();
        return httpCtrl->httpStatus;
    }
    return resultCode__timeout;
}   // http_get()


/**
 *	@brief Performs a HTTP POST page web request.
 */
resultCode_t http_post(httpCtrl_t *httpCtrl, const char* relativeUrl, bool returnResponseHdrs, const char* postData, uint16_t postDataSz, uint8_t timeoutSec)
{
    ASSERT(httpCtrl->ctrlMagic == streams__ctrlMagic, srcfile_ltemc_http_c);

    httpCtrl->requestState = httpState_idle;
    httpCtrl->requestType = 'P';
    httpCtrl->httpStatus = resultCode__unknown;

    timeoutSec = (timeoutSec == 0) ? http__defaultTimeoutBGxSec : timeoutSec;
    // uint32_t timeoutMS = timeoutSec * 1000;
    resultCode_t atResult;

    atcmd_setOptions(atcmd__defaultTimeoutMS, atcmd__useDefaultOKCompletionParser);
    if (ATCMD_awaitLock(PERIOD_FROM_SECONDS(timeoutSec)))
    {
        if (returnResponseHdrs)
        {
            atcmd_invokeReuseLock("AT+QHTTPCFG=\"responseheader\",%d",  (int)(httpCtrl->returnResponseHdrs));
            atResult = atcmd_awaitResult();
            if (atResult != resultCode__success)
            {
                atcmd_close();
                return atResult;
            }
        }

        if (httpCtrl->useTls)
        {
            // AT+QHTTPCFG="sslctxid",<httpCtrl->sckt>
            atcmd_invokeReuseLock("AT+QHTTPCFG=\"sslctxid\",%d",  (int)httpCtrl->sckt);
            atResult = atcmd_awaitResult();
            if (atResult != resultCode__success)
            {
                atcmd_close();
                return atResult;
            }
        }

        /* SET URL FOR REQUEST
        * set BGx HTTP URL: AT+QHTTPURL=<urlLength>,timeoutSec  (BGx default timeout is 60, if not specified)
        * wait for CONNECT prompt, then output <URL>, /r/n/r/nOK
        * 
        * NOTE: there is only 1 URL in the BGx at a time
        *---------------------------------------------------------------------------------------------------------------*/
        char url[240];                                                              // need to get length of URL prior to cmd
        strcpy(url, &httpCtrl->urlHost);
        if (strlen(relativeUrl) > 0 && relativeUrl[0] != '/')
            strcat(url, "/");
        strcat(url, relativeUrl);
        PRINTF(dbgColor__dGreen, "URL(%d)=%s \r", strlen(url), url);

        if ((atResult = S_setUrl(url, timeoutSec)) != resultCode__success)
        {
            PRINTF(dbgColor__warn, "Failed set URL (%d)\r", atResult);
            atcmd_close();
            return atResult;
        }

        /* INVOKE HTTP ** POST ** METHOD
        * BGx responds with OK immediately upon acceptance of cmd, then later (up to timeout) with "+QHTTPPOST: " string
        * After "OK" we switch IOP to data mode and return. S_httpDoWork() handles the parsing of the page response and
        * if successful, the issue of the AT+QHTTPREAD command to start the page data stream
        * 
        * This allows other application tasks to be performed while waiting for page. No LTEm commands can be invoked
        * but non-LTEm tasks like reading sensors can continue.
        *---------------------------------------------------------------------------------------------------------------*/
        ATCMD_reset(false);                                                                     // reset atCmd control struct WITHOUT clearing lock
        atcmd_setOptions(PERIOD_FROM_SECONDS(timeoutSec), ATCMD_connectPromptParser);

        char httpRequestCmd[40];
        uint16_t httpRequestLength = postDataSz;
        httpRequestLength += (httpCtrl->cstmHdrsSz > 0) ? (httpCtrl->cstmHdrsSz + 2) : 0;       // add the custom headers + 2 for EOL after hdrs

        snprintf(httpRequestCmd, sizeof(httpRequestCmd), "AT+QHTTPPOST=%d,10,%d", httpRequestLength, timeoutSec);
        atcmd_sendCmdData(httpRequestCmd, strlen(httpRequestCmd), "\r");               

        if (atcmd_awaitResult() == resultCode__success)                                 // wait for "CONNECT", the status result can be 5,10 seconds or longer
        {
            atcmd_setOptions(PERIOD_FROM_SECONDS(timeoutSec), S_httpPostStatusParser);
            atcmd_sendCmdData(postData, postDataSz, "");
            atResult = atcmd_awaitResult();
            if (atResult == resultCode__success)                                        // wait for "+QHTTPPOST trailer
            {
                httpCtrl->httpStatus = S_parseRequestResponse(httpCtrl, atcmd_getLastResponse());
                if (httpCtrl->httpStatus >= resultCode__success && httpCtrl->httpStatus <= resultCode__successMax)
                    httpCtrl->requestState = httpState_requestComplete;                 // update httpState, got GET/POST response
            }
            else
            {
                httpCtrl->requestState = httpState_idle;
                httpCtrl->httpStatus = atResult;
                PRINTF(dbgColor__warn, "Closed failed HTTP request, status=%d\r", httpCtrl->httpStatus);
            }
        }
        else
            httpCtrl->httpStatus = resultCode__internalError;
        atcmd_close();
        return httpCtrl->httpStatus;
    }                                                                           // master awaitLock() over transaction
    return resultCode__timeout;
}   // http_post()



/**
 *	@brief Retrieves page results from a previous GET or POST.
 */
resultCode_t http_readPage(httpCtrl_t *httpCtrl, uint16_t timeoutSec)
{
    iop_t *iopPtr = (iop_t*)g_lqLTEM.iop;                                       // shortcut to the IOP subsystem
    resultCode_t rslt;
    timeoutSec = (timeoutSec == 0) ? http__defaultTimeoutBGxSec : timeoutSec;
    uint32_t timeoutMS = timeoutSec * 1000;

    int pages = 0;
    int bytes = 0;

    /* PROCESS ACTIVE DATA STREAM FLOW 
     *-------------------------------------------------------------------------------------------*/
    if (httpCtrl->requestState != httpState_requestComplete)        // readPage() only valid after 
        return resultCode__notFound;                                // a completed GET\PUT

    // readability var
    rxDataBufferCtrl_t *pBuf = &(httpCtrl->recvBufCtrl);          // smart-buffer for this operation

    IOP_resetRxDataBufferPage(pBuf, 0);                           // reset data buffers for flow
    IOP_resetRxDataBufferPage(pBuf, 1);
    pBuf->iopPg = 0;


    // PRINTF(dbgColor__dGreen, "httpDoWork-iopPg=%d, [0]=%d, [1]=%d\r", pBuf->iopPg, rxPageDataAvailable(pBuf, 0), rxPageDataAvailable(pBuf, 1));

    /* once read is invoked page data buffered in BGx will stream VERY quickly. If the page is
    * large, there may be a pause for BGx to retrieve more data. At this point the device
    * needs to be dedicated to servicing the incoming data.
    * 
    * ISR will be firing and filling buffers, this block needs to efficiently send them to appl
    *-----------------------------------------------------------------------------------------------*/

    atcmd_setOptions(timeoutMS, ATCMD_connectPromptParser);
    if (ATCMD_awaitLock(timeoutMS))
    {
        iopPtr->rxStreamCtrl = httpCtrl;                            // put IOP in data mode

        /* issue AT+QHTTPREAD to start data stream, change state to httpState_responseRecvd
        *-----------------------------------------------------------------------------------*/
        uint16_t atResult;
        char atCmdStr[30];
        snprintf(atCmdStr, sizeof(atCmdStr), "AT+QHTTPREAD=%d", timeoutMS);
        atcmd_sendCmdData(atCmdStr, strlen(atCmdStr), "\r");
        
        /* process page data 
        *-----------------------------------------------------------------------------------*/
        uint16_t dataAvailable;
        char *pTrailer;
        uint8_t trailerLen = 0;
        uint16_t appAvailable;

        while (true)
        {
            dataAvailable = rxPageDataAvailable(pBuf, !pBuf->iopPg);

            /* skip "CONNECT"   (9 chars with \r\n) */
            if (httpCtrl->requestState < httpState_readingData && dataAvailable > 16)
            {
                char *continuePtr = lq_strnstr(pBuf->pages[!pBuf->iopPg]._buffer, "CONNECT\r\n", 16);
                if (continuePtr == NULL)
                {
                    httpCtrl->httpStatus = resultCode__internalError;
                    break;
                }
                httpCtrl->requestState = httpState_readingData;
                pBuf->pages[!pBuf->iopPg].tail = continuePtr + 9;
            }

            /* Check for end-of-page (EOP), stop sending app data at "+QHTTPREAD: <err>" 
             *-------------------------------------------------------------------------------*/
            // char *pTrailer = IOP_findLast(pBuf, "\r\nOK\r\n\r\n+QHTTPREAD: ", 25, "\r\n");

            pTrailer = IOP_findRxAhead(pBuf, 20, "+QHTTPREAD: ", "\r\n");               // room for 3 error response + term, points to char after "+QHTTPREAD: "
            if (pTrailer != NULL && httpCtrl->requestState != httpState_closing)
            {
                char resultBuf[20];
                if ((trailerLen = IOP_fetchRxAhead(pBuf, pTrailer, 19, resultBuf)) > 0)
                {
                    uint16_t bgxError = strtol(resultBuf + 12, NULL, 10);
                    if (bgxError != 0)
                        httpCtrl->httpStatus = resultCode__internalError + bgxError;
                }
                httpCtrl->requestState = httpState_closing;
                httpCtrl->httpStatus = resultCode__success;
            }

            /* Forward data to app
             *-------------------------------------------------------------------------------*/
            appAvailable = rxPageDataAvailable(pBuf, !pBuf->iopPg);
            if (httpCtrl->requestState >= httpState_readingData && appAvailable > 0)
            {
                if (!httpCtrl->pageCancellation)
                {
                    httpCtrl->dataRecvCB(httpCtrl->sckt, httpCtrl->httpStatus, pBuf->pages[!pBuf->iopPg].tail, (appAvailable - trailerLen));
                }
                IOP_resetRxDataBufferPage(pBuf, !pBuf->iopPg);                              // delivered, reset buffer and reset ready

                if (httpCtrl->requestState == httpState_closing)                            // if page trailer detected
                {
                    PRINTF(dbgColor__dGreen, "Closing reqst %d, status=%d\r", httpCtrl->sckt, httpCtrl->httpStatus);
                    httpCtrl->requestState = httpState_idle;
                    break;
                }
            }
            
            /* Check for buffer PAGE TIMEOUT to pull last partial buffer in from IOP
             *-------------------------------------------------------------------------------*/
            if (rxPageDataAvailable(pBuf, pBuf->iopPg) && IOP_getRxIdleDuration() > 200)
            {
                // char *pT = IOP_findRxAhead(pBuf, 20, "+QHTTPREAD: ", "\r\n"); // room for 3 digit response + term
                // char tBuf[16];
                // uint16_t charCnt = IOP_fetchRxAhead(pBuf, pT, 15, tBuf);
                // PRINTF(0, "%s\r", tBuf);

                PRINTF(dbgColor__cyan, "PgTO-Take>%d ", pBuf->iopPg);
                IOP_swapRxBufferPage(pBuf);                                                 // pull page forward
            }

            /* catch REQUEST TIMEOUT here, so we don't wait forever.
            *-----------------------------------------------------------------------------------------------------------*/
            if (pElapsed(((atcmd_t*)g_lqLTEM.atcmd)->invokedAt, timeoutMS))                 // if check for request timeout
            {
                httpCtrl->requestState = httpState_closing;
                httpCtrl->httpStatus = resultCode__timeout;
                break;
            }
        }   //while(true) 

        if (httpCtrl->pageCancellation)
            httpCtrl->httpStatus = resultCode__cancelled;

        atcmd_close();                                                                      // close the GET\POST request and release atcmd lock
        iopPtr->rxStreamCtrl = NULL;                                                        // done, take IOP out of data mode
        return httpCtrl->httpStatus;
    }
}


#pragma endregion



#pragma region Static Functions
/*-----------------------------------------------------------------------------------------------*/

static uint16_t S_parseRequestResponse(httpCtrl_t *httpCtrl, const char *responseTail)
{
    httpCtrl->httpStatus = strtol(++responseTail, &responseTail, 10);    // skip ',' and parse http status
    httpCtrl->pageRemaining = 0;
    httpCtrl->pageSize = strtol(++responseTail, &responseTail, 10);          // skip ',' and parse content length
    if (httpCtrl->pageSize > 0)
        httpCtrl->pageRemaining = httpCtrl->pageSize;

    return httpCtrl->httpStatus;
}


static uint16_t S_setUrl(const char *url, uint16_t timeoutSec)
{
    uint16_t atResult;

    atcmd_setOptions(PERIOD_FROM_SECONDS(10), ATCMD_connectPromptParser);
    uint8_t urlState = 0;
    atcmd_invokeReuseLock("AT+QHTTPURL=%d,%d", strlen(url), timeoutSec);
    atResult = atcmd_awaitResult();
    if (atResult == resultCode__success)                                            // waiting for URL prompt "CONNECT"
    {
        urlState++;
        atcmd_setOptions(PERIOD_FROM_SECONDS(10), atcmd__useDefaultOKCompletionParser); 
        atcmd_sendCmdData(url, strlen(url), "");                                    // wait for BGx OK (send complete) 
        atResult = atcmd_awaitResult();
        if (atResult == resultCode__success)
            urlState++;
    }
    return (urlState == 2) ? resultCode__success : atResult;
}


static cmdParseRslt_t S_httpGetStatusParser(const char *response, char **endptr) 
{
    return atcmd__stdResponseParser("+QHTTPGET: ", true, NULL, 0, 0, NULL, 0);
}


static cmdParseRslt_t S_httpPostStatusParser(const char *response, char **endptr) 
{
    // successful parsing returns 200 (success) + code at position 0, 
    return atcmd__stdResponseParser("+QHTTPPOST: ", true, NULL, 0, 0, NULL, 0);
}
