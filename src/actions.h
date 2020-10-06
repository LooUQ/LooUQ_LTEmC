// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef _ACTIONS_H_
#define _ACTIONS_H_

//#include "ltem1c.h"
#include <stddef.h>
#include <stdint.h>


#define ACTION_RETRIES_DEFAULT 10
#define ACTION_RETRY_INTERVALmillis 100
#define ACTION_TIMEOUT_DEFAULTmillis 500

typedef struct action_tag
{
    char cmdStr[IOP_TX_BUFFER_SZ];
    bool isOpen;
    unsigned long invokedAt;
    // char *resultHead;
    // char *resultTail;
    // uint8_t resultSz;
    uint16_t resultCode;                    // 0 is pending, otherwise ACTION_RESULT_* codes
    char *response;
    uint16_t timeoutMillis;
    uint16_t (*taskCompleteParser_func)(const char *response, char **endptr);
} action_t;


typedef struct actionResult_tag
{
    resultCode_t statusCode;
    char *response;
} actionResult_t;


#ifdef __cplusplus
extern "C" {
#endif

bool action_tryInvoke(const char *cmdStr);
bool action_tryInvokeAdv(const char *cmdStr, uint8_t retries, uint16_t timeout, uint16_t (*customCmdCompleteParser_func)(const char *response, char **endptr));
void action_sendRaw(const char *data, uint16_t dataSz, uint16_t timeoutMillis, uint16_t (*customCmdCompleteParser_func)(const char *response, char **endptr));
void action_sendRawWithEOTs(const char *data, uint16_t dataSz, const char* eotPhrase, uint16_t timeoutMillis, uint16_t (*customCmdCompleteParser_func)(const char *response, char **endptr));

actionResult_t action_awaitResult(bool closeAction);
actionResult_t action_getResult(bool closeAction);
bool actn_acquireLock(const char *cmdStr, uint8_t retries);
void action_close();

resultCode_t action_okResultParser(const char *response, char** endptr);
resultCode_t action_defaultResultParser(const char *response, const char *landmark, bool landmarkReqd, uint8_t gap, const char *terminator, char** endptr);
resultCode_t action_tokenResultParser(const char *response, const char *landmark, char token, uint8_t reqdTokens, const char *terminator, char** endptr);
resultCode_t action_serviceResponseParser(const char *response, const char *landmark, uint8_t resultIndx, char** endptr);


#ifdef __cplusplus
}
#endif

#endif  //!_ACTIONS_H_
