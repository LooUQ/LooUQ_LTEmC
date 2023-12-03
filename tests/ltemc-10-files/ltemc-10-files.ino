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

#define ENABLE_DIAGPRINT                    // expand DPRINT into debug output
//#define ENABLE_DIAGPRINT_VERBOSE            // expand DPRINT and DPRINT_V into debug output
#define ENABLE_ASSERT
#define ENABLE_JLINKRTT
#include <lqdiag.h>

/* specify the pin configuration 
 * --------------------------------------------------------------------------------------------- */
#ifdef ARDUINO_ARCH_ESP32
    #define HOST_ESP32_DEVMOD_BMS
#else
    #define HOST_FEATHER_UXPLOR_L
    // #define HOST_FEATHER_UXPLOR             
    // #define HOST_FEATHER_LTEM3F
#endif

#include <lq-SAMDutil.h>                // allows read of reset cause

#include <ltemc.h>
#include <ltemc-files.h>

// temporary for testing http_createRequest
#include <ltemc-http.h>

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
    #ifdef DIAGPRINT_SERIAL
        Serial.begin(115200);
        #if (SERIAL_OPT > 0)
        while (!Serial) {}      // force wait for serial ready
        #else
        delay(5000);            // just give it some time
        #endif
    #endif

    DPRINT(PRNT_RED, "\rLTEmC test-10-filesystem\r\n");

    ltem_create(ltem_pinConfig, NULL, applEvntNotify);
    ltem_start(resetAction_swReset);                                            // start LTEm, if found on reset it
    file_setAppReceiver(fileReadReceiver);

    rslt = file_getFSInfo(&fsInfo);
    DPRINT(PRNT_GREEN, "FileSystem: avail=%d, free=%d, fileCnt=%d, taken=%d\r\n", fsInfo.totalSz, fsInfo.freeSz, fsInfo.filesCnt, fsInfo.filesSz);

    rslt = file_getFilelist(&fileList, "");
    DPRINT(PRNT_GREEN, "\r\rFiles\r\n");
    for (size_t i = 0; i < fileList.fileCnt; i++)
    {
        DPRINT(PRNT_WHITE, "%s sz=%d\r\n", fileList.files[i].filename, fileList.files[i].fileSz);
    }
    DPRINT(PRNT_dGREEN, "---------------------\r\n");
    DPRINT(PRNT_dGREEN, "-- Start Test Loop --\r\n");

    // #define OPM_PROJECT_KEY "LQCloud-OrgKey: 5Zul5eB3Qn1gkEtDFCEs9dn0IXexhr3x"
    // #define OPM_HOST_URL "https://devices-dev-pelogical.azurewebsites.net"

    // httpCtrl_t httpCtrl_opMRpt;
    // // char httpCstmHdrs[512];
    // char relativeUrl[128] = {0};

    // http_initControl(&httpCtrl_opMRpt, dataCntxt_1, httpRecvCB);                                    // setup HTTP for OTA functions
    // http_setConnection(&httpCtrl_opMRpt, OPM_HOST_URL, 443);                                        // set host target to post opMetrics reporting host 
    // DPRINT_V(PRNT_dMAGENTA, "OpMetrics Reporting Host=%s\r\n", httpCtrl_opMRpt.hostUrl);
    // snprintf(relativeUrl, sizeof(relativeUrl), "/opmetrics/opmrpt/%s", ltem_getModemInfo()->imei);  // create relative URL

    // rslt = http_postFile(&httpCtrl_opMRpt, relativeUrl, false, "bmsOpMRpt");
    while(1){}
}


const int testCnt = 6;
uint16_t loopCnt = 1;
uint32_t lastCycle;
uint16_t bytesRead;

void loop() 
{
    char openlist[240] = {0};
    fileListResult_t fileList;
    rslt = file_getOpenFiles(openlist, sizeof(openlist));
    DPRINT(PRNT_GREEN, "\r\nStart Loop\r\nOpen Files (starting test pattern=%d)\r%s\r\n", loopCnt % testCnt, openlist);

    if (loopCnt % testCnt == 1)
    {
        DPRINT(0, "Creating testfile1\r\n");
        rslt = file_open("testfile1", fileOpenMode_ovrRdWr, &fHandle1);
        if (rslt != resultCode__success)
            indicateFailure("Error opening testfile1", rslt);
        file_close(fHandle1);

        DPRINT(0, "Checking for non-existent file: missingFile\r\n");
        rslt = file_getFilelist(&fileList, "missingFile");
        DPRINT(0, "Missing file result=%d\r\n", rslt);

        DPRINT(0, "Checking for present file: testfile1\r\n");
        rslt = file_getFilelist(&fileList, "testfile1");
        DPRINT(0, "Present file result=%d\r\n", rslt);
    }
    
    if (loopCnt % testCnt == 2)
    {
        DPRINT(0, "Opening testfile1\r\n");
        rslt = file_open("testfile1", fileOpenMode_rdWr, &fHandle1);
        if (rslt != resultCode__success)
            indicateFailure("Error opening testfile1", rslt);
    }

    else if (loopCnt % testCnt == 3)
    {
        DPRINT(0, "Opening/writing/closing testfile2\r\n");
        char src[80];
        rslt = file_open("testfile2", fileOpenMode_ovrRdWr, &fHandle2);
        DPRINT(PRNT_CYAN, "Test #3 OpenRslt=%d\r\n", rslt);
        // pDelay(5);
        strcpy(src, "0123456789");
        rslt = file_write(fHandle2, src, strlen(src), &writeRslt);
        DPRINT(PRNT_CYAN, "FileWrite #1 (rslt=%d): written=%d, filesz=%d\r\n", rslt, writeRslt.writtenSz, writeRslt.fileSz);
        // pDelay(100);
        strcpy(src, "abcdefghijklmnopqrstuvwxyz");
        rslt = file_write(fHandle2, src, strlen(src), &writeRslt);
        DPRINT(PRNT_CYAN, "FileWrite #2 (rslt=%d): written=%d, filesz=%d\r\n", rslt, writeRslt.writtenSz, writeRslt.fileSz);
        rslt = file_close(fHandle2);
    }

    else if (loopCnt % testCnt == 4)
    {
        DPRINT(0, "Opening/seeking/reading testfile2\r\n");
        rslt = file_open("testfile2", fileOpenMode_rdWr, &fHandle2);
        rslt = file_seek(fHandle2, 10, fileSeekMode_fromBegin);
        rslt = file_read(fHandle2, 10, &bytesRead);
        rslt = file_close(fHandle2);
    }

    else if (loopCnt % testCnt == 5)
    {
        DPRINT(0, "Opening (create/overwrite)/seeking/writing/reading testfile3\r\n");

        rslt = file_open("testfile3", fileOpenMode_ovrRdWr, &fHandle3);
        ASSERT(fHandle3 != 0);
        if (rslt == resultCode__success)
        {
            DPRINT(PRNT_CYAN, "testfile3 hndl=%d\r\n", fHandle3);
            createFileImage(filedata3, 256);
            DPRINT(PRNT_CYAN, "wr=%d\r\n", file_write(fHandle3, filedata3, 256, &writeRslt));
            DPRINT(PRNT_CYAN, "sk=%d\r\n", file_seek(fHandle3, 0, fileSeekMode_fromBegin));
            DPRINT(PRNT_CYAN, "rd=%d\r\n", file_read(fHandle3, 200, &bytesRead));
        }
    }

    else if (loopCnt % testCnt == 0)
    {
        DPRINT(0, "Closing all files\r\n");
        file_closeAll();
    }

    pDelay(1000);
    DPRINT(PRNT_MAGENTA, "\r\nEnd Test Loop=%d\r\n", loopCnt);
    loopCnt++;
}


void fileReadReceiver(uint16_t fHandle, const char *fileData, uint16_t dataSz)
{
    char pBffr[81] = {0};
    while (dataSz > 0)
    {
        uint8_t lineSz = MIN(80, dataSz);
        memcpy(pBffr, fileData, lineSz);
        DPRINT(PRNT_dGREEN, "fileRcvr: hndl=%d received>%s\r\n", fHandle, pBffr);
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

void applEvntNotify(appEvent_t eventType, const char *notifyMsg)
{
    if (eventType == appEvent_fault_assertFailed)
    if (eventType == appEvent_fault_assertFailed)
    {
        DPRINT(PRNT_ERROR, "LTEmC Fault: %s\r\n", notifyMsg);
    }
    else 
    {
        DPRINT(PRNT_WHITE, "LTEmC Info: %s\r\n", notifyMsg);
    }
    return;
}


void indicateFailure(char failureMsg[], uint16_t status)
{
	DPRINT(PRNT_ERROR, "\r** %s \r\n", failureMsg);
    DPRINT(PRNT_ERROR, "** Test Assertion Failed. r=%d\r\n", status);
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


void httpRecvCB(dataCntxt_t dataCntxt, char *recvData, uint16_t dataSz, bool isFinal)
{
    DPRINT(PRNT_MAGENTA, "AppRecv'd %d new chars\r\n", dataSz);
     if (isFinal)
     {
         DPRINT(PRNT_MAGENTA, "Read Complete!\r");
     }
}
