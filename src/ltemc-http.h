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

//#include "ltemc-httpTypes.h"
// #include "ltemc.h"

#include <lq-types.h>
#include "ltemc-streams.h"
#include "ltemc-internal.h"


enum http__constants
{
    http__noResponseHeaders = 0, 
    http__returnResponseHeaders = 1, 
    http__useDefaultTimeout = 0,

    http__urlHostSz = 128
};


/** 
 *  \brief Callback function for data received event. Notifies application that new data is available and needs serviced.
 * 
 *  The *data and dataSz values are for convenience, since the application supplied the buffer to LTEmC.
 * 
 *  \param dataCntxt [in] Data peer (data context or filesys) 
 *  \param httpRslt [in] HTTP status code found in the page result headers
 *  \param data [in] Pointer to received data buffer
 *  \param dataSz [in] The number of bytes available
*/
typedef void (*httpRecvFunc_t)(dataContext_t dataCntxt, uint16_t httpRslt, char *data, uint16_t dataSz);


/** 
 *  \brief If using custom headers, bit-map indicating what headers to create for default custom header collection.
*/
typedef enum httpHeaderMap_tag              // bit-map to indicate headers to create for add custom headers, bitwise OR to http_addDefaultHdrs()
{
    httpHeaderMap_accept = 0x0001,
    httpHeaderMap_userAgent = 0x0002,
    httpHeaderMap_connection = 0x0004,
    httpHeaderMap_contentType = 0x0008,
    httpHeaderMap_contentLenght = 0x0010,
    httpHeaderMap_all = 0xFFFF
} httpHeaderMap_t;


typedef enum httpState_tag
{
    httpState_idle = 0,
    httpState_requestComplete,
    httpState_readingData,
    httpState_closing
} httpState_t;


typedef struct httpCtrl_tag
{
    uint8_t ctrlMagic;                      ///< magic flag to validate incoming requests 
    dataContext_t dataCntxt;                ///< Data context where this control operates
    protocol_t protocol;                    ///< Socket's protocol : UDP/TCP/SSL.
    bool useTls;                            ///< flag indicating SSL/TLS applied to stream
    rxDataBufferCtrl_t recvBufCtrl;         ///< RX smart buffer 

    httpRecvFunc_t dataRecvCB;              ///< callback to application, signals data ready
    uint32_t bufPageSwapTck;                ///< last check for URC\dataPending
    uint32_t bufPageTimeout;                ///< set at init for doWork ASSERT, if timeout reached chance for a data overflow on socket
    char urlHost[http__urlHostSz];          ///< host portion of URL for GET/POST requests
    bool returnResponseHdrs;                ///< if set true, response headers are included in the returned response
    char *cstmHdrs;                         ///< custom header content, optional buffer provided by application
    uint16_t cstmHdrsSz;                    ///< size of custom header buffer
    char requestType;                       ///< type of current\last request: 'G'=GET, 'P'=POST
    httpState_t requestState;               ///< current state machine variable for HTTP request
    uint16_t bgxError;                      ///< BGx sprecific error code returned from GET\POST
    uint16_t httpStatus;                    ///< set to 0 during a request, initialized to 0xFFFF before any request
    uint32_t pageSize;                      ///< if provided in page response, the page size 
    uint32_t pageRemaining;                 ///< set to page size (if incl in respose) counts down to 0 (used for optimizing page end parsing)
    bool pageCancellation;                  ///< set to abandon further page loading
} httpCtrl_t;


#ifdef __cplusplus
extern "C" 
{
#endif

// Session settings factory methods to create options for a series of GET/POST operations

void http_initControl(httpCtrl_t *httpCtrl, dataContext_t dataContext, const char* urlHost, char *recvBuf, uint16_t recvBufSz, httpRecvFunc_t recvCallback);

void http_enableCustomHdrs(httpCtrl_t *httpCtrl, char *hdrBuffer, uint16_t hdrBufferSz);
void http_addDefaultHdrs(httpCtrl_t *httpCtrl, httpHeaderMap_t headerMap);
void http_addBasicAuthHdr(httpCtrl_t *httpCtrl, const char *user, const char *pw);

resultCode_t http_get(httpCtrl_t *httpCtrl, const char* relativeUrl, bool returnResponseHdrs, uint8_t timeoutSec);
resultCode_t http_post(httpCtrl_t *httpCtrl, const char* relativeUrl, bool returnResponseHdrs, const char* postData, uint16_t dataSz, uint8_t timeoutSec);

resultCode_t http_readPage(httpCtrl_t *httpCtrl, uint16_t timeoutSec);
void http_cancelPage(httpCtrl_t *httpCtrl);

// resultCode_t http_checkResult(httpCtrl_t *httpCtrl);
// resultCode_t http_getFinalResult(httpCtrl_t *httpCtrl);
// resultCode_t http_awaitResult(httpCtrl_t *httpCtrl, uint8_t timeoutSec);

// future support for fileSystem destinations, requires file_ module still under development planned for v2.1
/*
resultCode_t http_getFileResponse(void *httpSession, const char* url, const char* filename);
resultCode_t http_postFileResponse(void *httpSession, const char* url, const char* filename);
resultCode_t http_readFileResponse(httpCtrl_t *httpCtrl, char *response, uint16_t responseSz);
*/

#ifdef __cplusplus
}
#endif

#endif  // !__LTEMC_HTTP_H__
