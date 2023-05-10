/******************************************************************************
 *  \file ltemc-10-files.ino
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
 *  www.loouq.com
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
 * Test\demonstrate using the LTEm file system for persistent file storage.
 *****************************************************************************/

#define _DEBUG 2                        // set to non-zero value for PRINTF debugging output, 
// debugging output options             // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if _DEBUG > 0
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG >= 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #define PRINTF(c_,f_,__VA_ARGS__...) do { rtt_printf(c_, (f_), ## __VA_ARGS__); } while(0)
    #endif
#else
#define PRINTF(c_, f_, ...) 
#endif


/* specify the pin configuration
 * --------------------------------------------------------------------------------------------- */
// #define HOST_FEATHER_UXPLOR             
// #define HOST_FEATHER_LTEM3F
#define HOST_FEATHER_UXPLOR_L

#include <lq-diagnostics.h>
#include <lq-SAMDutil.h>                // allows read of reset cause

#include <ltemc.h>
#include <ltemc-files.h>

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

#define FILE2_SZ 1024
#define FILE3_SZ 2048

uint16_t fHandle;
char filedata2[FILE2_SZ];
char filedata3[FILE3_SZ];

resultCode_t rslt;
filesysInfo_t fsInfo;
fileListResult_t fileList;
fileWriteResult_t writeRslt;
uint16_t fHandle1;
uint16_t fHandle2;
uint16_t fHandle3;
char readData[256];

// for debugging access
#include "ltemc-internal.h"
extern ltemDevice_t g_lqLTEM;

void setup() {
    #ifdef SERIAL_OPT
        Serial.begin(115200);
        #if (SERIAL_OPT > 0)
        while (!Serial) {}      // force wait for serial ready
        #else
        delay(5000);            // just give it some time
        #endif
    #endif

    PRINTF(DBGCOLOR_red, "\rLTEmC test-10-filesystem\r");
    PRINTF(dbgColor__white, "RCause=%d\r\n", lqSAMD_getResetCause());
    platform_openPin(LED_BUILTIN, gpioMode_output);
    lqDiag_setNotifyCallback(applEvntNotify);

    ltem_create(ltem_pinConfig, NULL, applEvntNotify);
    ltem_start(resetAction_skipIfOn);                                            // start LTEm, if found on reset it

    // // create a test file images
    // createFileImage(file1, FILE1_SZ);
    // createFileImage(file2, FILE2_SZ);

    file_setAppReceiver(fileReadReceiver);

    rslt = file_getFSInfo(&fsInfo);
    PRINTF(dbgColor__green, "FileSystem: avail=%d, free=%d, fileCnt=%d, taken=%d\r", fsInfo.totalSz, fsInfo.freeSz, fsInfo.filesCnt, fsInfo.filesSz);

    rslt = file_getFilelist(&fileList, "");
    PRINTF(dbgColor__green, "\r\rFiles\r");
    for (size_t i = 0; i < fileList.fileCnt; i++)
    {
        PRINTF(dbgColor__white, "FN=%s sz=%d\r", fileList.files[i].filename, fileList.files[i].fileSz);
    }
    PRINTF(dbgColor__dGreen, "---------------------\r");
    PRINTF(dbgColor__dGreen, "-- Start Test Loop --\r");
}


const int testCnt = 6;
uint16_t loopCnt = 1;
uint32_t lastCycle;

void loop() 
{
    char openlist[240] = {0};
    rslt = file_getOpenFiles(openlist, sizeof(openlist));
    PRINTF(dbgColor__green, "\rOpen Files (t=%d)\r%s\r", loopCnt % testCnt, openlist);

    if (loopCnt % testCnt == 1)
    {
        PRINTF(0, "Closing all files.\r");
        file_closeAll();
    }
    
    else if (loopCnt % testCnt == 2)
    {
        rslt = file_open("testfile1", fileOpenMode_rdWr, &fHandle1);
        if (rslt != resultCode__success)
            indicateFailure("Error opening testfile1", rslt);
    }

    else if (loopCnt % testCnt == 3)
    {
        char src[80] = { "0123456789abcdefghijklmnopqrstuvwxyz\r~" };
        rslt = file_open("testfile2", fileOpenMode_ovrRdWr, &fHandle2);
        rslt = file_write(fHandle2, src, 37, &writeRslt);
        PRINTF(dbgColor__cyan, "FileWrite: written=%d, filesz=%d\r", writeRslt.writtenSz, writeRslt.fileSz);
        rslt = file_close(fHandle2);
    }

    else if (loopCnt % testCnt == 4)
    {
        rslt = file_open("testfile2", fileOpenMode_rdWr, &fHandle2);
        rslt = file_seek(fHandle2, 10, fileSeekMode_fromBegin);
        rslt = file_read(fHandle2, 10);
        rslt = file_close(fHandle2);
    }

    else if (loopCnt % testCnt == 5)
    {
        rslt = file_open("testfile3", fileOpenMode_ovrRdWr, &fHandle3);
        ASSERT(fHandle3 != 0);
        if (rslt == resultCode__success)
        {
            PRINTF(dbgColor__cyan, "testfile3 hndl=%d\r", fHandle3);
            createFileImage(filedata3, 256);
            PRINTF(dbgColor__cyan, "wr=%d\r", file_write(fHandle3, filedata3, 256, &writeRslt));
            PRINTF(dbgColor__cyan, "sk=%d\r", file_seek(fHandle3, 0, fileSeekMode_fromBegin));
            PRINTF(dbgColor__cyan, "rd=%d\r", file_read(fHandle3, 200));
        }
    }

        // else if (loopCnt % testCnt == 0)
    // {
    //     uint32_t fPtr;

    //     rslt = file_open("testfile3", fileOpenMode_rdWr, &fHandle3);
    //     memset(readData, 0, sizeof(readData));
    //     rslt = file_seek(fHandle2, 11, fileSeekMode_fromBegin);
    //     rslt = file_getPosition(fHandle2, &fPtr);
    //     PRINTF(dbgColor__cyan, "FilePtr=%d\r", fPtr);

    //     rslt = file_read(fHandle2, 5);
    //     rslt = file_getPosition(fHandle2, &fPtr);
    //     PRINTF(dbgColor__cyan, "Post Read: FilePtr=%d\r", readData, fPtr);
    //     rslt = file_close(fHandle2);
    // }

    pDelay(1000);
    loopCnt++;
    PRINTF(dbgColor__magenta, "\rFreeMem=%u  Loop=%d\r", getFreeMemory(), loopCnt);
}


void fileReadReceiver(uint16_t fHandle, const char *fileData, uint16_t dataSz)
{
    char pBffr[81] = {0};
    while (dataSz > 0)
    {
        uint8_t lineSz = MIN(80, dataSz);
        memcpy(pBffr, fileData, lineSz);
        PRINTF(dbgColor__dGreen, "fH= %d>%s\r", fHandle, pBffr);
        fileData += lineSz;
        dataSz -= lineSz;
    }
}

void createFileImage(char *fileData, uint16_t dataSz)
{
    const uint8_t cStart = 32;              // file content is ASCII 32-122
    const uint8_t cStop = 122;
    char fChar = cStart;

    for (size_t i = 0; i < dataSz; i++)
    {
        *(fileData + i) = fChar;
        fChar++;
        if (fChar == cStop)
            fChar = cStart;
    }
}

/* test helpers
========================================================================================================================= */

void applEvntNotify(appEvents_t eventType, const char *notifyMsg)
{
    if (eventType == appEvent_fault_assertFailed)
    if (eventType == appEvent_fault_assertFailed)
    {
        PRINTF(dbgColor__error, "LTEmC Fault: %s\r", notifyMsg);
    }
    else 
    {
        PRINTF(dbgColor__white, "LTEmC Info: %s\r", notifyMsg);
    }
    return;
}


void indicateFailure(char failureMsg[], uint16_t status)
{
	PRINTF(DBGCOLOR_error, "\r** %s \r\n", failureMsg);
    PRINTF(DBGCOLOR_error, "** Test Assertion Failed. r=%d\r", status);
    platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);

    int halt = 1;
    while (halt) {}
}



/* Check free memory (stack-heap) 
 * - Remove if not needed for production
--------------------------------------------------------------------------------- */

#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__

int getFreeMemory() 
{
    char top;
    #ifdef __arm__
    return &top - reinterpret_cast<char*>(sbrk(0));
    #elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
    return &top - __brkval;
    #else  // __arm__
    return __brkval ? &top - __brkval : &top - __malloc_heap_start;
    #endif  // __arm__
}

