// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef _FILE_H_
#define _FILE_H_
//#include <stdint.h>
#include "ltem1c.h"


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


// set file read data receiver function (here or with file_open). Not required if file is write only access.
void file_setRecvrFunc(fileReceiver_func_t fileRecvr_func);

fileInfoResult_t file_info();
fileListResult_t file_list(const char* fileName);
resultCode_t file_delete(const char* fileName);
fileOpenResult_t file_open(const char* fileName, fileOpenMode_t mode, fileReceiver_func_t fileRecvr_func);
resultCode_t file_read(uint16_t fileHandle, uint16_t readSz);
fileWriteResult_t file_write(uint16_t fileHandle, const char* writeData, uint16_t writeSz);
resultCode_t file_seek(uint16_t fileHandle, uint32_t offset, fileSeekMode_t seekFrom);
filePositionResult_t file_getPosition(uint16_t fileHandle);
resultCode_t file_truncate(uint16_t fileHandle);
resultCode_t file_close(uint16_t fileHandle);

/* future */
// fileUploadResult_t file_upload(const char* fileName);
// fileDownloadResult_t file_download(const char* fileName);


#ifdef __cplusplus
}
#endif

#endif  //!_FILE_H_
