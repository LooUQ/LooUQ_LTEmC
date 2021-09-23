/******************************************************************************
 *  \file ltemc-atcmd.h
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020,2021 LooUQ Incorporated.
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
 *****************************************************************************/

#ifndef __LTEMC_ATCMD_H__
#define __LTEMC_ATCMD_H__

#include "ltemc-iop.h"

enum
{
    atcmd__defaultTimeoutMS = 800,
    atcmd__useDefaultOKCompletionParser = 0,

    atcmd__setLockModeManual = 0,
    atcmd__setLockModeAuto = 1,

    atcmd__parserPendingResult = 0xFFFF      ///< value returned from response parsers indicating a pattern match has not yet been detected
};


// /** 
//  *  \brief Record of last action, NOTE: only set on action NON-SUCCESS.
// */
// typedef struct atcmdHistory_tag
// {
//     char cmdStr[atcmd__IcommandBufferSz];       ///< AT command string to be passed to the BGx module.
//     char response[atcmd__IcommandBufferSz];     ///< The char c-string containing the full response from the BGx.
//     uint32_t duration;                          ///< Duration from AT invoke to action complete (or timeout)
//     resultCode_t statusCode;                    ///< The HTML style status code, indicates the sucess or failure (type) for the command's invocation.
// } atcmdHistory_t;


/** 
 *  \brief Structure to control invocation and management of an AT command with the BGx module.
*/
typedef struct atcmd_tag
{
    char cmdStr[IOP__rxCoreBufferSize];         ///< AT command string to be passed to the BGx module.
    uint32_t timeoutMS;                         ///< Timout in milliseconds for the command, defaults to 300mS. BGx documentation indicates cmds with longer timeout.
    bool isOpenLocked;                          ///< True if the command is still open, AT commands are single threaded and this blocks a new cmd initiation.
    bool autoLock;                              ///< last invoke was auto and should be closed automatically on complete
    uint32_t invokedAt;                         ///< Tick value at the command invocation, used for timeout detection.
    char *response;                             ///< The response to the command received from the BGx. Points into IOP core buffer
    char *responseTail;                         ///< Pointer to the unparsed section of the response, beyond complete parser's match
    uint32_t execDuration;                      ///< duration of command's execution in milliseconds
    resultCode_t resultCode;

    uint16_t (*completeParser_func)(const char *response, char **endptr);  ///< Function to parse the response looking for completion.
} atcmd_t;


/** 
 *  \brief Result structure returned from a action request (await or get).
*/
typedef struct atcmdResult_tag
{
    resultCode_t statusCode;                    ///< The HTML style status code, indicates the sucess or failure (type) for the command's invocation.
    char *response;                             ///< The char c-string containing the full response from the BGx.
    uint16_t responseCode;                      ///< Numeric response value from many "status" action parsers (suffixed with _rc)
} atcmdResult_t;


#ifdef __cplusplus
extern "C" {
#endif

void atcmd_setOptions(uint32_t timeoutMS, uint16_t (*customCmdCompleteParser_func)(const char *response, char **endptr));
void atcmd_restoreOptionDefaults();

// automatically lock 
bool atcmd_tryInvoke(const char *cmdStr, ...);
bool atcmd_tryInvokeWithOptions(const char *cmdStr, ...);

// set a manual lock for a section of actions 
bool atcmd_awaitLock(uint16_t waitMillis);
void atcmd_invokeReuseLock(const char *cmdStr, ...);
void atcmd_sendCmdData(const char *data, uint16_t dataSz, const char* eotPhrase);

resultCode_t atcmd_awaitResult();
void atcmd_getResult();                                 
bool atcmd_isLockActive();
void atcmd_reset(bool releaseLock);
void atcmd_close();

resultCode_t atcmd_getLastResult();
uint32_t atcmd_getLastDuration();
char *atcmd_getLastResponse();

void atcmd_exitTextMode();
void atcmd_exitDataMode();


resultCode_t atcmd_okResultParser(const char *response, char** endptr);
resultCode_t atcmd_defaultResultParser(const char *response, const char *landmark, bool landmarkReqd, uint8_t gap, const char *terminator, char** endptr);
resultCode_t atcmd_tokenResultParser(const char *response, const char *landmark, char separator, uint8_t reqdTokens, const char *terminator, char** endptr);
resultCode_t atcmd_serviceResponseParser(const char *response, const char *landmark, uint8_t resultIndx, char** endptr);
resultCode_t atcmd_serviceResponseParserTerm(const char *response, const char *landmark, uint8_t resultIndx, const char *terminator, char** endptr);

resultCode_t atcmd_readyPromptParser(const char *response, const char *rdyPrompt, char **endptr);
resultCode_t atcmd_txDataPromptParser(const char *response, char **endptr);
resultCode_t atcmd_connectPromptParser(const char *response, char **endptr);

char *atcmd_strToken(char *source, int delimiter, char *token, uint8_t tokenMax);
void ATCMD_getResult();

#ifdef __cplusplus
}
#endif

#endif  // !__LTEMC_ATCMD_H__
