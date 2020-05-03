// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef _ATCMD_H_
#define _ATCMD_H_

#include <stddef.h>
#include <stdint.h>


#define ATCMD_DEFAULT_TIMEOUT_MILLIS 500
#define ATCMD_INVOKE_CMDSTR_SZ 46
#define ATCMD_DEFAULT_RESULT_BUF_SZ 80


#ifdef __cplusplus
extern "C" {
#endif


#define ATCMD_RESULT_PENDING    0
#define ATCMD_RESULT_SUCCESS    200
#define ATCMD_RESULT_BASEERROR  400
#define ATCMD_RESULT_TIMEOUT    408
#define ATCMD_RESULT_ERROR      500



// #define ATCMD_PARSER_RESULT_PENDING 0
// #define ATCMD_PARSER_RESULT_SUCCESS 200
// #define ATCMD_PARSER_RESULT_ERROR 500

// #define ATCMD_ERRORCODE_TIMEOUT 408
// #define ATCMD_ERRORCODE_GENERIC 500

// typedef enum 
// {
//     atcmd_resultCode_success = 200,
//     atcmd_resultCode_timeout = 408,
//     atcmd_resultCode_failed  = 500
// } atcmd_resultCode_t;

typedef uint16_t atcmd_result_t;

// typedef struct atcmd_result_tag
// {
//     bool complete;
//     bool successful;
//     uint16_t errorCode;
// } atcmd_result_t;


typedef struct atcmd_tag
{
    char cmdStr[ATCMD_INVOKE_CMDSTR_SZ];
    char *resultHead;
    char *resultTail;
    uint8_t resultSz;
    uint16_t resultCode;
    unsigned long invokedAt;
    uint16_t timeoutMillis;
    uint16_t (*cmdCompleteParser_func)(const char *response);
    iop_process_t irdPending;          // -1 not pending, otherwise proto pending on
} atcmd_t;


atcmd_t *atcmd_create(uint8_t resultSz);
atcmd_t *atcmd_build(const char* cmdStr, char* resultBuf, size_t resultBufSz, uint16_t timeoutMillis, atcmd_result_t (*cmdCompleteParser_func)(const char *response));
void atcmd_destroy(atcmd_t * atCmd);

void atcmd_invoke(const char *cmdStr);
void atcmd_invokeAdv(atcmd_t *atCmd);

void atcmd_reset(atcmd_t *atCmd);

atcmd_result_t atcmd_getResult(atcmd_t *atCmd);
atcmd_result_t atcmd_awaitResult(atcmd_t *atCmd);
void atcmd_cancel(atcmd_t *atCmd);

atcmd_result_t atcmd_gapCompletedHelper(const char *response, const char *landmark, u_int8_t gap, const char *terminator);
atcmd_result_t atcmd_tokenCompletedHelper(const char *response, const char *landmark, char token, uint8_t tokenCnt);

#ifdef __cplusplus
}
#endif

#endif  //!_ATCMD_H_
