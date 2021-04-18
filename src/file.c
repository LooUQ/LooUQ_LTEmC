// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

//#define _DEBUG
#include "ltem1c.h"
#include "file.h"

#define FILE_CMD_SZ             81
#define FILE_INFO_DATAOFFSET    10
#define FILE_POS_DATAOFFSET     12      ///< +QFPOSITION: 
#define FILE_OPEN_DATAOFFSET     9      ///< +QFOPEN: <filehandle>
#define FILE_TIMEOUTml         800


void file_setRecvrFunc(fileReceiver_func_t fileRecvr_func)
{
}


fileInfoResult_t file_info()
{
    fileInfoResult_t fileResult;
    char *continueAt;

    // first get file system info
    if (action_tryInvokeAdv("AT+QFLDS=\"UFS\"", FILE_TIMEOUTml, NULL))
    {
        actionResult_t atResult = action_awaitResult(false);
        if (atResult.statusCode != RESULT_CODE_SUCCESS)
        {
            fileResult.resultCode = atResult.statusCode;
            action_close();
            return fileResult;
        }
        // parse response
        // +QFLDS: <freesize>,<total_size>
        continueAt = atResult.response + FILE_INFO_DATAOFFSET;      // skip past +QFLDS: 
        fileResult.freeSz = strtol(continueAt, &continueAt, 10);  
        fileResult.totalSz = strtol(++continueAt, &continueAt, 10); // inc past comma, then grab total fileSystem size
        action_close();
    }
    else
    {
        fileResult.resultCode = RESULT_CODE_CONFLICT;
        return fileResult;
    }
    // now get file collection info
    if (action_tryInvokeAdv("AT+QFLDS", FILE_TIMEOUTml, NULL))
    {
        actionResult_t atResult = action_awaitResult(false);
        if (atResult.statusCode != RESULT_CODE_SUCCESS)
        {
            fileResult.resultCode = atResult.statusCode;
            action_close();
            return fileResult;
        }
        // parse response
        // +QFLDS: <freesize>,<total_size>
        continueAt = atResult.response + FILE_INFO_DATAOFFSET;      // skip past +QFLDS: 
        fileResult.filesSz = strtol(continueAt, &continueAt, 10);  
        fileResult.filesCnt = strtol(++continueAt, &continueAt, 10); // inc past comma, then grab total fileSystem size
        action_close();
    }
    else
    {
        fileResult.resultCode = RESULT_CODE_CONFLICT;
        return fileResult;
    }
}


fileListResult_t file_list(const char* fileName)
{
}


/**
 *	\brief Delete a file from the file system.
 *
 *	\param [in] namePattern - "*" or filename to delete. Wildcard with filename is not allowed.
 * 
 *  \return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_delete(const char* fileName)
{
    char fileCmd[FILE_CMD_SZ] = {0};

    snprintf(fileCmd, FILE_CMD_SZ, "AT+QFDEL=%s", fileName);

    if (action_tryInvokeAdv(fileCmd, FILE_TIMEOUTml, NULL))
    {
        return action_awaitResult(true).statusCode;
    }
    return RESULT_CODE_CONFLICT;
}


// fileUploadResult_t file_upload(const char* fileName)
// {
// }
// fileDownloadResult_t file_download(const char* fileName)
// {
// }


fileOpenResult_t file_open(const char* fileName, fileOpenMode_t openMode, fileReceiver_func_t fileRecvr_func)
{
    fileOpenResult_t fileResult = { 0, RESULT_CODE_BADREQUEST };
    char *continueAt;
    char fileCmd[FILE_CMD_SZ] = {0};

    if (strlen(fileName) == 0)
    {
        return fileResult;
    }

    snprintf(fileCmd, FILE_CMD_SZ, "AT+QFOPEN=%s,%d", fileName, openMode);

    // first get file system info
    if (action_tryInvokeAdv(fileCmd, FILE_TIMEOUTml, NULL))
    {
        actionResult_t atResult = action_awaitResult(false);
        if (atResult.statusCode != RESULT_CODE_SUCCESS)
        {
            fileResult.resultCode = atResult.statusCode;
            action_close();
            return fileResult;
        }
        // parse response
        // +QFOPEN: <filehandle>
        continueAt = atResult.response + FILE_INFO_DATAOFFSET;      // skip past +QFLDS: 
        fileResult.fileHandle = strtol(continueAt, &continueAt, 10);  
        action_close();
    }
    else
    {
        fileResult.resultCode = RESULT_CODE_CONFLICT;
        return fileResult;
    }
}


resultCode_t file_read(uint16_t fileHandle, uint16_t readSz)
{
}


fileWriteResult_t file_write(uint16_t fileHandle, const char* writeData, uint16_t writeSz)
{
}


/**
 *	\brief Set the position of the file pointer.
 *
 *	\param [in] fileHandle - Numeric handle for the file to operate with.
 *	\param [in] offset - Number of bytes to move the file pointer by.
 *	\param [in] seekFrom - The starting point for the pointer movement (positive only).
 * 
 *  \return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_seek(uint16_t fileHandle, uint32_t offset, fileSeekMode_t seekFrom)
{
    char fileCmd[FILE_CMD_SZ] = {0};

    snprintf(fileCmd, FILE_CMD_SZ, "AT+QFSEEK=%d,%d,%d", fileHandle, offset, seekFrom);

    if (action_tryInvokeAdv(fileCmd, FILE_TIMEOUTml, NULL))
    {
        return action_awaitResult(true).statusCode;
    }
    return RESULT_CODE_CONFLICT;
}


filePositionResult_t file_getPosition(uint16_t fileHandle)
{
    filePositionResult_t fileResult;
    char fileCmd[FILE_CMD_SZ] = {0};
    char *continueAt;

    snprintf(fileCmd, FILE_CMD_SZ, "AT+QFPOSITION=%d", fileHandle);
    if (action_tryInvokeAdv(fileCmd, FILE_TIMEOUTml, NULL))
    {
        actionResult_t atResult = action_awaitResult(false);
        if (atResult.statusCode != RESULT_CODE_SUCCESS)
        {
            fileResult.resultCode = atResult.statusCode;
            action_close();
            return fileResult;
        }
        // parse response
        continueAt = atResult.response + FILE_POS_DATAOFFSET;           // skip past +QFPOSITION: 
        fileResult.fileOffset = strtol(continueAt, &continueAt, 10);  
        action_close();
    }
    else
    {
        fileResult.resultCode = RESULT_CODE_CONFLICT;
        return fileResult;
    }
}


/**
 *	\brief Truncate all the data beyond the current position of the file pointer.
 *
 *	\param [in] fileHandle - Numeric handle for the file to truncate.
 * 
 *  \return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_truncate(uint16_t fileHandle)
{
    char fileCmd[FILE_CMD_SZ] = {0};

    snprintf(fileCmd, FILE_CMD_SZ, "AT+QFTUCAT=%d", fileHandle);

    if (action_tryInvokeAdv(fileCmd, FILE_TIMEOUTml, NULL))
    {
        return action_awaitResult(true).statusCode;
    }
    return RESULT_CODE_CONFLICT;
}


/**
 *	\brief Closes the file. 
 *
 *	\param [in] fileHandle - Numeric handle for the file to close.
 * 
 *  \return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_close(uint16_t fileHandle)
{
    char fileCmd[FILE_CMD_SZ] = {0};

    snprintf(fileCmd, FILE_CMD_SZ, "AT+QFCLOSE=%d", fileHandle);

    if (action_tryInvokeAdv(fileCmd, FILE_TIMEOUTml, NULL))
    {
        return action_awaitResult(true).statusCode;
    }
    return RESULT_CODE_CONFLICT;
}

