/** ***************************************************************************
  @file 
  @brief Modem filesystem storage features/services.

  @author Greg Terrell, LooUQ Incorporated

  \loouq
-------------------------------------------------------------------------------

LooUQ-LTEmC // Software driver for the LooUQ LTEm series cellular modems.
Copyright (C) 2022-2023 LooUQ Incorporated

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


#ifndef __LTEMC_FILES_H__
#define __LTEMC_FILES_H__

#include "ltemc.h"


/*
------------------------------------------------------------------------------------------------ */

/**
 * @brief file module constants
 */
enum file
{
    file__filenameSz = 81,
    file__timeoutMS = 800,                                          ///< file system command default timeout (milliseconds)
    file__fileListMaxCnt = 32,
    file__openFileItemSz = 28,
    file__openFileMaxCnt = 10,
    
    file__dataOffset_info = 7,                                      ///< +QFLDS and +QFLST
    file__dataOffset_open = 9,                                      ///< +QFOPEN: "filename",{handle},{mode}
    file__handleSearchMax = 20,
    file__dataOffset_pos = 13                                       ///< +QFPOSITION: 
};


/**
 * @brief file error map
 */
enum fileErrMap
{
    fileErr__detail_fileAlreadyOpen = 426,
    fileErr__result_fileAlreadyOpen = 409,

    fileErr__detail_fileNotFound = 405,
    fileErr__result_fileNotFound = 404
};

/**
 * @brief Describes the returned type of file object
 */
typedef enum fileInfoType_tag
{
    fileInfoType_fileSystem = 0,
    fileInfoType_file = 1
} fileInfoType_t;


/**
 * @brief Summary structure describing the LTEm file system and contents
 */
typedef struct filesysInfo_tag
{
    uint32_t freeSz;                                                ///< Free space available on LTEm device for file system
    uint32_t totalSz;                                               ///< Total capacity of LTEm device for file system
    uint32_t filesSz;                                               ///< Summation occupied space for all files
    uint16_t filesCnt;                                              ///< Total number of files in the file system
} filesysInfo_t;


/**
 * @brief Structure describing an individual item returned in a file list
 */
typedef struct fileListItem_tag
{
    char filename[file__filenameSz];                                ///< Filename of the item
    uint32_t fileSz;                                                ///< File's size on the filesystem
} fileListItem_t;


/**
 * @brief Summary result of a file list query
 */
typedef struct fileListResult_tag
{
    char namePattern[file__filenameSz];                             ///< Searched pattern
    uint8_t fileCnt;                                                ///< Number of matches returned
    fileListItem_t files[file__fileListMaxCnt];                     ///< Collection of file objects for the matched files
} fileListResult_t;


/**
 * @brief Returned result object from a file I/O request (upload, download, write)
 */
typedef struct fileResult_tag
{
    uint32_t ioSize;                                                ///< Number of bytes read/written 
    uint16_t checksum;                                              ///< Module calculated checksum of the upload
    uint32_t filesize;                                              ///< resulting file size
} fileResult_t;


/**
 * @brief File open mode, controls file I/O behavior until file is closed
 */
typedef enum fileOpenMode_tag
{
    fileOpenMode_rdWr = 0,                                          ///< Open file for read/write, no change to file at open
    fileOpenMode_ovrRdWr = 1,                                       ///< Open file for read/write, TRUNCATE file at open
    fileOpenMode_rdOnly = 2                                         ///< Open file for read-only
} fileOpenMode_t;

/**
 * @brief When seeking to a new position within a file, the starting point
 */
typedef enum fileSeekMode_tag
{
    fileSeekMode_fromBegin = 0,                                     ///< Move file pointer (position) relative to the start of the file
    fileSeekMode_fromCurrent = 1,                                   ///< Move file pointer (position) relative to the current position
    fileSeekMode_fromEnd = 2                                        ///< Move file pointer (position) relative to the last byte position of the file
} fileSeekMode_t;



// /** 
//  *  @brief typedef for the filesystem services data receiver function. Connects filesystem processing to the application (receive).
// */
// typedef void (*genericAppRcvr_func)(uint8_t context, const char *fileData, uint16_t dataSz);


#ifdef __cplusplus
extern "C" {
#endif

void file_setAppReceiver(appRcvr_func fileReceiver);


/**
 *	@brief get filesystem information.
 */
resultCode_t file_getFSInfo(filesysInfo_t * fsInfo);


/**
 *	@brief get list of files from filesystem.
 */
resultCode_t file_getFilelist(fileListResult_t *filelist, const char* fileName);


// /**
//  *	@brief set file read data receiver function (here or with filesys_open). Not required if file is write only access.
//  */
// void file_setRecvrFunc(fileReceiver_func fileReceiver);


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
 *
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_closeAll();


/**
 *	@brief Closes the file. 
 *
 *	@param [in] fileHandle - Numeric handle for the file to close.
 *	@param [in] requestSz - Requested number of bytes to read from the file.
 *	@param [out] readSz - Number of bytes actually read from file.
 *  @return resultCode_t 200==successful, otherwise error code (HTTP status type).
 */
resultCode_t file_read(uint16_t fileHandle, uint16_t requestSz, uint16_t* readSz);


/**
 *	@brief Closes the file. 
 *	@param [in] fileHandle - Numeric handle for the file to write/update.
 *	@param [in] writeData Pointer to char array containing data to write to file.
 *	@param [in] requestSz - Number of bytes in buffer to write to file.
 *	@param [in] writeSz - Numberic of bytes actually written to file.
 *  @return resultCode_t 200==successful, otherwise error code (HTTP status type).
 */
resultCode_t file_write(uint16_t fileHandle, const char* writeData, uint16_t requestSz, fileResult_t *writeSz);


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
 *	@param [in] fileName "*" or filename to delete. Wildcard with filename is not allowed.
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_delete(const char* fileName);

/**
 * @brief Create a timestamp based filename, with optional suffix/extension
 * 
 * @param [in] tsFilename char-array for filename 
 * @param [in] fnSize size of filename char array, array will be initialized to /0
 * @param [in] suffix Optional suffix to add to timestamp section of filename
 */
void file_getTsFilename(char* tsFilename, uint8_t fnSize, const char* suffix);


/* future */
// fileUploadResult_t file_upload(const char* fileName);
// fileDownloadResult_t file_download(const char* fileName);


#ifdef __cplusplus
}
#endif

#endif  // !__LTEMC_FILES_H__
