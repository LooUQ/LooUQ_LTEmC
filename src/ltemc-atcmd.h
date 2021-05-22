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

#include <stddef.h>
#include <stdint.h>


#define ACTION_TIMEOUTml             500        ///< Default number of millis to wait for action to complete, can be overridden in action_tryInvokeAdv()
#define RESULT_CODE_PENDING       0xFFFF        ///< Value returned from response parsers indicating a pattern match has not yet been detected

// structure sizing
#define ACTION_HISTRESPBUF_SZ        240        ///< Size of response captured by action history (action error diagnostics)


/** 
 *  \brief Record of last action, NOTE: only set on action NON-SUCCESS.
*/
typedef struct atcmdHistory_tag
{
    char cmdStr[IOP_TX_BUFFER_SZ];              ///< AT command string to be passed to the BGx module.
    char response[ACTION_HISTRESPBUF_SZ];       ///< The char c-string containing the full response from the BGx.
    uint32_t duration;                          ///< Duration from AT invoke to action complete (or timeout)
    resultCode_t statusCode;                    ///< The HTML style status code, indicates the sucess or failure (type) for the command's invocation.
} atcmdHistory_t;


/** 
 *  \brief Structure to control invocation and management of an AT command with the BGx module.
*/
typedef struct atcmd_tag
{
    char cmdStr[IOP_TX_BUFFER_SZ];      ///< AT command string to be passed to the BGx module.
    bool isOpen;                        ///< True if the command is still open, AT commands are single threaded and this blocks a new cmd initiation.
    uint32_t invokedAt;                 ///< Tick value at the command invocation, used for timeout detection.
    uint16_t resultCode;                ///< HTML type response code, 0 is special "pending" status, see ACTION_RESULT_* codes.
    char *response;                     ///< The response to the command received from the BGx.
    uint16_t timeoutMillis;             ///< Timout in milliseconds for the command, defaults to 300mS. BGx documentation indicates cmds with longer timeout.
    atcmdHistory_t *lastActionError;   ///< Struct containing information on last action response\result. NOTE: only set on NON-SUCCESS.
    uint16_t (*taskCompleteParser_func)(const char *response, char **endptr);  ///< Function to parse the response looking for completion.
} atcmd_t;


/** 
 *  \brief Result structure returned from a action request (await or get).
*/
typedef struct atcmdResult_tag
{
    resultCode_t statusCode;            ///< The HTML style status code, indicates the sucess or failure (type) for the command's invocation.
    char *response;                     ///< The char c-string containing the full response from the BGx.
    uint16_t responseCode;              ///< Numeric response value from many "status" action parsers (suffixed with _rc)
} atcmdResult_t;



#ifdef __cplusplus
extern "C" {
#endif

bool atcmd_tryInvoke(const char *cmdStr);
bool atcmd_tryInvokeAdv(const char *cmdStr, uint16_t timeout, uint16_t (*customCmdCompleteParser_func)(const char *response, char **endptr));
void atcmd_sendRaw(const char *data, uint16_t dataSz, uint16_t timeoutMillis, uint16_t (*customCmdCompleteParser_func)(const char *response, char **endptr));
void atcmd_sendRawWithEOTs(const char *data, uint16_t dataSz, const char* eotPhrase, uint16_t timeoutMillis, uint16_t (*customCmdCompleteParser_func)(const char *response, char **endptr));

atcmdResult_t atcmd_awaitResult(bool closeAction);
atcmdResult_t atcmd_getResult(bool closeAction);
bool actn_acquireLock(const char *cmdStr, uint8_t retries);
void atcmd_close();

void atcmd_exitTextMode();
void atcmd_exitDataMode();
//void atcmd_exitStreamMode(uint8_t fillValue);

resultCode_t atcmd_okResultParser(const char *response, char** endptr);
resultCode_t atcmd_defaultResultParser(const char *response, const char *landmark, bool landmarkReqd, uint8_t gap, const char *terminator, char** endptr);
resultCode_t atcmd_tokenResultParser(const char *response, const char *landmark, char token, uint8_t reqdTokens, const char *terminator, char** endptr);
resultCode_t atcmd_serviceResponseParser(const char *response, const char *landmark, uint8_t resultIndx, char** endptr);
char *atcmd_strToken(char *source, int delimiter, char *token, uint8_t tokenMax);


bool atcmd__acquireLock(const char *cmdStr, uint8_t retries);

#ifdef __cplusplus
}
#endif

#endif  // !__LTEMC_ATCMD_H__
