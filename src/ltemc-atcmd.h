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

/** 
 *  @brief Typed constants for AT-CMD module.
*/
enum atcmd__constants
{
    atcmd__defaultTimeoutMS = 800,
    atcmd__useDefaultOKCompletionParser = 0,

    atcmd__setLockModeManual = 0,
    atcmd__setLockModeAuto = 1,

    atcmd__parserPendingResult = 0xFFFF      ///< value returned from response parsers indicating a pattern match has not yet been detected
};


typedef uint16_t (*cmdResponseParser_func)(const char *response, uint16_t *completeSz, uint32_t *responseValue);

// typedef struct atcmd_tag
// {
//     char cmdStr[IOP__rxCoreBufferSize];         ///< AT command string to be passed to the BGx module.
//     uint32_t timeoutMS;                         ///< Timout in milliseconds for the command, defaults to 300mS. BGx documentation indicates cmds with longer timeout.
//     bool isOpenLocked;                          ///< True if the command is still open, AT commands are single threaded and this blocks a new cmd initiation.
//     bool autoLock;                              ///< last invoke was auto and should be closed automatically on complete
//     uint32_t invokedAt;                         ///< Tick value at the command invocation, used for timeout detection.
//     char *response;                             ///< The response to the command received from the BGx. Points into IOP core buffer
//     char *responseTail;                         ///< Pointer to the unparsed section of the response, beyond complete parser's match
//     uint32_t execDuration;                      ///< duration of command's execution in milliseconds
//     //uint32_t responseValue;                     /// storage for optional value obtained by response parser
//     resultCode_t resultCode;

//     uint16_t (*completeParser_func)(const char *response, char **endptr);  ///< Function to parse the response looking for completion.
//     //responseParser_func *responseParser;
// } atcmd_t;


/** 
 *  @brief Structure to control invocation and management of an AT command with the BGx module.
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
 *  @brief Result structure returned from a action request (await or get).
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

/**
 *	@brief Sets options for BGx AT command control (atcmd). 
 *  @param timeoutMS [in] Number of milliseconds the action can take. Use system default ACTION_TIMEOUTms or your value.
 *  @param customCmdCompleteParser_func [in] Custom command response parser to signal result is complete. NULL for std parser.
 */
void atcmd_setOptions(uint32_t timeoutMS, uint16_t (*customCmdCompleteParser_func)(const char *response, char **endptr));


/**
 *	@brief Reset AT command options to defaults.
 *  
 *  @details Use after setOptions() to reset AT-Cmd behaviors to default settings.
 */
void atcmd_restoreOptionDefaults();


/**
 *	@brief Invokes a BGx AT command using default option values (automatic locking).
 *	@param cmdStrTemplate [in] The command string to send to the BG96 module.
 *  @param ... [in] Variadic parameter list to integrate into the cmdStrTemplate.
 *  @return True if action was invoked, false if not
 */
bool atcmd_tryInvoke(const char *cmdTemplate, ...);

/**
 *	@brief Invokes a BGx AT command using set option values, previously set with setOptions() (automatic locking).
 *	@param cmdStrTemplate [in] The command string to send to the BG96 module.
 *  @param ... [in] Variadic parameter list to integrate into the cmdStrTemplate.
 *  @return True if action was invoked, false if not
 */
bool atcmd_tryInvokeWithOptions(const char *cmdTemplate, ...);


/**
 *	@brief Invokes a BGx AT command without acquiring a lock, using previously set setOptions() values.
 *	@param cmdStrTemplate [in] The command string to send to the BG96 module.
 *  @param ... [in] Variadic parameter list to integrate into the cmdStrTemplate.
 */
void atcmd_invokeReuseLock(const char *cmdTemplate, ...);


/**
 *	@brief Closes (completes) a BGx AT command structure and frees action resource (release action lock).
 */
void atcmd_close();


/**
 *	@brief Performs blind send data transfer to device.
 *  @details Operates in without regard to atcmd lock. Doesn't set/reset lock, doesn't check for lock.
 *           Typically used to send subcommand text
 *  @param data [in] - Pointer to the block of character data to send
 *  @param dataSz [in] - The size of the data block
 *  @param eotPhrase [in] - Char string that terminates the sent characters
 */
void atcmd_sendCmdData(const char *data, uint16_t dataSz, const char* eotPhrase);


/**
 *	@brief Checks recv buffer for command response and sets atcmd structure data with result.
 */
void atcmd_getResult();


/**
 *	@brief Waits for atcmd result, periodically checking recv buffer for valid response until timeout.
 */
resultCode_t atcmd_awaitResult();


/**
 *	@brief Returns the atCmd result code, 0xFFFF or atcmd__parserPendingResult if command is pending completion
 */
resultCode_t atcmd_getLastResult();


/**
 *	@brief Returns the atCmd last execution duration in milliseconds
 */
uint32_t atcmd_getLastDuration();


/**
 *	@brief Returns the atCmd response text if completed, will be empty C-string prior to completion
 */
char *ATCMD_getLastResponse();


/**
 *	@brief Returns the atCmd response text beyond internal completion parser's progress
 */
char *ATCMD_getLastResponseTail();

/**
 *	@brief Sends ESC character to ensure BGx is not in text mode (">" prompt awaiting ^Z/ESC, MQTT publish etc.).
 */
void atcmd_exitTextMode();


/**
 *	@brief Sends +++ sequence to transition BGx out of data mode to command mode.
 */
void atcmd_exitDataMode();



#pragma region LTEmC Internal Functions
/*-----------------------------------------------------------------------------------------------*/

/**
 *  @brief Awaits exclusive access to QBG module command interface.
 *  @param timeoutMS [in] - Number of milliseconds to wait for a lock.
 *  @return true if lock aquired prior to the timeout period.
*/
bool ATCMD_awaitLock(uint16_t timeoutMS);


/**
 *	@brief Returns the current atCmd lock state
 *  @return True if atcmd lock is active (command underway)
 */
bool ATCMD_isLockActive();


/**
 *	@brief Resets atCmd struct and optionally releases lock, a BGx AT command structure.
 */
void ATCMD_reset(bool releaseLock);

#pragma endregion
/*-----------------------------------------------------------------------------------------------*/



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
