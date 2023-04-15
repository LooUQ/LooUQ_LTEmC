/** ****************************************************************************
  \file 
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
    http__customHdrSmallWarning = 40
    // http__reqdResponseSz = 22                           /// BGx HTTP(S) Application Note
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
typedef void (*httpRecv_func)(dataCntxt_t sckt, uint16_t httpRslt, char *data, uint16_t dataSz);


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


typedef volatile struct httpCtrl_tag
{
    /* Top section of xCtrl structure is the same for all LTEmC implemented protocol suites TCP/HTTP/MQTT etc. */
    dataCntxt_t dataCntxt;                                  /// Data context where this control operates (only SSL/TLS contexts 1-6)
    bool useTls;                                            /// flag indicating SSL/TLS applied to stream
    char hostUrl[host__urlSz];                              /// URL or IP address of host
    uint16_t hostPort;                                      /// IP port number host is listening on (allows for 65535/0)
    bool returnResponseHdrs;                                /// if set true, response headers are included in the returned response
    char *cstmHdrs;                                         /// custom header content, optional buffer provided by application
    uint16_t cstmHdrsSz;                                    /// size of custom header buffer
    char requestType[http__rqstTypeSz];                     /// type of current/last request: 'G'=GET, 'P'=POST
    httpState_t requestState;                               /// current state machine variable for HTTP request
    uint16_t bgxError;                                      /// BGx sprecific error code returned from GET/POST
    uint16_t httpStatus;                                    /// set to 0 during a request, initialized to 0xFFFF before any request
    uint32_t pageSize;                                      /// if provided in page response, the page size 
    uint32_t pageRemaining;                                 /// set to page size (if incl in respose) counts down to 0 (used for optimizing page end parsing)
    uint8_t defaultTimeoutS;                                /// default timeout for page requests (BGx is 60 secs)
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
void http_initControl(httpCtrl_t *httpCtrl, dataCntxt_t dataCntxt, httpRecv_func recvCallback);


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
 *	@param pageBffr [in] - Pointer to the host application buffer to receive page contents.
 *  @param bffrSz [in] The number of chars to return.
 *  
 *  @return true if there are additional chars to read, false if page contents have been transfered and the request is complete.
 */
bool http_readPage(httpCtrl_t *httpCtrl, char *pageBffr, uint16_t pageBffrSz, uint16_t *httpResult);

/**
 *	@brief Cancels a http_readPage flow if the remaining contents are not needed.
 *  @details This is a blocking call. The page read off the network will continue, but the contents will be discarded.

 *  @param httpCtrl [in] - Pointer to the control block for HTTP communications.
 */
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
