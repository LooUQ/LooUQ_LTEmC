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

#include "ltemc-internal.h"

/** 
 *  \brief Typed constants for AT-CMD module.
*/
enum atcmd__constants
{
    atcmd__defaultTimeoutMS = 800,
    atcmd__useDefaultOKCompletionParser = 0,

    atcmd__setLockModeManual = 0,
    atcmd__setLockModeAuto = 1,
};


// typedef enum cmdParseRslt_tag
// {
//     cmdParseRslt_pending = 0x00,

//     cmdParseRslt_preambleMissing = 0x01,
//     cmdParseRslt_countShort = 0x02,
//     cmdParseRslt_moduleError = 0x04,
//     cmdParseRslt_excessRecv = 0x20,

//     cmdParseRslt_success = 0x40,
//     cmdParseRslt_error = 0x80,
// } cmdParseRslt_t;


// typedef cmdParseRslt_t (*cmdResponseParser_func)(ltemDevice_t *modem);


/** 
 *  \brief Structure to control invocation and management of an AT command with the BGx module.
*/
typedef struct atcmd_tag
{
    char cmdStr[bufferSz__cmdTx];               /// AT command string to be passed to the BGx module.
    uint32_t timeoutMS;                         /// Timout in milliseconds for the command, defaults to 300mS. BGx documentation indicates cmds with longer timeout.
    bool isOpenLocked;                          /// True if the command is still open, AT commands are single threaded and this blocks a new cmd initiation.
    bool autoLock;                              /// last invoke was auto and should be closed automatically on complete
    uint32_t invokedAt;                         /// Tick value at the command invocation, used for timeout detection.
    char *response;                             /// PTR into IOP "core" buffer, the response to the command received from the BGx. Parser with NULL terminate.
    uint32_t execDuration;                      /// duration of command's execution in milliseconds
    resultCode_t resultCode;                    /// consumer API result value (HTTP style), success=200, timeout=408, single digit BG errors are expected to be offset by 1000
    cmdResponseParser_func responseParserFunc;  /// parser function to analyze AT cmd response and optionally extract value
    uint16_t errorCode;                         /// BGx error code returned, could be CME ERROR (< 100) or subsystem error (generally > 500)
    int32_t retValue;                           /// optional signed int value extracted from response
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

/**
 *	\brief Sets options for BGx AT command control (atcmd). 
 *  \param timeoutMS [in] Number of milliseconds the action can take. Use system default ACTION_TIMEOUTms or your value.
 *  \param cmdResponseParser [in] Custom command response parser to signal result is complete. NULL for std parser.
 */
void atcmd_setOptions(uint32_t timeoutMS, cmdResponseParser_func *cmdResponseParser);


/**
 *	\brief Reset AT command options to defaults.
 *  
 *  \details Use after setOptions() to reset AT-Cmd behaviors to default settings.
 */
void atcmd_restoreOptionDefaults();


/**
 *	\brief Invokes a BGx AT command using default option values (automatic locking).
 *	\param cmdStrTemplate [in] The command string to send to the BG96 module.
 *  \param ... [in] Variadic parameter list to integrate into the cmdStrTemplate.
 *  @return True if action was invoked, false if not
 */
bool atcmd_tryInvoke(const char *cmdTemplate, ...);

/**
 *	\brief Invokes a BGx AT command using set option values, previously set with setOptions() (automatic locking).
 *	\param cmdStrTemplate [in] The command string to send to the BG96 module.
 *  \param ... [in] Variadic parameter list to integrate into the cmdStrTemplate.
 *  @return True if action was invoked, false if not
 */
bool atcmd_tryInvokeWithOptions(const char *cmdTemplate, ...);


/**
 *	\brief Invokes a BGx AT command without acquiring a lock, using previously set setOptions() values.
 *	\param cmdStrTemplate [in] The command string to send to the BG96 module.
 *  \param ... [in] Variadic parameter list to integrate into the cmdStrTemplate.
 */
void atcmd_invokeReuseLock(const char *cmdTemplate, ...);


/**
 *	\brief Closes (completes) a BGx AT command structure and frees action resource (release action lock).
 */
void atcmd_close();


/**
 *	\brief Performs blind send data transfer to device.
 *  \details Operates in without regard to atcmd lock. Doesn't set/reset lock, doesn't check for lock.
 *           Typically used to send subcommand text
 *  \param data [in] - Pointer to the block of character data to send
 *  \param dataSz [in] - The size of the data block
 *  \param eotPhrase [in] - Char string that terminates the sent characters
 */
void atcmd_sendCmdData(const char *data, uint16_t dataSz, const char* eotPhrase);


/**
 *	\brief Checks recv buffer for command response and sets atcmd structure data with result.
 */
resultCode_t atcmd_getResult();


/**
 *	\brief Waits for atcmd result, periodically checking recv buffer for valid response until timeout.
 */
resultCode_t atcmd_awaitResult();


/**
 *	\brief Returns the atCmd result code or 0 if command is pending completion
 */
resultCode_t atcmd_getLastResult();


/**
 *	\brief Returns the atCmd parsed response (preamble to finale) if completed. An empty C-string will returned prior to completion.
 */
char *atcmd_getLastResponse();


/**
 *	\brief Returns the atCmd result code, 0xFFFF or cmdParseRslt_pending if command is pending completion
 */
int32_t atcmd_getLastValue();


/**
 *	\brief Returns the BGx module error code or 0 if no error. Use this function to get details on a resultCode_t = 500
 */
resultCode_t atcmd_getLastError();


/**
 *	\brief Returns the atCmd last execution duration in milliseconds
 */
uint32_t atcmd_getLastDuration();


/**
 *	\brief Sends +++ sequence to transition BGx out of data mode to command mode.
 */
void atcmd_exitDataMode();


/**
 *	\brief Sends ESC character to ensure BGx is not in text mode (">" prompt awaiting ^Z/ESC, MQTT publish etc.).
 */
void atcmd_exitTextMode();


/*-----------------------------------------------------------------------------------------------*/

// direct use standard parsers
cmdParseRslt_t atcmd_okResponseParser(ltemDevice_t *modem);


/**
 *	\brief Resets atCmd struct and optionally releases lock, a BGx AT command structure. 
 *  \details Some AT-cmds will omit preamble under certain conditions; usually indicating an empty response (AT+QIACT? == no PDP active). 
 *           Note: If no stop condition is specified, finale, tokensReqd, and lengthReqd are all omitted, the parser will return with 
 *                 the first block of characters received.
 *           The "value" and "response" variables are cached internally to the atCmd structure and can be retrieved with atcmd_getLastValue()
 *           and atcmd_getLastResponse() functions respectively.
 *  \param [in] preamble - C-string containing the expected phrase that starts response. 
 *  \param [in] preambleReqd - True to require the presence of the preamble for a SUCCESS response
 *  \param [in] delimiters - (optional: 0=nN/A) C-string containing the expected delimiter between response tokens.
 *  \param [in] tokensReqd - (optional: 0=N/A) The minimum count of tokens between preamble and finale for a SUCCESS response.
 *  \param [in] valueIndx - (optional: 0=N/A) Indicates the 1-n position (chars/tokens) of an integer value to grab from the response.
 *  \param [in] finale - (optional: NULL,empty str, use full response) C-string containing the expected phrase that concludes response.
 *  \param [in] lengthReqd - (optional: 0=N/A) The minimum character count between preamble and finale for a SUCCESS response.
 */
cmdParseRslt_t atcmd__stdResponseParser(const char *preamble, bool preambleReqd, const char *delimiters, uint8_t tokensReqd, uint8_t valueIndx, const char *finale, uint16_t lengthReqd);


// base parsers to be wrapped by custom initialization values
// cmdParseRslt_t atcmd__defaultResponseParser(void *atcmd, const char *response, const char *preamble, bool preambleReqd, uint8_t gap, const char *finale);
// cmdParseRslt_t atcmd__tokenCountResponseParser(void *atcmd, const char *response, const char *preamble, char separator, uint8_t reqdTokens, const char *finale);
// cmdParseRslt_t atcmd__serviceResponseParser(void *atcmd, const char *response, const char *preamble, uint8_t resultIndx);
// cmdParseRslt_t atcmd__serviceResponseParserTerm(void *atcmd, const char *response, const char *preamble, uint8_t resultIndx, const char *finale);



#pragma region LTEmC Internal Functions
/* ------------------------------------------------------------------------------------------------
 * Not intended for user application consumption.
 **/

/**
 *  \brief Awaits exclusive access to QBG module command interface.
 *  \param timeoutMS [in] - Number of milliseconds to wait for a lock.
 *  @return true if lock aquired prior to the timeout period.
*/
bool ATCMD_awaitLock(uint16_t timeoutMS);


/**
 *	\brief Returns the current atCmd lock state
 *  \return True if atcmd lock is active (command underway)
 */
bool ATCMD_isLockActive();


/**
 *	\brief Resets atCmd struct and optionally releases lock, a BGx AT command structure.
 */
void ATCMD_reset(bool releaseLock);


// LTEmC INTERNAL prompt parsers
cmdParseRslt_t ATCMD_readyPromptParser(const char *response, const char *rdyPrompt);
cmdParseRslt_t ATCMD_txDataPromptParser(const char *response);
cmdParseRslt_t ATCMD_connectPromptParser(const char *response);

#pragma endregion LTEmC Internal Functions
/* ------------------------------------------------------------------------------------------------
 * End LTEmC Internal Functions */

#ifdef __cplusplus
}
#endif

#endif  // !__LTEMC_ATCMD_H__
