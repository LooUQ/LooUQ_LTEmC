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

/** 
 *  \brief Structure to control invocation and management of an AT command with the BGx module.
*/
typedef struct action_tag
{
    char cmdStr[IOP_TX_BUFFER_SZ];      ///< AT command string to be passed to the BGx module.
    bool isOpen;                        ///< True if the command is still open, AT commands are single threaded and this blocks a new cmd initiation.
    unsigned long invokedAt;            ///< Tick value at the command invocation, used for timeout detection.
    uint16_t resultCode;                ///< HTML type response code, 0 is special "pending" status, see ACTION_RESULT_* codes.
    char *response;                     ///< The response to the command received from the BGx.
    uint16_t timeoutMillis;             ///< Timout in milliseconds for the command, defaults to 300mS. BGx documentation indicates cmds with longer timeout.
    uint16_t (*taskCompleteParser_func)(const char *response, char **endptr);   ///< Function to parse the response looking for completion.
} action_t;


/** 
 *  \brief Result structure returned from a action request (await or get).
*/
typedef struct actionResult_tag
{
    resultCode_t statusCode;            ///< The HTML style status code, indicates the sucess or failure (type) for the command's invocation.
    char *response;                     ///< The char c-string containing the full response from the BGx.
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
