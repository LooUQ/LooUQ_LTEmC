// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef _ACTIONS_H_
#define _ACTIONS_H_

#include <stddef.h>
#include <stdint.h>


#define ACTION_DEFAULT_TIMEOUT_MILLIS 500
#define ACTION_INVOKE_CMDSTR_SZ 46
#define ACTION_DEFAULT_RESULT_BUF_SZ 80


#ifdef __cplusplus
extern "C" {
#endif

#define ACTIONS

void action_adf();

void actions_cmeErrorResultParser();

void actns_dafd();

#define ACTION_RESULT_PENDING        0
#define ACTION_RESULT_SUCCESS      200
#define ACTION_RESULT_BASEERROR    400
#define ACTION_RESULT_TIMEOUT      408
#define ACTION_RESULT_ERROR        500


// action_result_t should be populated with ACTION_RESULT_x constant values or an errorCode (uint >= 400)
typedef uint16_t action_result_t;


typedef struct action_tag
{
    char cmdStr[ACTION_INVOKE_CMDSTR_SZ];
    char *resultHead;
    char *resultTail;
    uint8_t resultSz;
    uint16_t resultCode;
    unsigned long invokedAt;
    uint16_t timeoutMillis;
    uint16_t (*cmdCompleteParser_func)(const char *response);
    iop_process_t irdPending;          // -1 not pending, otherwise proto pending on
} action_t;


action_t *action_create(uint8_t resultSz);
action_t *action_build(const char* cmdStr, uint16_t resultBufSz, uint16_t timeoutMillis, action_result_t (*cmdCompleteParser_func)(const char *response));
void action_destroy(action_t * atCmd);

void action_invokeCmd(const char *cmdStr);
void action_invokeCmdWithParser(const char *cmdStr, uint16_t (*cmdCompleteParser_func)(const char *response));
void action_invokeCustomCmd(action_t *cmdAction);
void action_sendData(const char *data, uint16_t dataSz);

void action_reset(action_t *atCmd);

action_result_t action_getResult(action_t *atCmd, bool autoClose);
action_result_t action_awaitResult(action_t *atCmd);
void action_cancel(action_t *atCmd);

action_result_t action_okResultParser(const char *response);
action_result_t action_gapResultParser(const char *response, const char *landmark, bool landmarkReqd, uint8_t gap, const char *terminator);
action_result_t action_tokenResultParser(const char *response, const char *landmark, char token, uint8_t tokenCnt);
action_result_t action_cmeResultParser(const char *response);


#ifdef __cplusplus
}
#endif

#endif  //!_ACTIONS_H_
