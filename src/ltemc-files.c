/** ****************************************************************************
  \file 
  \brief Public API persistent file system support
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


#define _DEBUG 2                                // set to non-zero value for PRINTF debugging output, 
// debugging output options                     // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if defined(_DEBUG) && _DEBUG > 0
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG >= 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #define PRINTF(c_,f_,__VA_ARGS__...) do { rtt_printf(c_, (f_), ## __VA_ARGS__); } while(0)
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#endif


#define SRCFILE "FIL"                           // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT
#include "ltemc-internal.h"
#include "ltemc-files.h"

extern ltemDevice_t g_lqLTEM;


#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

/* Local Static Functions
------------------------------------------------------------------------------------------------------------------------- */
static cmdParseRslt_t S__writeStatusParser();
static resultCode_t S__filesRxHndlr();



/**
 *	@brief Set the data callback function for filedata.
 */
void file_setAppReceiver(fileReceiver_func fileReceiver)
{
    ASSERT(fileReceiver != NULL);
    g_lqLTEM.fileCtrl->appRecvDataCB = fileReceiver;
}


/**
 *	@brief get filesystem information.
 */
resultCode_t file_getFSInfo(filesysInfo_t * fsInfo)
{
    resultCode_t rslt;
    char *workPtr;

    if (!ATCMD_awaitLock(atcmd__defaultTimeout))
        return resultCode__conflict;                                    // failed to get lock

    do
    {
        // first get file system info
        atcmd_invokeReuseLock("AT+QFLDS=\"UFS\"");
        rslt = atcmd_awaitResult();
        if (rslt != RESULT_CODE_SUCCESS)
        {
            break;
        }
        // parse response >>  +QFLDS: <freesize>,<total_size>
        workPtr = atcmd_getResponse() + file__dataOffset_info;    // skip past +QFLDS: 
        fsInfo->freeSz = strtol(workPtr, &workPtr, 10);  
        fsInfo->totalSz = strtol(++workPtr, &workPtr, 10);      // incr past comma, then grab total fileSystem size

        // now get file collection info
        atcmd_invokeReuseLock("AT+QFLDS");
        rslt = atcmd_awaitResult();
        if (rslt != resultCode__success)
        {
            break;
        }
        // parse response
        // +QFLDS: <freesize>,<total_size>
        workPtr = atcmd_getResponse() + file__dataOffset_info;    // skip past +QFLDS: 
        fsInfo->filesSz = strtol(workPtr, &workPtr, 10);  
        fsInfo->filesCnt = strtol(++workPtr, &workPtr, 10);     // incr past comma, then grab total fileSystem size
    } while (0);
    
    atcmd_close();
    return rslt;
}


resultCode_t file_getFilelist(fileListResult_t *fileList, const char* filename)
{

    resultCode_t rslt;
    do
    {
        if (!ATCMD_awaitLock(atcmd__defaultTimeout))
        {
            rslt = resultCode__conflict;                                    // failed to get lock
            break;
        }

        if (strlen(filename) == 0)
        {
            fileList->namePattern[0] = '*';
            fileList->namePattern[1] = '\0';
            atcmd_invokeReuseLock("AT+QFLST");
        }
        else
        {
            strncpy(fileList->namePattern, filename, MIN(strlen(filename), file__filenameSz));
            atcmd_invokeReuseLock("AT+QFLST=\"%s\"", fileList->namePattern);
        }
        rslt = atcmd_awaitResult();
        if (rslt != RESULT_CODE_SUCCESS)
            break;

        // parse response >>  +QFLST: <filename>,<file_size>
        uint8_t lineNm = 0;
        char *workPtr = atcmd_getResponse();

        for (size_t i = 0; i < file__fileListMaxCnt; i++)
        {
            workPtr += 9;
            uint8_t len = strchr(workPtr, '"') - workPtr;
            strncpy(fileList->files[i].filename, workPtr, len);
            workPtr += len + 2;
            fileList->files[i].fileSz = strtol(workPtr, &workPtr, 10);

            workPtr += 2;
            if (*workPtr != '+')
            {
                fileList->fileCnt = ++i;
                break;
            }
        }
    } while (0);

    atcmd_close();
    return rslt;
}


resultCode_t file_open(const char* filename, fileOpenMode_t openMode, uint16_t* fileHandle)
{
    ASSERT(strlen(filename) > 0);
    resultCode_t rslt;
    char *workPtr;

    do
    {
        if (!atcmd_tryInvoke("AT+QFOPEN=\"%s\",%d", filename, openMode))
        {
            return resultCode__conflict;
        }
        
        rslt = atcmd_awaitResult();
        if (rslt != resultCode__success)
        {
            if (rslt == resultCode__cmError)
            {
                uint16_t errDetail = atcmd_getErrorDetailCode();
                if (errDetail == fileErr__detail_fileAlreadyOpen)
                    rslt = fileErr__result_fileAlreadyOpen;
                else if (errDetail == fileErr__detail_fileAlreadyOpen)
                    rslt = fileErr__result_fileAlreadyOpen;
            }
            break;
        }
        // parse response >> +QFOPEN: <filehandle>
        workPtr = atcmd_getResponse() + file__dataOffset_open;
        *fileHandle = strtol(workPtr, NULL, 10);

        // ASSERT(fileHandle > 0);
        break;
    } while (0);

    atcmd_close();
    return rslt;
}


/**
 *	@brief Get a list of open files, including their mode and file handles.
 */
resultCode_t file_getOpenFiles(char *fileInfo, uint16_t fileInfoSz)
{
    if (atcmd_tryInvoke("AT+QFOPEN?"))
    {
        char* workPtr;
        char* eolPtr;
        memset(fileInfo, 0, fileInfoSz);                            // init for c-str behavior

        if (atcmd_awaitResult() == resultCode__success)
        {
            workPtr = atcmd_getResponse();                          // ptr to response
            while (memcmp(workPtr, "+QFOPEN: ", file__dataOffset_open) == 0)
            {
                workPtr += file__dataOffset_open;
                eolPtr = strchr(workPtr, '\r');
                memcpy(fileInfo, workPtr, eolPtr - workPtr);
                fileInfo += eolPtr - workPtr;
                *fileInfo = '\r';
                fileInfo++;
                workPtr = eolPtr + 2;
            }
            return resultCode__success;
        }
    }
    return resultCode__conflict;
}


/**
 *	@brief Closes the file. 
 */
resultCode_t file_close(uint16_t fileHandle)
{
    if (atcmd_tryInvoke("AT+QFCLOSE=%d", fileHandle))
    {
        return atcmd_awaitResult();
    }
    return resultCode__conflict;
}


/**
 *	@brief Closes all open files. 
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_closeAll()
{
    char openList[file__openFileItemSz * file__openFileMaxCnt];

    if (file_getOpenFiles(openList, file__openFileItemSz * file__openFileMaxCnt))
    {
        char *workPtr = openList;
        while (*workPtr != 0)
        {
            workPtr = memchr(workPtr, ',', file__openFileItemSz) + 1;
            uint8_t fHandle = strtol(workPtr, &workPtr, 10);
            if (fHandle == 0 || fHandle > file__openFileMaxCnt)
            {
                return resultCode__internalError;
            }
            file_close(fHandle);
            workPtr = memchr(workPtr, '\r', file__openFileItemSz) + 2;
        }
        return resultCode__success;
    }
    return resultCode__conflict;
}


resultCode_t file_read(uint16_t fileHandle, uint16_t readSz)
{
    resultCode_t rslt = resultCode__success;
    ASSERT(g_lqLTEM.fileCtrl->appRecvDataCB);

    if (readSz > 0)
        rslt = atcmd_tryInvoke("AT+QFREAD=%d,%d", fileHandle, readSz);
    else
        rslt = atcmd_tryInvoke("AT+QFREAD=%d", fileHandle);

    if (rslt)
    {
        atcmd_setStreamControl("CONNECT", g_lqLTEM.fileCtrl);
        g_lqLTEM.fileCtrl->handle = fileHandle;
        return atcmd_awaitResult() == resultCode__success;                     // dataHandler will be invoked by atcmd module and return a resultCode
    }
    return resultCode__conflict;
}


resultCode_t file_write(uint16_t fileHandle, const char* writeData, uint16_t writeSz, fileWriteResult_t *writeResult)
{
    resultCode_t rslt;
    char *workPtr;

    if (!ATCMD_awaitLock(atcmd__defaultTimeout))
        return resultCode__conflict;                                                            // failed to get lock

    do
    {
        atcmd_invokeReuseLock("AT+QFWRITE=%d,%d", fileHandle, writeSz);
        rslt = atcmd_awaitResultWithOptions(PERIOD_FROM_SECONDS(5), ATCMD_connectPromptParser);
        if (rslt == resultCode__success)                                                        // "CONNECT" prompt result
        {
            atcmd_reset(false);                                                                 // clear CONNECT event from atcmd results
            atcmd_sendCmdData(writeData, writeSz);
        }

        rslt = atcmd_awaitResultWithOptions(atcmd__defaultTimeout, S__writeStatusParser);       // wait for "+QFWRITE result
        if (rslt == resultCode__success)
        {
            writeResult->writtenSz = strtol(atcmd_getResponse(), &workPtr, 10);
            writeResult->fileSz = strtol(++workPtr, NULL, 10);
        }
    } while (0);

    atcmd_close();
    return rslt;
}


/**
 *	@brief Set the position of the file pointer.
 */
resultCode_t file_seek(uint16_t fileHandle, uint32_t offset, fileSeekMode_t seekFrom)
{
    if (atcmd_tryInvoke("AT+QFSEEK=%d,%d,%d", fileHandle, offset, seekFrom))
    {
        return atcmd_awaitResult();
    }
    return resultCode__conflict;
}


resultCode_t file_getPosition(uint16_t fileHandle, uint32_t* filePtr)
{
    resultCode_t rslt;
    char *workPtr;

    if (atcmd_tryInvoke("AT+QFPOSITION=%d", fileHandle))
    {
        rslt = atcmd_awaitResult();
        if (rslt != resultCode__success)
            return rslt;

        // parse response
        workPtr = atcmd_getResponse() + file__dataOffset_pos;           // skip past preamble 
        *filePtr = strtol(workPtr, NULL, 10);
    }
    return rslt;
}


/**
 *	@brief Truncate all the data beyond the CURRENT position of the file pointer.
 */
resultCode_t file_truncate(uint16_t fileHandle)
{
    if (atcmd_tryInvokeAdv("AT+QFTUCAT=%d", fileHandle))
    {
        return atcmd_awaitResult();
    }
    return resultCode__conflict;
}


/**
 *	@brief Delete a file from the file system.
 */
resultCode_t file_delete(const char* filename)
{
    if (atcmd_tryInvoke("AT+QFDEL=%s", filename))
    {
        return atcmd_awaitResult();
    }
    return resultCode__conflict;
}


/* Possible future API methods
*/
// fileUploadResult_t file_upload(const char* filename) {}
// fileDownloadResult_t file_download(const char* filename) {}


#pragma Static Helpers and Response Parsers
/*
 * --------------------------------------------------------------------------------------------- */


static cmdParseRslt_t S__writeStatusParser() 
{
    // +QFWRITE: <written_length>,<total_length>
    return atcmd_stdResponseParser("+QFWRITE: ", true, ",", 0, 1, "\r\n", 0);
}


/**
 * @brief File stream RX data handler, marshalls incoming data from RX buffer to app (application).
 * 
 * @return resultCode_t 
 */
static resultCode_t S__filesRxHndlr()
{
    char wrkBffr[32];
    fileCtrl_t* fileCtrl = S__getFileCtrl();
    ASSERT(fileCtrl);
    
    uint8_t popCnt = cbffr_find(g_lqLTEM.iop->rxBffr, "\r", 0, 0, false);
    if (popCnt == CBFFR_NOFIND)
    {
        return resultCode__internalError;
    }
    
    cbffr_pop(g_lqLTEM.iop->rxBffr, wrkBffr, popCnt + 2);                                               // pop CONNECT phrase for parsing data length
    uint16_t readSz = strtol(wrkBffr + 8, NULL, 10);
    uint16_t streamSz = readSz + file__readTrailerSz;

    PRINTF(dbgColor__cyan, "filesDataRcvr() fHandle=%d sz=%d\r", fileCtrl->handle, streamSz);
    while (streamSz > 0)
    {
        uint32_t readTimeout = pMillis();
        uint16_t occupiedCnt;
        do
        {
            occupiedCnt = cbffr_getOccupied(g_lqLTEM.iop->rxBffr);
            if (pMillis() - readTimeout > file__readTimeoutMs)
            {
                return resultCode__timeout;
            }
        } while (occupiedCnt == 0);
        
        if (readSz > 0)                                                                                         // read content, forward to app
        {
            char* streamPtr;
            uint16_t blockSz = cbffr_popBlock(g_lqLTEM.iop->rxBffr, &streamPtr, readSz);                        // get address from rxBffr
            PRINTF(dbgColor__cyan, "filesRxHndlr() ptr=%p, bSz=%d, rSz=%d\r", streamPtr, blockSz, readSz);
            ((fileReceiver_func)(*fileCtrl->appRecvDataCB))(fileCtrl->handle, streamPtr, blockSz);                  // forward to application
            cbffr_popBlockFinalize(g_lqLTEM.iop->rxBffr, true);                                                 // commit POP
            readSz -= blockSz;
            streamSz -= blockSz;
        }

        if (cbffr_getOccupied(g_lqLTEM.iop->rxBffr) >= file__readTrailerSz)                                     // cleanup, remove trailer
        {
            cbffr_skipTail(g_lqLTEM.iop->rxBffr, file__readTrailerSz);
        }
    }
    return resultCode__success;
}


#pragma endregion