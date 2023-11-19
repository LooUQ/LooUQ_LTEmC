/** ***************************************************************************
  @file 
  @brief Modem HTTP(S) communication features/services.

  @author Greg Terrell, LooUQ Incorporated

  \loouq

  @warning Internal dependencies, changes only as directed by LooUQ staff.

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
 * @brief Typed numeric constants used in HTTP module.
*/
enum http__constants
{
    // http__getRequestLength = 128,
    // http__postRequestLength = 128,

    http__noResponseHeaders = 0, 
    http__returnResponseHeaders = 1, 
    http__getRequestSz = 448,
    http__postRequestSz = 448,
    http__useDefaultTimeout = 0,
    http__defaultTimeoutBGxSec = 60,
    http__rqstTypeSz = 5,                           /// GET or POST
    http__customHdrSmallWarning = 40,
    http__readToFileBytesPerSecond = 11520,
    http__readToFileTimeoutRatio = 150,
    http__readToFileNameSzMax = 80,
    http__readToFileTimeoutSec = 240,               // Total number of seconds for read to file allowed (atcmd processing)
    http__readToFileInterPcktTimeoutSec = 20        // BGx inter-packet timeout (max interval between two packets)
};


/** 
 * @brief Callback function for data received event. Notifies application that new data is available and needs serviced.
 *
 * @param [in] dataCntxt [in] Originating data context
 * @param [in] data [in] Pointer to received data buffer
 * @param [in] dataSz [in] The number of bytes available
 * @param [in] isFinal Last invoke of the callback will indicate with isFinal = true.
 */
typedef void (*httpRecv_func)(dataCntxt_t dataCntxt, char *data, uint16_t dataSz, bool isFinal);


/** 
 * @brief If using custom headers, bit-map indicating what headers to create for default custom header collection.
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
    char *cstmHdrs;                             /// custom header content, optional buffer provided by application
    uint16_t cstmHdrsSz;                        /// size of custom header buffer
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
 * @brief Create a HTTP(s) control structure to manage web communications. 
 * @param [in] httpCtrl HTTP control structure pointer, struct defines parameters of communications with web server.
 * @param [in] dataCntxt [in] The data context (0-5) to use for this communications.
 * @param [in] recvCallback [in] Callback function to receive incoming page data.
 */
void http_initControl(httpCtrl_t *httpCtrl, dataCntxt_t dataCntxt, httpRecv_func recvCallback);


/**
 * @brief Set host connection characteristics. 
 * @param [in] httpCtrl [in] HTTP control structure pointer, struct defines parameters of communications with web server.
 * @param [in] hostURL [in] The HOST address of the web server URL.
 * @param [in] hostPort [in] The port number for the host web server. 0 >> auto-select HTTP(80), HTTPS(443)
 */
void http_setConnection(httpCtrl_t *httpCtrl, const char *hostUrl, uint16_t hostPort);


/**
 * @brief Registers custom headers (char) buffer with HTTP control.
 * @param [in] httpCtrl Pointer to the control block for HTTP communications.
 * @param [in] headerBuf pointer to header buffer  created by application
 * @param [in] headerBufSz size of the header buffer
 */
void http_enableCustomHdrs(httpCtrl_t *httpCtrl, char *hdrBuffer, uint16_t hdrBufferSz);


/**
 * @brief Adds common http headers to the custom headers buffer. REQUIRES previous enable of custom headers and buffer registration.
 * @param [in] httpCtrl Pointer to the control block for HTTP communications.
 * @param [in] headerMap Bitmap for which standard headers to use.
 */
void http_addCommonHdrs(httpCtrl_t *httpCtrl, httpHeaderMap_t headerMap);


/**
 * @brief Adds a basic authorization header to the custom headers buffer, requires previous custom headers buffer registration.
 *
 * @param [in] http Pointer to the control block for HTTP communications.
 * @param [in] user User name.
 * @param [in] pw Password/secret for header.
 */
void http_addBasicAuthHdr(httpCtrl_t *httpCtrl, const char *user, const char *pw);


void http_addCustomHdr(httpCtrl_t *httpCtrl, const char *hdrText);


/* ------------------------------------------------------------------------------------------------
 *  Request and Response Section 
 * --------------------------------------------------------------------------------------------- */

/**
 * @brief Perform HTTP GET operation. Results are internally buffered on the LTEm, see http_read().
 * @param [in] httpCtrl Pointer to the control block for HTTP communications.
 * @param [in] relativeUrl The URL to GET (starts with \ and doesn't include the host part)
 * @param [in] returnResponseHdrs Set to true for page result to include response headers at the start of the page
 * @return true if GET request sent successfully
 */
resultCode_t http_get(httpCtrl_t *httpCtrl, const char* relativeUrl, bool returnResponseHdrs);


/**
 * @brief Performs a HTTP POST page web request.
 * @param [in] httpCtrl Pointer to the control block for HTTP communications.
 * @param [in] relativeUrl URL, relative to the host. If none, can be provided as "" or "/" ()
 * @param [in] returnResponseHdrs if requested (true) the page response stream will prefix the page data
 * @param [in] postData Pointer to char buffer with POST content
 * @param [in] postDataSz Size of the POST content reference by *postData
 * @return true if POST request completed
 */
resultCode_t http_post(httpCtrl_t *httpCtrl, const char* relativeUrl, bool returnResponseHdrs, const char* postData, uint16_t dataSz);


/**
 * @brief Retrieves page results from a previous GET or POST.

 * @param [in] httpCtrl Pointer to the control block for HTTP communications.
 * @return HTTP status of read.
 */
uint16_t http_readPage(httpCtrl_t *httpCtrl);


/**
 * @brief Retrieves page results from a previous GET or POST.

 * @param [in] httpCtrl Pointer to the control block for HTTP communications.
 * @param [in] filename C-string containing the name of the file to create from page content.
 * @param [in] filesz Approximate file size for read, used to calculate command timeout. 0=use default timeout of http__readToFileTimeoutSec. 
 * @return HTTP status of read.
 */
uint16_t http_readPageToFile(httpCtrl_t *httpCtrl, const char* filename, uint32_t filesz);


/**
 * @brief Cancels a http_readPage flow if the remaining contents are not needed.
 * @details This is a blocking call. The page read off the network will continue, but the contents will be discarded.

 * @param [in] httpCtrl Pointer to the control block for HTTP communications.
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
