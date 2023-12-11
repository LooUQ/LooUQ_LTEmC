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


#ifndef __LTEMC_HTTP_H__
#define __LTEMC_HTTP_H__

#include <lq-types.h>
#include "ltemc-types.h"

/** 
 *  @brief Typed numeric constants used in HTTP module.
*/
enum http__constants
{
    // http__getRequestLength = 128,
    // http__postRequestLength = 128,

    http__getRequestCustomSz = 256,
    http__noResponseHeaders = 0, 
    http__returnResponseHeaders = 1, 
    http__useDefaultTimeout = 0,
    http__defaultTimeoutBGxSec = 60,
    http__rqstTypeSz = 5,                           /// GET or POST
    http__commandHdrSz = 105,
    http__readToFileNameSzMax = 80,
    http__readToFileTimeoutSec = 180,               // Total number of seconds for read to file allowed (atcmd processing)
    http__readToFileInterPcktTimeoutSec = 20        // BGx inter-packet timeout (max interval between two packets)
};


/** 
 *  @brief Callback function for data received event. Notifies application that new data is available and needs serviced.
 *
 *  @param [in] dataCntxt [in] Originating data context
 *  @param [in] data [in] Pointer to received data buffer
 *  @param [in] dataSz [in] The number of bytes available
 *  @param [in] isFinal Last invoke of the callback will indicate with isFinal = true.
 */
typedef void (*httpRecv_func)(dataCntxt_t dataCntxt, char *data, uint16_t dataSz, bool isFinal);


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


/**
 * @brief Internal state of the request/read cycle.
 */
typedef enum httpState_tag
{
    httpState_idle = 0,
    httpState_requestComplete,
    httpState_readingData,
    httpState_closing
} httpState_t;


/**
 * @brief Request types supported by LTEmC.
 */
typedef enum httpRequestType_tag
{
    httpRequestType_GET = 'G',
    httpRequestType_POST = 'P',
    httpRequestType_none = 0
} httpRequestType_t;


/**
 * @brief Request (custom requests) object.
 */
typedef struct httpRequest_tag
{
    char * requestBuffer;
    uint16_t requestBuffersz;
    uint16_t headersLen;
    uint16_t contentLen;
} httpRequest_t;


typedef struct httpCtrl_tag
{
    char streamType;                            /// stream type
    dataCntxt_t dataCntxt;                      /// integer representing the source of the stream; fixed for protocols, file handle for FS
    dataRxHndlr_func dataRxHndlr;               /// function to handle data streaming, initiated by eventMgr() or atcmd module
    urcEvntHndlr_func urcEvntHndlr;             /// function to determine if "potential" URC event is for an open stream and perform reqd actions

    /* Above section of <stream>Ctrl structure is the same for all LTEmC implemented streams/protocols TCP/HTTP/MQTT etc. 
    */
    appRcvProto_func appRecvDataCB;             /// callback into host application with data (cast from generic func* to stream specific function)
    bool useTls;                                /// flag indicating SSL/TLS applied to stream
    char hostUrl[host__urlSz];                  /// URL or IP address of host
    uint16_t hostPort;                          /// IP port number host is listening on (allows for 65535/0)
    bool returnResponseHdrs;                    /// if set true, response headers are included in the returned response
    // char *cstmHdrsBffr;                         /// custom header content, optional buffer provided by application
    // uint16_t cstmHdrsBffrSz;                    /// size of custom header buffer
    char requestType[http__rqstTypeSz];         /// type of current/last request: 'G'=GET, 'P'=POST
    httpState_t requestState;                   /// current state machine variable for HTTP request
    uint16_t bgxError;                          /// BGx sprecific error code returned from GET/POST
    uint16_t httpStatus;                        /// set to 0 during a request, initialized to 0xFFFF before any request
    uint32_t pageSize;                          /// if provided in page response, the page size 
    uint32_t pageRemaining;                     /// set to page size (if incl in respose) counts down to 0 (used for optimizing page end parsing)
    uint8_t timeoutSec;                         /// default timeout for GET/POST/read requests (BGx is 60 secs)
    uint16_t defaultBlockSz;                    /// default size of block (in of bytes) to transfer to app from page read (page read spans blocks)
    bool pageCancellation;                      /// set to abandon further page loading
} httpCtrl_t;


#ifdef __cplusplus
extern "C" 
{
#endif


/**
 *	@brief Create a HTTP(s) control structure to manage web communications. 
 *  @param [in] httpCtrl HTTP control structure pointer, struct defines parameters of communications with web server.
 *	@param [in] dataCntxt [in] The data context (0-5) to use for this communications.
 *  @param [in] recvCallback [in] Callback function to receive incoming page data.
 */
void http_initControl(httpCtrl_t *httpCtrl, dataCntxt_t dataCntxt, httpRecv_func recvCallback);


/**
 *	@brief Set host connection characteristics. 
 *  @param [in] httpCtrl [in] HTTP control structure pointer, struct defines parameters of communications with web server.
 *  @param [in] hostURL [in] The HOST address of the web server URL.
 *  @param [in] hostPort [in] The port number for the host web server. 0 >> auto-select HTTP(80), HTTPS(443)
 */
void http_setConnection(httpCtrl_t *httpCtrl, const char *hostUrl, uint16_t hostPort);


/**
 * @brief Creates a base HTTP request that can be appended with custom headers.
 * @note This creates a BASE request that can be appended with custom headers of your choosing, the request must be followed 
 * by an empty line "\r\n" when forming the complete HTTP stream sent to the server.
 * 
 * @param reqstType Enum directing the type of request to create
 * @param [in] host Text containing the host section of the request URL
 * @param [in] relativeUrl Text containing the relative URL to the host (excludes query string)
 * @param [in] contentLength Total expected length of the request body 
 * @param [in] reqstBffr Char buffer to hold the request
 * @param [in] reqstBffrSz Size of the buffer
 * @return httpRequest_t object containing the components for a custom HTTP(S) request.
 */
httpRequest_t http_createRequest(httpRequestType_t reqstType, const char* host, const char* relativeUrl, char* requestBuffer, uint16_t requestBufferSz);

// resultCode_t http_createRequest(httpRequestType_t reqstType, const char* host, const char* relativeUrl, const char* cstmHdrs, uint16_t contentLength, char* reqstBffr, uint16_t reqstBffrSz);


/**
 * @brief Adds common http headers to a custom headers buffer.
 * 
 * @param [in] requestBuffer Char buffer (array) to use for composition and headers store.
 * @param [in] requestBufferSz Size of buffer.
 * @param [in] headerMap Bitmap for which standard headers to use.
 */
void http_addCommonHdrs(httpRequest_t* request, httpHeaderMap_t headerMap);


/**
 * @brief Adds a basic authorization header to a headers buffer.
 *
 * @param [in] requestBuffer Char buffer (array) to use for composition and headers store.
 * @param [in] requestBufferSz Size of buffer.
 * @param [in] user User name.
 * @param [in] pw Password/secret for header.
 */
void http_addBasicAuthHdr(httpRequest_t* request, const char *user, const char *pw);


/**
 * @brief Helper to compose a generic header and add it to the headers collection being composed.
 * 
 * @param [in] requestBuffer Char buffer (array) to use for composition and headers store.
 * @param [in] requestBufferSz Size of buffer.
 * @param [in] key Header's key value
 * @param [in] val Header's value
 */
void http_addHeader(httpRequest_t* request, const char *key, const char *val);


/**
 * @brief Add full or partial post data content.
 * @note This function closes request header section, no additional headers can be added to request
 * once this function has been called with the referenced requestBuffer. 
 * 
 * @param [in] requestBuffer The char buffer to contain the HTTP POST request being constructed.
 * @param [in] requestBufferSz The size of the request char buffer.
 * @param [in] postData Character data to be appended to the request.
 */
void http_addPostData(httpRequest_t* request, const char *postData, uint16_t postDataSz);

/* ------------------------------------------------------------------------------------------------
 *  Request and Response Section 
 * --------------------------------------------------------------------------------------------- */

resultCode_t http_get(httpCtrl_t *httpCtrl, const char* relativeUrl, bool returnResponseHdrs);

/**
 *	@brief Perform HTTP GET operation. Results are internally buffered on the LTEm, see http_read().
 *  @note If customHeaders is provided, this REPLACES the headers normally supplied by the BGx module
 *  and the only headers sent with the request is the headers provided in the customHeaders parameter.
 *
 *  @param [in] httpCtrl Pointer to the control block for HTTP communications.
 *	@param [in] relativeUrl The URL to GET (starts with \ and doesn't include the host part)
 *  @param [in] customHeaders If provided (not NULL or empty) GET will include contents as a custom header array.
 *  @param [in] returnResponseHdrs Set to true for page result to include response headers at the start of the page
 *  @return resultCode_t indicating the success/failure of the request (HTTP standard result codes).
 */
resultCode_t http_getCustomRequest(httpCtrl_t *httpCtrl, const char* relativeUrl, httpRequest_t* customRequest, bool returnResponseHdrs);


/**
 *	@brief Performs a HTTP POST page web request.
 *  @param [in] httpCtrl Pointer to the control block for HTTP communications.
 *	@param [in] relativeUrl URL, relative to the host. If none, can be provided as "" or "/".
 *  @param [in] customHeaders If provided (not NULL or empty) GET will include contents as a custom header array.
 *  @param [in] postData Pointer to char buffer with POST content.
 *  @param [in] returnResponseHdrs if requested (true) the page response stream will prefix the page data.
 *  @return resultCode_t indicating the success/failure of the request (HTTP standard result codes).
 */
resultCode_t http_post(httpCtrl_t *httpCtrl, const char* relativeUrl, const char* postData, uint16_t postDataSz, bool returnResponseHdrs);


/**
 *	@brief Performs a HTTP POST page web request.
 *  @param [in] httpCtrl Pointer to the control block for HTTP communications.
 *	@param [in] relativeUrl URL, relative to the host. If none, can be provided as "" or "/".
 *  @param [in] customHeaders If provided (not NULL or empty) GET will include contents as a custom header array.
 *  @param [in] postData Pointer to char buffer with POST content.
 *  @param [in] returnResponseHdrs if requested (true) the page response stream will prefix the page data.
 *  @return resultCode_t indicating the success/failure of the request (HTTP standard result codes).
 */
resultCode_t http_postCustomRequest(httpCtrl_t *httpCtrl, const char* relativeUrl, httpRequest_t* customRequest, bool returnResponseHdrs);


/**
 *	@brief Sends contents of a file (LTEM filesystem) as POST to remote.

 *  @param [in] httpCtrl Pointer to the control block for HTTP communications.
 *	@param [in] relativeUrl URL, relative to the host. If none, can be provided as "" or "/".
 *  @param [in] filename C-string containing the name of the file containing the request/headers/body content.
 *  @param [in] returnResponseHdrs if requested (true) the page response stream will prefix the page data
 *  @return resultCode_t indicating the success/failure of the request (HTTP standard result codes).
 */
resultCode_t http_postFile(httpCtrl_t *httpCtrl, const char *relativeUrl, const char* filename, bool returnResponseHdrs);


/**
 *	@brief Retrieves page results from a previous GET or POST.

 *  @param [in] httpCtrl Pointer to the control block for HTTP communications.
 *  @return resultCode_t indicating the success/failure of the request (HTTP standard result codes).
 */
resultCode_t http_readPage(httpCtrl_t *httpCtrl);


/**
 *	@brief Retrieves page results from a previous GET or POST storing in file (LTEM filesystem).

 *  @param [in] httpCtrl Pointer to the control block for HTTP communications.
 *  @param [in] filename C-string containing the name of the file to create from page content.
 *  @return resultCode_t indicating the success/failure of the request (HTTP standard result codes).
 */
resultCode_t http_readPageToFile(httpCtrl_t *httpCtrl, const char* filename);


/**
 *	@brief Cancels a http_readPage flow if the remaining contents are not needed.
 *  @details This is a blocking call. The page read off the network will continue, but the contents will be discarded.

 *  @param [in] httpCtrl Pointer to the control block for HTTP communications.
 */
void http_cancelPage(httpCtrl_t *httpCtrl);


#ifdef __cplusplus
}
#endif

#endif  // !__LTEMC_HTTP_H__
