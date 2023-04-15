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


#ifndef __LTEMC_FILESYS_H__
#define __LTEMC_FILESYS_H__

#include "ltemc.h"






/*
------------------------------------------------------------------------------------------------ */

enum filesys
{
    filesys__cmdSz = 81,
    filesys__dataOffsetInfo = 10,
    filesys__dataOffsetPosition = 12,   ///< +QFPOSITION: 
    filesys__dataOffsetOpen = 9,        ///< +QFOPEN: {filehandle}
    filesys__timeoutMS = 800            ///< file system command default timeout (milliseconds)
};


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
 *  @brief typedef for the socket services data receiver function. Connects filesystem processing to the application (receive).
*/
typedef void (*fileReceiver_func_t)(uint16_t fileHandle, void *fileData, uint16_t dataSz);


#ifdef __cplusplus
extern "C" {
#endif


/**
 *	@brief set file read data receiver function (here or with filesys_open). Not required if file is write only access.
 */
void filesys_setRecvrFunc(fileReceiver_func_t fileRecvr_func);


fileInfoResult_t filesys_info();


fileListResult_t filesys_list(const char* fileName);


/**
 *	@brief Delete a file from the file system.
 *	@param fileName [in] - "*" or filename to delete. Wildcard with filename is not allowed.
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t filesys_delete(const char* fileName);


fileOpenResult_t filesys_open(const char* fileName, fileOpenMode_t mode, fileReceiver_func_t fileRecvr_func);


resultCode_t filesys_read(uint16_t fileHandle, uint16_t readSz);


fileWriteResult_t filesys_write(uint16_t fileHandle, const char* writeData, uint16_t writeSz);


/**
 *	@brief Set the position of the file pointer.
 *
 *	@param [in] fileHandle - Numeric handle for the file to operate with.
 *	@param [in] offset - Number of bytes to move the file pointer by.
 *	@param [in] seekFrom - The starting point for the pointer movement (positive only).
 * 
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t filesys_seek(uint16_t fileHandle, uint32_t offset, fileSeekMode_t seekFrom);


filePositionResult_t filesys_getPosition(uint16_t fileHandle);


/**
 *	@brief Truncate all the data beyond the current position of the file pointer.
 *	@param [in] fileHandle - Numeric handle for the file to truncate.
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t filesys_truncate(uint16_t fileHandle);


/**
 *	@brief Closes the file. 
 *	@param [in] fileHandle - Numeric handle for the file to close.
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t filesys_close(uint16_t fileHandle);

/* future */
// fileUploadResult_t filesys_upload(const char* fileName);
// fileDownloadResult_t filesys_download(const char* fileName);


#ifdef __cplusplus
}
#endif

#endif  // !__LTEMC_FILESYS_H__
