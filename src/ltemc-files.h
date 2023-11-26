/** ***************************************************************************
  @file 
  @brief Modem filesystem storage features/services.

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


#ifndef __LTEMC_FILES_H__
#define __LTEMC_FILES_H__

#include "ltemc-iTypes.h"


enum file__constants_tag                            // file system constant values
{
    file__filenameSz = 81,                          // max length for a filesystem filename
    file__timeoutMS = 800,                          // file system command default timeout (milliseconds)
    file__fileListMaxCnt = 32,                      // max number of returned filesystem directory list
    file__openFileItemSz = 28,                      // 
    file__openFileMaxCnt = 10,                      // max number of files allowed to be concurrently open
    
    file__dataOffset_info = 7,                      // +QFLDS and +QFLST
    file__dataOffset_open = 9,                      // +QFOPEN: "filename",<handle>,<mode>
    file__handleSearchMax = 20,                     // 
    file__dataOffset_pos = 13,                      // +QFPOSITION: 
    file__readTrailerSz = 6,                        // 

    fileErr__fileAlreadyOpen = 409,                 // already open
    fileErr__fileNotFound = 404                     // not found
};


/**
 * @brief For file information request, the scope of the request
 */
typedef enum fileInfoType_tag
{
    fileInfoType_fileSystem = 0,                    // request for the filesystem as a whole
    fileInfoType_file = 1                           // request for info about a specific file
} fileInfoType_t;


/**
 * @brief File open modes, aka constraints on file actions on the opened file
 */
typedef enum fileOpenMode_tag
{
    fileOpenMode_rdWr = 0,                          // read/write
    fileOpenMode_ovrRdWr = 1,                       // read/overwrite (truncate file on write first)
    fileOpenMode_rdOnly = 2                         // read only
} fileOpenMode_t;


/**
 * @brief Starting point for file seek operations
 */
typedef enum fileSeekMode_tag
{
    fileSeekMode_fromBegin = 0,                     // the beginning of the file
    fileSeekMode_fromCurrent = 1,                   // current position pointer of the file
    fileSeekMode_fromEnd = 2                        // backwards from the end of the file
} fileSeekMode_t;


/**
 * @brief Filesystem application data receiver prototype
 */
typedef void (*fileRecv_func)(char* dataPtr, uint16_t dataSz, bool isFinal);  // stream callback to deliver data to application


/**
 * @brief Stream control for filesystem
 * @note Does NOT follow exact struct field layout of the other streams, shares 1st field to validate type before casting 
 */
typedef struct fileCtrl_tag
{
    dataCntxt_t dataCntxt;                  // stream context 
    streamType_t streamType;                // stream type (cast to char from enum )
    urcEvntHndlr_func urcHndlr;             // URC handler (invoke by eventMgr)
    dataHndlr_func dataRxHndlr;             // function to handle data streaming, initiated by eventMgr() or atcmd module
    appGenRcvr_func appRcvr;                // application receiver for incoming network data

    uint16_t fileHandle;                    // temporary storage of the file handle 
} fileCtrl_t;


/**
 * @brief Structure describing the filesystem status
 */
typedef struct filesysInfo_tag
{
    uint32_t freeSz;                                // number of bytes free (available)
    uint32_t totalSz;                               // total capacity in bytes of the filesystem
    uint32_t filesSz;                               // occupied file size in bytes
    uint16_t filesCnt;                              // number of files
} filesysInfo_t;


/**
 * @brief Struct for information about an individual file returned within a file listing
 */
typedef struct fileListItem_tag
{
    char filename[file__filenameSz];                // filename
    uint32_t fileSz;                                // size of file in bytes
} fileListItem_t;


/**
 * @brief Struct containing the information from a get file list
 */
typedef struct fileListResult_tag
{
    char namePattern[file__filenameSz];             // searched pattern
    uint8_t fileCnt;                                // number of files returned
    fileListItem_t files[file__fileListMaxCnt];     // collection of file info structures
} fileListResult_t;


/**
 * @brief Status result from a file upload
 */
typedef struct fileUploadResult_tag 
{
    uint32_t size;                                  // Size reported by module
    uint16_t checksum;                              // Checksum reported by module
} fileUploadResult_t;


/**
 * @brief Status result from a file download
 */
typedef struct fileDownloadResult_tag
{
    uint32_t size;                                  // Size of file (app to validate)
    uint16_t checksum;                              // Checksum of file (app to validate)
} fileDownloadResult_t;

/**
 * @brief Result structure describing a write operation
 * 
 */
typedef struct fileWriteResult_tag
{
    uint16_t writtenSz;                             // number of bytes written
    uint32_t fileSz;                                // the resulting number of bytes in the file
} fileWriteResult_t;


/** 
 *  @brief typedef for the filesystem services data receiver function. Connects filesystem processing to the application (receive).
*/
typedef void (*fileReceiver_func)(uint16_t fileHandle, const char *fileData, uint16_t dataSz);


#ifdef __cplusplus
extern "C" {
#endif
/* ============================================================================================= */

/**
 * @brief Set the application read data receiver callback function
 * @details Creates a singleton fileCtrl_t if not previously created/initialized
 * 
 * @param fileReceiver The application (app) receive callback function
 * @return appGenRcvr_func* Returns a pointer to the recv function if successful, NULL if failed
 */
appGenRcvr_func* file_setAppReceiver(fileReceiver_func fileReceiver);


/**
 * @brief get filesystem information.
 * @param [out] fsInfo Struct pointer to be filled with the file system information
 */
resultCode_t file_getFSInfo(filesysInfo_t * fsInfo);


/**
 * @brief Get a list of files from filesystem.
 * @param [in] namePattern Char-array specifying the filename pattern to return
 * @param [out] filelist Struct pointer to the file list information
 * @return resultCode_t Success/failure of the operation
 */
resultCode_t file_getFilelist(const char* namePattern, fileListResult_t *filelist);


/**
 * @brief Open file for read/write.
 * @param [in] fileName - Char array with the name of the file to open (recommended DOS 8.3 format).
 * @param [in] openMode - Enum with file behavior after file is successfully opened.
 * @param [out] fileHandle - Pointer to the numeric handle for the file to close.
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_open(const char* fileName, fileOpenMode_t openMode, uint16_t* fileHandle);


/**
 * @brief Get a list of open files, including their mode and file handles.
 * @param [out] fileinfo - Char buffer to fill with information about open files.
 * @param [in] fileinfoSz - Buffer size (max chars to return).
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_getOpenFiles(char *fileinfo, uint16_t fileinfoSz);


/**
 * @brief Closes the file. 
 * @param [in] fileHandle - Numeric handle for the file to close.
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_close(uint16_t fileHandle);


/**
 * @brief Closes all open files.
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_closeAll();


/**
 * @brief Closes the file. 
 * @param [in] fileHandle - Numeric handle for the file to close.
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_read(uint16_t fileHandle, uint16_t requestSz, uint16_t* readSz);

/**
 * @brief Closes the file. 
 * @param [in] fileHandle - Numeric handle for the file to close.
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_write(uint16_t fileHandle, const char* writeData, uint16_t writeSz, fileWriteResult_t *writeResult);


/**
 * @brief Set the position of the file pointer.
 *
 * @param [in] fileHandle - Numeric handle for the file to operate with.
 * @param [in] offset - Number of bytes to move the file pointer by.
 * @param [in] seekFrom - The starting point for the pointer movement (positive only).
 * 
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_seek(uint16_t fileHandle, uint32_t offset, fileSeekMode_t seekFrom);


/**
 * @brief Closes the file. 
 * @param [in] fileHandle - Numeric handle for the file to close.
 * @param [out] filePtr - Pointer to location (integer file pointer) within file: "current".
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_getPosition(uint16_t fileHandle, uint32_t* filePtr);


/**
 * @brief Truncate all the data beyond the current position of the file pointer.
 * @param [in] fileHandle - Numeric handle for the file to truncate.
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_truncate(uint16_t fileHandle);


/**
 * @brief Delete a file from the file system.
 * @param [in] fileName "*" or filename to delete. Wildcard with filename is not allowed.
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

/* ============================================================================================= */
#ifdef __cplusplus
}
#endif

#endif  // !__LTEMC_FILES_H__
