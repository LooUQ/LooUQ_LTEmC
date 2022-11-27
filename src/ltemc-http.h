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

#include <lq-types.h>
#include "ltemc-types.h"

/** 
 *  @brief Typed numeric constants used in HTTP module.
*/
enum http__constants
{
    http__getRequestLength = 128,
    http__postRequestLength = 128,

    http__noResponseHeaders = 0, 
    http__returnResponseHeaders = 1, 
    http__useDefaultTimeout = 0,
    http__defaultTimeoutBGxSec = 60,
    http__urlHostSz = 128,
    http__rqstTypeSz = 5,                               /// GET or POST
    http__customHdrSmallWarning = 40,
    http__reqdResponseSz = 22                           /// BGx HTTP(S) Application Note
};


/** 
 *  @brief Callback function for data received event. Notifies application that new data is available and needs serviced.
 * 
 *  @details The *data and dataSz values are for convenience, since the application supplied the buffer to LTEmC.
 * 
 *  @param dataCntxt [in] Data peer (data context or filesys) 
 *  @param httpRslt [in] HTTP status code found in the page result headers
 *  @param data [in] Pointer to received data buffer
 *  @param dataSz [in] The number of bytes available
*/
typedef void (*httpRecvFunc_t)(dataCntxt_t sckt, uint16_t httpRslt, char *data, uint16_t dataSz);


/** 
 *  @brief If using custom headers, bit-map indicating what headers to create for default custom header collection.
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
    /* Top section of xCtrl structure is the same for all LTEmC implemented protocol suites TCP/HTTP/MQTT etc. */
    uint16_t ctrlMagic;                                     /// magic flag to validate incoming requests 
    dataCntxt_t dataCntxt;                                  /// Data context where this control operates (only SSL/TLS contexts 1-6)
    protocol_t protocol;                                    /// Socket's protocol : UDP/TCP/SSL.
    bool useTls;                                            /// flag indicating SSL/TLS applied to stream
    char hostUrl[host__urlSz];                              /// URL or IP address of host
    char hostPort[host__portSz];                            /// IP port number host is listening on (allows for 65535/0)
    rxDataBufferCtrl_t recvBufCtrl;                         /// RX smart buffer 
    /* End of Common Structure Fields */

    /* HTTP(S) Specific Fields */
    httpRecvFunc_t dataRecvCB;                              /// callback to application, signals data ready
    bool returnResponseHdrs;                                /// if set true, response headers are included in the returned response
    char *cstmHdrs;                                         /// custom header content, optional buffer provided by application
    uint16_t cstmHdrsSz;                                    /// size of custom header buffer
    char requestType[http__rqstTypeSz];                     /// type of current/last request: 'G'=GET, 'P'=POST
    httpState_t requestState;                               /// current state machine variable for HTTP request
    uint16_t bgxError;                                      /// BGx sprecific error code returned from GET/POST
    uint16_t httpStatus;                                    /// set to 0 during a request, initialized to 0xFFFF before any request
    uint32_t pageSize;                                      /// if provided in page response, the page size 
    uint32_t pageRemaining;                                 /// set to page size (if incl in respose) counts down to 0 (used for optimizing page end parsing)
    bool pageCancellation;                                  /// set to abandon further page loading
} httpCtrl_t;


#ifdef __cplusplus
extern "C" 
{
#endif


/**
 *	@brief Create a HTTP(s) control structure to manage web communications. 
 *  @param httpCtrl [in] HTTP control structure pointer, struct defines parameters of communications with web server.
 *	@param dataCntxt [in] The data context (0-5) to use for this communications.
 *  @param recvBuf [in] Pointer to application provided char buffer.
 *  @param recvBufSz [in] Size of the receive buffer.
 *  @param recvCallback [in] Callback function to receive incoming page data.
 */
void http_initControl(httpCtrl_t *httpCtrl, dataCntxt_t dataCntxt, char *recvBuf, uint16_t recvBufSz, httpRecvFunc_t recvCallback);


/**
 *	@brief Set host connection characteristics. 
 *  @param httpCtrl [in] HTTP control structure pointer, struct defines parameters of communications with web server.
 *  @param hostURL [in] The HOST address of the web server URL.
 *  @param hostPort [in] The port number for the host web server. 0 >> auto-select HTTP(80), HTTPS(443)
 */
void http_setConnection(httpCtrl_t *httpCtrl, const char *hostUrl, uint16_t hostPort);


/**
 *	@brief Registers custom headers (char) buffer with HTTP control.
 *  @param httpCtrl [in] - Pointer to the control block for HTTP communications.
 *	@param headerBuf [in] - pointer to header buffer  created by application
 *  @param headerBufSz [in] - size of the header buffer
 */
void http_enableCustomHdrs(httpCtrl_t *httpCtrl, char *hdrBuffer, uint16_t hdrBufferSz);


/**
 *	@brief Adds common http headers to the custom headers buffer. REQUIRES previous enable of custom headers and buffer registration.
 *  @param httpCtrl [in] - Pointer to the control block for HTTP communications.
 *	@param headerMap [in] - Bitmap for which standard headers to use.
 */
void http_addCommonHdrs(httpCtrl_t *httpCtrl, httpHeaderMap_t headerMap);


/**
 *	@brief Adds a basic authorization header to the custom headers buffer, requires previous custom headers buffer registration.
 *
 *  @param http [in] - Pointer to the control block for HTTP communications.
 *	@param user [in] - User name.
 *  @param pw [in] - Password/secret for header.
 */
void http_addBasicAuthHdr(httpCtrl_t *httpCtrl, const char *user, const char *pw);


void http_addCustomHdr(httpCtrl_t *httpCtrl, const char *hdrText);


/* ------------------------------------------------------------------------------------------------
 *  Request and Response Section 
 * --------------------------------------------------------------------------------------------- */

/**
 *	@brief Perform HTTP GET operation. Results are internally buffered on the LTEm, see http_read().
 *  @param httpCtrl [in] - Pointer to the control block for HTTP communications.
 *	@param relativeUrl [in] - The URL to GET (starts with \ and doesn't include the host part)
 *  @param returnResponseHdrs [in] - Set to true for page result to include response headers at the start of the page
 *  @param timeoutSec [in] - Timeout for entire GET request cycle
 *  @return true if GET request sent successfully
 */
resultCode_t http_get(httpCtrl_t *httpCtrl, const char* relativeUrl, bool returnResponseHdrs, uint8_t timeoutSec);


/**
 *	@brief Performs a HTTP POST page web request.
 *  @param httpCtrl [in] - Pointer to the control block for HTTP communications.
 *	@param relativeUrl [in] - URL, relative to the host. If none, can be provided as "" or "/" ()
 *  @param returnResponseHdrs [in] - if requested (true) the page response stream will prefix the page data
 *  @param postData [in] - Pointer to char buffer with POST content
 *  @param postDataSz [in] - Size of the POST content reference by *postData
 *	@param timeoutSec [in] - the number of seconds to wait (blocking) for a page response
 *  @return true if POST request completed
 */
resultCode_t http_post(httpCtrl_t *httpCtrl, const char* relativeUrl, bool returnResponseHdrs, const char* postData, uint16_t dataSz, uint8_t timeoutSec);


/**
 *	@brief Retrieves page results from a previous GET or POST.
 *  @param httpCtrl [in] - Pointer to the control block for HTTP communications.
 *	@param timeoutSec [in] - the number of seconds to wait (blocking) for a page response
 *  @return true if POST request completed
 */
resultCode_t http_readPage(httpCtrl_t *httpCtrl, uint16_t timeoutSec);

void http_cancelPage(httpCtrl_t *httpCtrl);


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
