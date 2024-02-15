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

    http__noReturnResponseHeaders = 0, 
    http__returnResponseHeaders = 1, 
    http__useDefaultTimeout = 0,
    http__defaultTimeoutBGxSec = 60,
    http__requestTypeSz = 5,                            // GET or POST
    http__requestBaseSz = 50,                           // Size of a custom request base (1st 2 lines) less URLs and content-length
    http__standardHeadersSz = 130,                      // For custom requests, the combined length of all the "standard/common" headers 
    http__authHeaderSz = 20,                            // Size of authorization header without encoded credentials
    http__contentLengthMinHeaderSz = 21,                // Size of "Content-Length: 0" header with 2 EOL body separator
    http__contentLengthHeaderSz = 25,                   // Size of Content-Length header with 2 EOL body separator
    http__minUrlSz = 16,                                // URL validation (ASSERT)
    http__maxUrlSz = 224,                               // Maximum URL length host+relative
    http__maxHeaderKeySz = 48,                          // Maximum length for a custom header key size to use with http_addHeader()
    http__readToFileNameSzMax = 80,
    http__readToFileTimeoutSec = 180,                   // Total number of seconds for read to file allowed (atcmd processing)
    http__readToFileInterPcktTimeoutSec = 20,           // BGx inter-packet timeout (max interval between two packets)

    http__typicalCustomRequestHeaders = 275             // HTTP request with typical URL, common headers and space for one/two custom headers
};


/** 
 *  @brief Callback function for data received event. Notifies application that new data is available and needs serviced.
 *
 *  @param [in] dataCntxt [in] Originating data context
 *  @param [in] data [in] Pointer to received data buffer
 *  @param [in] dataSz [in] The number of bytes available
 *  @param [in] isFinal Last invoke of the callback will indicate with isFinal = true.
 */
typedef void (*httpAppRcvr_func)(dataCntxt_t dataCntxt, char *data, uint16_t dataSz, bool isFinal);


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
    char * buffer;
    uint16_t buffersz;
    uint16_t headersLen;
    uint16_t contentLen;
} httpRequest_t;


typedef struct httpCtrl_tag
{
    char streamType;                            // stream type
    dataCntxt_t dataCntxt;                      // integer representing the source of the stream; fixed for protocols, file handle for FS
    dataHndlr_func dataRxHndlr;                 // function to handle data streaming, initiated by eventMgr() or atcmd module
    urcEvntHndlr_func urcEvntHndlr;             // function to determine if "potential" URC event is for an open stream and perform reqd actions
    closeStream_func closeStreamCB;             // function to close stream and update stream control structure (usually invoked after URC detected)

    /* Above section of <stream>Ctrl structure is the same for all LTEmC implemented streams/protocols TCP/HTTP/MQTT etc. 
    */
    appRcvr_func appRcvrCB;                     // callback into host application with data (cast from generic func* to stream specific function)
    bool useTls;                                // flag indicating SSL/TLS applied to stream
    char hostUrl[host__urlSz];                  // URL or IP address of host
    uint16_t hostPort;                          // IP port number host is listening on (allows for 65535/0)
    // bool returnResponseHdrs;                    // if set true, response headers are included in the returned response
    char * responseHdrs;                        // pointer to application provided buffer for response headers
    uint16_t responseBffrSz;                    // size of app provided response header buffer
    char requestType[http__requestTypeSz];      // type of current/last request: 'G'=GET, 'P'=POST
    httpState_t requestState;                   // current state machine variable for HTTP request
    uint16_t bgxError;                          // BGx sprecific error code returned from GET/POST
    uint16_t httpStatus;                        // set to 0 during a request, initialized to 0xFFFF before any request
    uint32_t pageSize;                          // if provided in page response, the page size 
    uint32_t pageRemaining;                     // set to page size (if incl in respose) counts down to 0 (used for optimizing page end parsing)
    uint8_t timeoutSec;                         // default timeout for GET/POST/read requests (BGx is 60 secs)
    uint16_t defaultBlockSz;                    // default size of block (in of bytes) to transfer to app from page read (page read spans blocks)
    bool pageCancellation;                      // set to abandon further page loading
} httpCtrl_t;


#ifdef __cplusplus
extern "C" 
{
#endif


/**
 *	@brief Create a HTTP(s) control structure to manage web communications. 
 *  @param [in] httpCtrl HTTP control structure pointer, struct defines parameters of communications with web server.
 *	@param [in] dataCntxt The data context (0-5) to use for this communications.
 *  @param [in] recvCallback Callback function to receive incoming page data.
 */
void http_initControl(httpCtrl_t *httpCtrl, dataCntxt_t dataCntxt, httpAppRcvr_func appRcvrCB);


/**
 *	@brief Set host connection characteristics. 
 *  @param [in] httpCtrl HTTP control structure pointer, struct defines parameters of communications with web server.
 *  @param [in] hostURL The HOST address of the web server URL.
 *  @param [in] hostPort The port number for the host web server. 0 >> auto-select HTTP(80), HTTPS(443)
 */
void http_setConnection(httpCtrl_t *httpCtrl, const char *hostUrl, uint16_t hostPort);


/**
 *	@brief Set host connection characteristics. 
 *  @param [in] httpCtrl HTTP control structure pointer, struct defines parameters of communications with web server.
 *  @param [in] respHdrBffr Char buffer to return response headers.
 *  @param [in] respHdrBffrSz Size of the response header buffer, if response exceeds buffer size the excess header content is dropped.
 */
void http_setResponseHeadersBuffer(httpCtrl_t *httpCtrl, char *respHdrBffr, uint16_t respHdrBffrSz);


/**
 * @brief Creates a base HTTP request that can be appended with custom headers.
 * @note This creates a BASE request that can be appended with custom headers of your choosing, the request must be followed 
 * by an empty line (CrLf) when forming the complete HTTP stream sent to the server.
 * 
 * @param reqstType Enum directing the type of request to create.
 * @param [in] hostUrl Host name or IP address.
 * @param [in] relativeUrl Text containing the relative URL to the host (excludes query string).
 * @param [in] contentLength Total expected length of the request body.
 * @param [in] reqstBffr Char buffer to hold the request.
 * @param [in] reqstBffrSz Size of the buffer.
 * @return httpRequest_t object containing the components for a custom HTTP(S) request.
 */
httpRequest_t http_createRequest(httpRequestType_t reqstType, const char* hostUrl, const char* relativeUrl, char* requestBuffer, uint16_t requestBufferSz);

// resultCode_t http_createRequest(httpRequestType_t reqstType, const char* host, const char* relativeUrl, const char* cstmHdrs, uint16_t contentLength, char* reqstBffr, uint16_t reqstBffrSz);


/**
 * @brief Adds standard http headers to a custom headers buffer.
 * 
 * @param [in] request Request object to update.
 * @param [in] headerMap Bitmap for which standard headers to use.
 */
void http_addStandardHeaders(httpRequest_t* request, httpHeaderMap_t headerMap);


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
 * @param [in] keyValue Fully formed header with key and value
 */
void http_addHeader(httpRequest_t* request, const char *keyValue);


/**
 * @brief Helper to compose a generic header and add it to the headers collection being composed.
 * 
 * @param [in] requestBuffer Char buffer (array) to use for composition and headers store.
 * @param [in] requestBufferSz Size of buffer.
 * @param [in] key Header's key value
 * @param [in] val Header's value
 */
void http_addHeaderKeyAndValue(httpRequest_t* request, const char *key, const char *value);


/**
 * @brief Close a request to further header changes. 
 * @details Primarily used with POST file operations; adding post data automatically closes request.
 * 
 * @param [in] httpReqst HTTP request to get updated.
 */
void http_closeHeaders(httpRequest_t* httpReqst);


/**
 * @brief Update in-memory HTTP request with final Content-Length value
 * 
 * @param httpReqst The request to operate on.
 * @param contentLength The length value to set in header.
 */
void http_updateContentLength(httpRequest_t* httpReqst, uint32_t contentLength);


/**
 * @brief Add full or partial post data content.
 * @note This function closes request header section, no additional headers can be added to request
 * once this function has been called with the referenced requestBuffer. 
 * 
 * @param [in] requestBuffer The char buffer to contain the HTTP POST request being constructed.
 * @param [in] requestBufferSz The size of the request char buffer.
 * @param [in] postData Character data to be appended to the request.
 * @return true The provided postData was added to the request.
 * @return uint16_t The number of DROPPED chars due to insufficient room in request buffer.
 */
uint16_t http_addPostData(httpRequest_t* request, const char *postData, uint16_t postDataSz);

/* ------------------------------------------------------------------------------------------------
 *  Request and Response Section 
 * --------------------------------------------------------------------------------------------- */

resultCode_t http_get(httpCtrl_t *httpCtrl, const char* relativeUrl);

/**
 *	@brief Perform HTTP GET operation. Results are internally buffered on the LTEm, see http_read().
 *  @note If customHeaders is provided, this REPLACES the headers normally supplied by the BGx module
 *  and the only headers sent with the request is the headers provided in the customHeaders parameter.
 *
 *  @param [in] httpCtrl Pointer to the control block for HTTP communications.
 *  @param [in] customRequest Provides custom request/headers for GET.
 *  @return resultCode_t indicating the success/failure of the request (HTTP standard result codes).
 */
resultCode_t http_getCustomRequest(httpCtrl_t *httpCtrl, httpRequest_t* customRequest);


/**
 *	@brief Performs a HTTP POST page web request.
 *  @param [in] httpCtrl Pointer to the control block for HTTP communications.
 *	@param [in] relativeUrl URL, relative to the host. If none, can be provided as "" or "/".
 *  @param [in] customHeaders If provided (not NULL or empty) GET will include contents as a custom header array.
 *  @param [in] postData Pointer to char buffer with POST content.
 *  @param [in] returnResponseHdrs if requested (true) the page response stream will prefix the page data.
 *  @return resultCode_t indicating the success/failure of the request (HTTP standard result codes).
 */
resultCode_t http_post(httpCtrl_t *httpCtrl, const char* relativeUrl, const char* postData, uint16_t postDataSz);


/**
 *	@brief Performs a HTTP POST page web request.
 *  @param [in] httpCtrl Pointer to the control block for HTTP communications.
 *	@param [in] relativeUrl URL, relative to the host. If none, can be provided as "" or "/".
 *  @param [in] customHeaders If provided (not NULL or empty) GET will include contents as a custom header array.
 *  @param [in] postData Pointer to char buffer with POST content.
 *  @param [in] returnResponseHdrs if requested (true) the page response stream will prefix the page data.
 *  @return resultCode_t indicating the success/failure of the request (HTTP standard result codes).
 */
resultCode_t http_postCustomRequest(httpCtrl_t *httpCtrl, const char* relativeUrl, httpRequest_t* customRequest);


/**
 *	@brief Sends contents of a file (LTEM filesystem) as POST to remote.

 *  @param [in] httpCtrl Pointer to the control block for HTTP communications.
 *	@param [in] relativeUrl URL, relative to the host. If none, can be provided as "" or "/".
 *  @param [in] filename C-string containing the name of the file containing the request/headers/body content.
 *  @param [in] customHeaders TRUE: the file includes custom headers AND body; FALSE: the file only include body content.
 *  @return resultCode_t indicating the success/failure of the request (HTTP standard result codes).
 */
resultCode_t http_postFile(httpCtrl_t *httpCtrl, const char *relativeUrl, const char* filename, bool customHeaders);


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


/**
 * @brief Translate a module specific HTTP error code into a standard HTTP response code.
 * 
 * @param [in] extendedResultCode BGx HTTP error code.
 * @return resultCode_t Translated standard HTTP response code.
 */
resultCode_t http_translateExtended(uint16_t extendedResultCode);

/**
 * @brief Parse and output a custom HTTP request object to the current logger
 * 
 * @param httpReqst 
 */
void http_writeRequestToLog(httpRequest_t* httpReqst);

#ifdef __cplusplus
}
#endif

#endif  // !__LTEMC_HTTP_H__
