// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef _ACTIONS_H_
#define _ACTIONS_H_

//#include "ltem1c.h"
#include <stddef.h>
#include <stdint.h>


#define ACTION_CMDSTR_BRIEFSZ 64
#define ACTION_DEFAULT_RESPONSE_SZ 64
#define ACTION_DEFAULT_TIMEOUT_MILLIS 600


typedef struct action_tag
{
    char cmdBrief[ACTION_CMDSTR_BRIEFSZ + 1];
    bool autoClose;
    char *resultHead;
    char *resultTail;
    uint8_t resultSz;
    uint16_t resultCode;                    // 0 is pending, otherwise ACTION_RESULT_* codes
    unsigned long invokedAt;
    uint16_t timeoutMillis;
    uint16_t (*cmdCompleteParser_func)(const char *response);
} action_t;


#ifdef __cplusplus
extern "C" {
#endif


// action_t *action_build(const char* cmdStr, uint16_t resultBufSz, uint16_t timeoutMillis, actionResult_t (*cmdCompleteParser_func)(const char *response));
// void action_destroy(action_t * atCmd);

//bool action_clearToInvoke();
//void action_invoke(const char *cmdStr);
bool action_tryInvoke(const char *cmdStr, bool retry);
// bool action_tryInvokeCustom(action_t *cmdAction, bool retry);
void action_sendData(const char *data, uint16_t dataSz);

void action_reset();
void action_setAutoClose(bool autoClose);

actionResult_t action_getResult(char *response, uint16_t responseSz, uint16_t timeout, uint16_t (*customCmdCompleteParser_func)(const char *response));
actionResult_t action_awaitResult(char *response, uint16_t responseSz, uint16_t timeout, uint16_t (*customCmdCompleteParser_func)(const char *response));
void action_cancel();

actionResult_t action_okResultParser(const char *response);
actionResult_t action_gapResultParser(const char *response, const char *landmark, bool landmarkReqd, uint8_t gap, const char *terminator);
actionResult_t action_tokenResultParser(const char *response, const char *landmark, char token, uint8_t tokenCnt);
actionResult_t action_serviceResponseParser(const char *response, const char *landmark, uint8_t resultIndx);
actionResult_t action_cmeResultParser(const char *response);


#ifdef __cplusplus
}
#endif

#endif  //!_ACTIONS_H_
