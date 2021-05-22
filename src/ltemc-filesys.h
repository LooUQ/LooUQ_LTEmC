/******************************************************************************
 *  \file ltemc-filesys.h
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
 * Use BGx internal flash excess capacity as filesystem for device application 
 *****************************************************************************/

#ifndef __LTEMC_FILESYS_H__
#define __LTEMC_FILESYS_H__

#include "ltemc.h"


typedef enum fileInfoType_tag
{
    fileInfoType_fileSystem = 0,
    fileInfoType_file = 1
} fileInfoType_t;


typedef struct fileInfoResult_tag
{
    uint32_t freeSz;
    uint32_t totalSz;
    uint32_t filesSz;
    uint16_t filesCnt;
    resultCode_t resultCode;
} fileInfoResult_t;


typedef struct fileListItem_tag
{
    const char* filename;
    uint32_t fileSize;
} fileListItem_t;


typedef struct fileListResult_tag
{
    const char* namePattern;
    fileListItem_t fileList[10];
    resultCode_t resultCode;
} fileListResult_t;

typedef struct fileUploadResult_tag
{
    uint32_t size;
    uint16_t checksum;
} fileUploadResult_t;


typedef struct fileDownloadResult_tag
{
    uint32_t size;
    uint16_t checksum;
} fileDownloadResult_t;


typedef enum fileOpenMode_tag
{
    fileOpenMode_normalRdWr = 0,
    fileOpenMode_clearRdWr = 1,
    fileOpenMode_normalRdOnly = 2
} fileOpenMode_t;


typedef enum fileSeekMode_tag
{
    fileSeekMode_seekFromBegin = 0,
    fileSeekMode_seekFromCurrent = 1,
    fileSeekMode_seekFromEnd = 2
} fileSeekMode_t;


typedef struct fileOpenResult_tag
{
    uint16_t fileHandle;
    resultCode_t resultCode;
} fileOpenResult_t;


typedef struct fileWriteResult_tag
{
    uint16_t writtenSz;
    uint32_t fileSz;
    resultCode_t resultCode;
} fileWriteResult_t;


typedef struct filePositionResult_tag
{
    uint16_t fileOffset;
    resultCode_t resultCode;
} filePositionResult_t;

/** 
 *  \brief typedef for the socket services data receiver function. Connects filesystem processing to the application (receive).
*/
typedef void (*fileReceiver_func_t)(uint16_t fileHandle, void *fileData, uint16_t dataSz);


#ifdef __cplusplus
extern "C" {
#endif


// set file read data receiver function (here or with filsys_open). Not required if file is write only access.
void filsys_setRecvrFunc(fileReceiver_func_t fileRecvr_func);

fileInfoResult_t filesys_info();
fileListResult_t filsys_list(const char* fileName);
resultCode_t filsys_delete(const char* fileName);
fileOpenResult_t filsys_open(const char* fileName, fileOpenMode_t mode, fileReceiver_func_t fileRecvr_func);
resultCode_t filsys_read(uint16_t fileHandle, uint16_t readSz);
fileWriteResult_t filsys_write(uint16_t fileHandle, const char* writeData, uint16_t writeSz);
resultCode_t filsys_seek(uint16_t fileHandle, uint32_t offset, fileSeekMode_t seekFrom);
filePositionResult_t filsys_getPosition(uint16_t fileHandle);
resultCode_t filsys_truncate(uint16_t fileHandle);
resultCode_t filsys_close(uint16_t fileHandle);

/* future */
// fileUploadResult_t filsys_upload(const char* fileName);
// fileDownloadResult_t filsys_download(const char* fileName);


#ifdef __cplusplus
}
#endif

#endif  // !__LTEMC_FILESYS_H__
