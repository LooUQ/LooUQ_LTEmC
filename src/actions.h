// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef _ACTIONS_H_
#define _ACTIONS_H_

//#include "ltem1c.h"
#include <stddef.h>
#include <stdint.h>


#define ACTION_TIMEOUTml             500        ///< Default number of millis to wait for action to complete, can be overridden in action_tryInvokeAdv()
#define RESULT_CODE_PENDING       0xFFFF        ///< Value returned from response parsers indicating a pattern match has not yet been detected

// structure sizing
#define ACTION_HISTRESPBUF_SZ        240        ///< Size of response captured by action history (action error diagnostics)


/** 
 *  \brief Record of last action, NOTE: only set on action NON-SUCCESS.
*/
typedef struct actionHistory_tag
{
    char cmdStr[IOP_TX_BUFFER_SZ];              ///< AT command string to be passed to the BGx module.
    char response[ACTION_HISTRESPBUF_SZ];       ///< The char c-string containing the full response from the BGx.
    uint32_t duration;                          ///< Duration from AT invoke to action complete (or timeout)
    resultCode_t statusCode;                    ///< The HTML style status code, indicates the sucess or failure (type) for the command's invocation.
} actionHistory_t;


/** 
 *  \brief Structure to control invocation and management of an AT command with the BGx module.
*/
typedef struct action_tag
{
    char cmdStr[IOP_TX_BUFFER_SZ];      ///< AT command string to be passed to the BGx module.
    bool isOpen;                        ///< True if the command is still open, AT commands are single threaded and this blocks a new cmd initiation.
    uint32_t invokedAt;                 ///< Tick value at the command invocation, used for timeout detection.
    uint16_t resultCode;                ///< HTML type response code, 0 is special "pending" status, see ACTION_RESULT_* codes.
    char *response;                     ///< The response to the command received from the BGx.
    uint16_t timeoutMillis;             ///< Timout in milliseconds for the command, defaults to 300mS. BGx documentation indicates cmds with longer timeout.
    actionHistory_t *lastActionError;   ///< Struct containing information on last action response\result. NOTE: only set on NON-SUCCESS.
    uint16_t (*taskCompleteParser_func)(const char *response, char **endptr);  ///< Function to parse the response looking for completion.
} action_t;


/** 
 *  \brief Result structure returned from a action request (await or get).
*/
typedef struct actionResult_tag
{
    resultCode_t statusCode;            ///< The HTML style status code, indicates the sucess or failure (type) for the command's invocation.
    char *response;                     ///< The char c-string containing the full response from the BGx.
    uint16_t responseCode;              ///< Numeric response value from many "status" action parsers (suffixed with _rc)
} actionResult_t;



#ifdef __cplusplus
extern "C" {
#endif

bool action_tryInvoke(const char *cmdStr);
bool action_tryInvokeAdv(const char *cmdStr, uint16_t timeout, uint16_t (*customCmdCompleteParser_func)(const char *response, char **endptr));
void action_sendRaw(const char *data, uint16_t dataSz, uint16_t timeoutMillis, uint16_t (*customCmdCompleteParser_func)(const char *response, char **endptr));
void action_sendRawWithEOTs(const char *data, uint16_t dataSz, const char* eotPhrase, uint16_t timeoutMillis, uint16_t (*customCmdCompleteParser_func)(const char *response, char **endptr));

actionResult_t action_awaitResult(bool closeAction);
actionResult_t action_getResult(bool closeAction);
bool actn_acquireLock(const char *cmdStr, uint8_t retries);
void action_close();

void action_exitTextMode();
void action_exitDataMode();
//void action_exitStreamMode(uint8_t fillValue);

resultCode_t action_okResultParser(const char *response, char** endptr);
resultCode_t action_defaultResultParser(const char *response, const char *landmark, bool landmarkReqd, uint8_t gap, const char *terminator, char** endptr);
resultCode_t action_tokenResultParser(const char *response, const char *landmark, char token, uint8_t reqdTokens, const char *terminator, char** endptr);
resultCode_t action_serviceResponseParser(const char *response, const char *landmark, uint8_t resultIndx, char** endptr);
char *action_strToken(char *source, int delimiter, char *token, uint8_t tokenMax);

#ifdef __cplusplus
}
#endif

#endif  //!_ACTIONS_H_
