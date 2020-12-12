// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef _HTTP_H_
#define _FILE_H_

#include "ltem1c.h"


/** 
 *  \brief typedef for the socket services data receiver function. Connects filesystem processing to the application (receive).
*/
typedef void (*httpReceiver_func_t)(void *fileData, uint16_t dataSz);


#ifdef __cplusplus
extern "C" {
#endif


// set file read data receiver function (here or with file_open). Not required if file is write only access.
void http_setRecvrFunc(httpReceiver_func_t fileRecvr_func);

resultCode_t http_setServerUrl(const char* fileName);
resultCode_t http_setSslOptions();

resultCode_t http_get(const char* url, httpReceiver_func_t fileRecvr_func);
resultCode_t http_getFile(const char* url, const char* filename);
resultCode_t http_post(const char* url, const char* postData, uint16_t dataSz);
resultCode_t http_postFile(const char* url, const char* filename);


#ifdef __cplusplus
}
#endif

#endif  //!_HTTP_H_
