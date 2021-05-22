/******************************************************************************
 *  \file ltemc-geo.h
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020,2021 LooUQ Incorporated.
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

#include "ltemc.h"


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

#endif  // !__LTEMC_HTTP_H__
