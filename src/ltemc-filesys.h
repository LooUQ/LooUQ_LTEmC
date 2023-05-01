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


#ifndef __LTEMC_FILE_H__
#define __LTEMC_FILE_H__

#include "ltemc.h"


/*
------------------------------------------------------------------------------------------------ */

enum file
{
    file__filenameSz = 81,
    file__timeoutMS = 800,              /// file system command default timeout (milliseconds)
    file__fileListMaxCnt = 32,
    file__openFileItemSz = 28,
    file__openFileMaxCnt = 10,
    
    file__dataOffset_info = 7,          /// +QFLDS and +QFLST
    file__dataOffset_open = 9,          /// +QFOPEN: "filename",<handle>,<mode>
    file__handleSearchMax = 20,
    file__dataOffset_pos = 13,          /// +QFPOSITION: 
    file__readTrailerSz = 6,
    file__readTimeoutMs = 100
};


enum fileErrMap
{
    fileErr__detail_fileAlreadyOpen = 426,
    fileErr__result_fileAlreadyOpen = 409,

    fileErr__detail_fileNotFound = 405,
    fileErr__result_fileNotFound = 404
};


typedef enum fileInfoType_tag
{
    fileInfoType_fileSystem = 0,
    fileInfoType_file = 1
} fileInfoType_t;


typedef struct filesysInfo_tag
{
    uint32_t freeSz;
    uint32_t totalSz;
    uint32_t filesSz;
    uint16_t filesCnt;
} filesysInfo_t;


typedef struct fileListItem_tag
{
    char filename[file__filenameSz];
    uint32_t fileSz;
} fileListItem_t;


typedef struct fileListResult_tag
{
    char namePattern[file__filenameSz];
    uint8_t fileCnt;
    fileListItem_t files[file__fileListMaxCnt];
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
    fileOpenMode_rdWr = 0,
    fileOpenMode_ovrRdWr = 1,
    fileOpenMode_rdOnly = 2
} fileOpenMode_t;


typedef enum fileSeekMode_tag
{
    fileSeekMode_fromBegin = 0,
    fileSeekMode_fromCurrent = 1,
    fileSeekMode_fromEnd = 2
} fileSeekMode_t;


typedef struct fileWriteResult_tag
{
    uint16_t writtenSz;
    uint32_t fileSz;
} fileWriteResult_t;


/** 
 *  @brief typedef for the filesystem services data receiver function. Connects filesystem processing to the application (receive).
*/
typedef void (*fileReceiver_func_t)(uint16_t fileHandle, const char *fileData, uint16_t dataSz);


#ifdef __cplusplus
extern "C" {
#endif


/**
 *	@brief get filesystem information.
 */
resultCode_t file_getFSInfo(filesysInfo_t * fsInfo);


/**
 *	@brief get list of files from filesystem.
 */
resultCode_t file_getFilelist(fileListResult_t *filelist, const char* fileName);


/**
 *	@brief set file read data receiver function (here or with filesys_open). Not required if file is write only access.
 */
void file_setRecvrFunc(fileReceiver_func_t fileRecvr_func);


/**
 *	@brief Open file for read/write.
 *	@param [in] fileName - Char array with the name of the file to open (recommended DOS 8.3 format).
 *	@param [in] openMode - Enum with file behavior after file is successfully opened.
 *	@param [out] fileHandle - Pointer to the numeric handle for the file to close.
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_open(const char* fileName, fileOpenMode_t openMode, uint16_t* fileHandle);


/**
 *	@brief Get a list of open files, including their mode and file handles.
 *	@param [out] fileinfo - Char buffer to fill with information about open files.
 *	@param [in] fileinfoSz - Buffer size (max chars to return).
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_getOpenFiles(char *fileinfo, uint16_t fileinfoSz);


/**
 *	@brief Closes the file. 
 *	@param [in] fileHandle - Numeric handle for the file to close.
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_close(uint16_t fileHandle);


/**
 *	@brief Closes all open files.
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_closeAll();


/**
 *	@brief Closes the file. 
 *	@param [in] fileHandle - Numeric handle for the file to close.
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_read(uint16_t fileHandle, uint16_t readSz);


/**
 *	@brief Closes the file. 
 *	@param [in] fileHandle - Numeric handle for the file to close.
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_write(uint16_t fileHandle, const char* writeData, uint16_t writeSz, fileWriteResult_t *writeResult);


/**
 *	@brief Set the position of the file pointer.
 *
 *	@param [in] fileHandle - Numeric handle for the file to operate with.
 *	@param [in] offset - Number of bytes to move the file pointer by.
 *	@param [in] seekFrom - The starting point for the pointer movement (positive only).
 * 
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_seek(uint16_t fileHandle, uint32_t offset, fileSeekMode_t seekFrom);


/**
 *	@brief Closes the file. 
 *	@param [in] fileHandle - Numeric handle for the file to close.
 *	@param [out] filePtr - Pointer to location (integer file pointer) within file: "current".
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_getPosition(uint16_t fileHandle, uint32_t* filePtr);


/**
 *	@brief Truncate all the data beyond the current position of the file pointer.
 *	@param [in] fileHandle - Numeric handle for the file to truncate.
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_truncate(uint16_t fileHandle);


/**
 *	@brief Delete a file from the file system.
 *	@param fileName [in] - "*" or filename to delete. Wildcard with filename is not allowed.
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_delete(const char* fileName);



/* future */
// fileUploadResult_t file_upload(const char* fileName);
// fileDownloadResult_t file_download(const char* fileName);


#ifdef __cplusplus
}
#endif

#endif  // !__LTEMC_FILE_H__
