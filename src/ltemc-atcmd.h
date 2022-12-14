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

#include "ltemc-types.h"
#ifdef __cplusplus
extern "C" {
#endif

// /**
//  *	\brief Sets options for BGx AT command control (atcmd). 
//  *  \param timeoutMS [in] Number of milliseconds the action can take. Use system default ACTION_TIMEOUTms or your value.
//  *  \param cmdResponseParser [in] Custom command response parser to signal result is complete. NULL for std parser.
//  */
// void atcmd_setOptions(uint32_t timeoutMS, cmdResponseParser_func cmdResponseParser);


/**
 *	\brief Resets atCmd struct and optionally releases lock, a BGx AT command structure.
 *  \param releaseLock [in] If false, clears ATCMD internal state, but leaves the command lock state unchanged
 */
void atcmd_reset(bool releaseLock);


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
 *  \return True if action was invoked, false if not
 */
bool atcmd_tryInvoke(const char *cmdTemplate, ...);


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
 *	\brief Waits for atcmd result, periodically checking recv buffer for valid response until timeout.
 */
resultCode_t atcmd_awaitResult();


/**
 *	\brief Waits for atcmd result, periodically checking recv buffer for valid response until timeout.
 *  \param timeoutMS Time to wait for command response (0==no change). 
 *  \param cmdResponseParser If provided sets parser as the response parser (NULL==no change). 
 */
resultCode_t atcmd_awaitResultWithOptions(uint32_t timeoutMS, cmdResponseParser_func cmdResponseParser);


/**
 *	\brief Returns the atCmd result code or 0 if command is pending completion
 *  \return HTTP style result code.
 */
resultCode_t atcmd_getResult();


/**
 *	\brief Returns the atCmd parsed response (preamble to finale) if completed. An empty C-string will returned prior to completion.
 *  \return Char pointer to the command response (note: this is stripped of preamble and finale strings)
 */
char *atcmd_getResponse();


/**
 *	\brief Returns true if the last atCmd response contained a requested preamble. If no preamble specified will return false.
 *  \return True if preamble string was detected in the AT command response
 */
bool atcmd_getPreambleFound();


/**
 *	\brief Returns the atCmd result value
 *  \return If the parser was instructed to capture a value (see atcmd_stdResponseParser()) the signed integer value found
 */
int32_t atcmd_getValue();


/**
 *	\brief Returns the atCmd parser result code, 0xFFFF or cmdParseRslt_pending if command is pending completion
 *  \return The PARSER result from the last interation of the parser execution. This is generally not applicable to end-user applications.
 */
cmdParseRslt_t atcmd_getParserResult();


/**
 *	\brief Returns the BGx reported CME/CMS error code. Use this function to get details on a resultCode_t = 500
 *  \return Pointer to a error value reported by the BGx module. Note that not all BGx errors have a detailed descriptor.
 */
char *atcmd_getErrorDetail();


/**
 *	\brief Returns the atCmd last execution duration in milliseconds
 *  \return The number of milliseconds. If the command timed out this will be approximately timeout value.
 */
uint32_t atcmd_getDuration();


/**
 *	\brief Sends +++ sequence to transition BGx out of data mode to command mode.
 */
void atcmd_exitDataMode();


/**
 *	\brief Sends ESC character to ensure BGx is not in text mode (">" prompt awaiting ^Z/ESC, MQTT publish etc.).
 */
void atcmd_exitTextMode();

/*-----------------------------------------------------------------------------------------------*/

/**
 *	\brief Resets atCmd struct and optionally releases lock, a BGx AT command structure. 
 *  \details LTEmC internal function, not static as it is used by several LTEmC modules
 *           Some AT-cmds will omit preamble under certain conditions; usually indicating an empty response (AT+QIACT? == no PDP active). 
 *           Note: If no stop condition is specified, finale, tokensReqd, and lengthReqd are all omitted, the parser will return with 
 *                 the first block of characters received.
 *           The "value" and "response" variables are cached internally to the atCmd structure and can be retrieved with atcmd_getValue()
 *           and atcmd_getResponse() functions respectively.
 *  \param [in] preamble - C-string containing the expected phrase that starts response. 
 *  \param [in] preambleReqd - True to require the presence of the preamble for a SUCCESS response
 *  \param [in] delimiters - (optional: ""=N/A) C-string containing the expected delimiter between response tokens.
 *  \param [in] tokensReqd - (optional: 0=N/A, 1=first) The minimum count of tokens between preamble and finale for a SUCCESS response.
 *  \param [in] valueIndx - (optional: 0=N/A, 1=first) Indicates the 1-n position (chars/tokens) of an integer value to grab from the response.
 *  \param [in] finale - (optional: ""=finale not required) C-string containing the expected phrase that concludes response.
 *  \param [in] lengthReqd - (optional: 0=N/A) The minimum character count between preamble and finale for a SUCCESS response.
 *  \return Parse status result
 */
cmdParseRslt_t atcmd_stdResponseParser(const char *preamble, bool preambleReqd, const char *delimiters, uint8_t tokensReqd, uint8_t valueIndx, const char *finale, uint16_t lengthReqd);


/**
 *	\brief LTEmC internal testing parser to capture incoming response until timeout. This is generally not used by end-user applications.
 *  \return Parse status result (always pending for this test parser)
 */
cmdParseRslt_t ATCMD_testResponseTrace();


#ifdef __cplusplus
}
#endif

#endif  // !__LTEMC_ATCMD_H__
