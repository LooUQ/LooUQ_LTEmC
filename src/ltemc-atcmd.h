/** ***************************************************************************
  @file 
  @brief Modem command/response and data transfer functions.

  @author Greg Terrell, LooUQ Incorporated

  \loouq
-------------------------------------------------------------------------------

LooUQ-LTEmC // Software driver for the LooUQ LTEm series cellular modems.
Copyright (C) 2017-2023 LooUQ Incorporated

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
Also add information on how to contact you by electronic and paper mail.

**************************************************************************** */


#ifndef __LTEMC_atcmd_H__
#define __LTEMC_atcmd_H__

#ifdef __cplusplus
extern "C" {
#endif


// #include <stdint.h>
// #include <stdbool.h>
// #include <stddef.h>
// #include <string.h>
// #include <stdlib.h>

//#include "ltemc-types.h"


// /**
//  *	@brief Resets atCmd struct (BGx AT command structure) and optionally releases lock.
//  *  @param [in] releaseLock If false, clears ATCMD internal state, but leaves the command lock state unchanged
//  */
// void atcmd_reset(bool releaseLock);


/**
 *	@brief Resets AT-CMD last execution result properties.
 */
void atcmd_resetPreInvoke();


/**
 *	@brief Resets AT-CMD next execution invoke properties.
 */
void atcmd_resetPostInvoke();


/**
 * @brief Sets command timeout for next invocation of a BGx AT command. 
 * @details If newTimeout is zero (0) no change to timeout is made, the current timeout is returned. Following the next command, the timeout value returns to default value.
 * @param [in] newTimeout Value in milliseconds to wait for a command to complete.
 * @return The value of the existing timeout.
 */
uint16_t atcmd_ovrrdTimeout(uint16_t newTimeout);


/**
 * @brief Sets response parser for next invocation of a BGx AT command. 
 * @details If newParser is NULL the existing parser is CLEARED, but its location is returned. Following the next command, the parser will revert to the default parser function.
 * @param [in] newParser Address of the parser function to use for the next command. 
 * @return The value of the existing timeout.
 */
cmdResponseParser_func atcmd_ovrrdParser(cmdResponseParser_func newParser);


/**
 * @brief Configure default ATCMD response parser for a specific command response.
 * 
 *  @param [in] preamble - C-string containing the expected phrase that starts response. 
 *  @param [in] preambleReqd - True to require the presence of the preamble for a SUCCESS response
 *  @param [in] delimiters - (optional: ""=N/A) C-string containing the expected delimiter between response tokens.
 *  @param [in] tokensReqd - (optional: 0=N/A, 1=first) The minimum count of tokens between preamble and finale for a SUCCESS response.
 *  @param [in] finale - (optional: ""=finale not required) C-string containing the expected phrase that concludes response.
 *  @param [in] lengthReqd - (optional: 0=N/A) The minimum character count between preamble and finale for a SUCCESS response.
 *  @return Parse status result
 */
void atcmd_configParser(const char *preamble, bool preambleReqd, const char *delimiters, uint8_t tokensReqd, const char *finale, uint16_t lengthReqd);

/**
 * @brief Configure atcmd automatic datamode processing
 * 
 * @param [in] streamCtrl Stream control for flow to be serviced. Optional for most writes, generally required for reads
 * @param [in] trigger Character string that prefixes wait for data
 * @param [in] dataHndlr Handler function that services the data transfer 
 * @param [in] dataLoc Pointer to the data to be sent
 * @param [in] dataSz Size of the data block (opperates in transparent data mode, no EOT char/phrase)
 * @param [in] applRecvDataCB Handler function to receive/parse incoming data 
 * @param [in] runParser If true, registered command response parser is invoked after successful data mode processing
 */
void atcmd_configDataMode(streamCtrl_t * streamCtrl, const char* trigger, dataHndlr_func dataHndlr, const char* txDataPtr, uint16_t txDataSz, appRcvr_func applRecvDataCB, bool skipParser);


// /**
//  * @brief Set the TX end-of-transmission (EOT) signally character
//  * @details The EOT character is automatically sent by the IOP module when the TX side of the UART goes idle. Cleared automatically on send.
//  * 
//  * @param eotChar 
//  */
// void atcmd_setDataModeEot(uint8_t eotChar);


/**
 *	@brief Invokes a BGx AT command using default option values (automatic locking).
 *	@param [in] cmdStrTemplate The command string to send to the BG96 module.
 *  @param [in] variadic ... parameter list to integrate into the cmdStrTemplate.
 *  @return resultCode_t Status code representing outcome of AT-CMD execution.
 */
resultCode_t atcmd_dispatch(const char *cmdTemplate, ...);


/**
 *	@brief Invokes a BGx AT command using default option values (automatic locking).
 *	@param [in] cmdStrTemplate The command string to send to the BG96 module.
 *  @param [in] variadic ... parameter list to integrate into the cmdStrTemplate.
 *  @return True if action was invoked, false if not
 */
bool atcmd_tryInvoke(const char *cmdTemplate, ...);


/**
 *	@brief Invokes a BGx AT command without acquiring a lock, using previously set setOptions() values.
 *	@param [in] cmdStrTemplate The command string to send to the BG96 module.
 *  @param [in] variadic ... parameter list to integrate into the cmdStrTemplate.
 */
void atcmd_invokeReuseLock(const char *cmdTemplate, ...);


/**
 * @brief 
 * 
 * @param timeoutMS 
 */
bool atcmd_awaitLock(uint16_t timeoutMS);


/**
 *	@brief Closes (completes) a BGx AT command structure and frees action resource (release action lock).
 */
void atcmd_close();


/**
 *	@brief Waits for atcmd result, periodically checking recv buffer for valid response until timeout.
 */
resultCode_t atcmd_awaitResult();


// /**
//  *	@brief Waits for atcmd result, periodically checking recv buffer for valid response until timeout.
//  *  @param timeoutMS Time to wait for command response (0==no change). 
//  *  @param cmdResponseParser If provided sets parser as the response parser (NULL==no change). 
//  */
// resultCode_t atcmd_awaitResultWithOptions(uint32_t timeoutMS, cmdResponseParser_func cmdResponseParser);


/**
 *	@brief Returns the atCmd result code or 0 if command is pending completion
 *  @return HTTP style result code.
 */
resultCode_t atcmd_getResult();


/**
 *	@brief Returns the atCmd result code, 0xFFFF or cmdParseRslt_pending if command is pending completion
 */
const char* atcmd_getCommand();


/**
 *	@brief Returns the atCmd parsed response (preamble to finale) if completed. An empty C-string will return prior to completion.
 *  @return Char pointer to the command response (note: this is stripped of preamble and finale strings)
 */
char* atcmd_getRawResponse();


/**
 *	@brief Returns the string captured from the last command response with prefixing white-space and any preamble removed.
 *  @return Const char pointer to the command response (note: this is stripped of preamble and finale strings)
 */
char* atcmd_getResponse();


/**
 * @brief Returns a token from the result of the last module command

 * @param tokenIndx 
 * @return Pointer to token from LTEm internal buffer
 */
char* atcmd_getToken(uint8_t tokenIndx);


/**
 * @brief Grab characters from RX (receive) buffer.
 * @warning GRAB is generally a diagnostic function note intended for general use. It changes the state of the RX
 * stream in potentially detrimental ways.
 * 
 * @param grabBffr Buffer to hold the grabbed characters.
 * @param grabBffrSz Specifies both the size of the buffer and the number of characters being requested.
 */
void ATCMD_GRABRX(char * grabBffr, uint8_t grabBffrSz);


// /**
//  *	@brief Returns the atCmd result value
//  *  @return If the parser was instructed to capture a value (see atcmd_stdResponseParser()) the signed integer value found
//  */
// int32_t atcmd_getValue();


/**
 *	@brief Returns the last dataMode RX read size.
 *  @return uint16_t Length of last dataMode RX transfer.
 */
uint16_t atcmd_getRxLength();


/**
 *	@brief Returns the atCmd parser result code, 0xFFFF or cmdParseRslt_pending if command is pending completion
 *  @return The PARSER result from the last interation of the parser execution. This is generally not applicable to end-user applications.
 */
cmdParseRslt_t atcmd_getParserResult();


/**
 *	@brief Returns the atCmd last execution duration in milliseconds
 *  @return The number of milliseconds. If the command timed out this will be approximately timeout value.
 */
uint32_t atcmd_getDuration();


/**
 *	@brief Sends ^Z character to ensure BGx is not in text mode.
 */
void atcmd_exitTextMode();


/**
 *	@brief Sends break sequence to transition BGx out of fixed-size data mode to command mode (up to 1500 char).
 */
void atcmd_exitDataMode();


/**
 *	@brief Sends +++ sequence to transition BGx out of transparent data mode to command mode.
 */
void atcmd_exitTransparentMode();


/* Response Parsers
 * --------------------------------------------------------------------------------------------- */

cmdParseRslt_t atcmd_defaultResponseParser();

/**
 *	@brief Resets atCmd struct and optionally releases lock, a BGx AT command structure. 
 *  @details LTEmC internal function, not static as it is used by several LTEmC modules
 *           Some AT-cmds will omit preamble under certain conditions; usually indicating an empty response (AT+QIACT? == no PDP active). 
 *           Note: If no stop condition is specified, finale, tokensReqd, and lengthReqd are all omitted, the parser will return with 
 *                 the first block of characters received.
 *           The "value" and "response" variables are cached internally to the atCmd structure and can be retrieved with atcmd_getValue()
 *           and atcmd_getResponse() functions respectively.
 *  @param [in] preamble - C-string containing the expected phrase that starts response. 
 *  @param [in] preambleReqd - True to require the presence of the preamble for a SUCCESS response
 *  @param [in] delimiters - (optional: ""=N/A) C-string containing the expected delimiter between response tokens.
 *  @param [in] tokensReqd - (optional: 0=N/A, 1=first) The minimum count of tokens between preamble and finale for a SUCCESS response.
 *  @param [in] valueIndx - (optional: 0=N/A, 1=first) Indicates the 1-n position (chars/tokens) of an integer value to grab from the response.
 *  @param [in] finale - (optional: ""=finale not required) C-string containing the expected phrase that concludes response.
 *  @param [in] lengthReqd - (optional: 0=N/A) The minimum character count between preamble and finale for a SUCCESS response.
 *  @return Parse status result
 */
cmdParseRslt_t atcmd_stdResponseParser(const char *preamble, bool preambleReqd, const char *delimiters, uint8_t tokensReqd, uint8_t valueIndx, const char *finale, uint16_t lengthReqd);


// /**
//  *	@brief LTEmC internal testing parser to capture incoming response until timeout. This is generally not used by end-user applications.
//  *  @return Parse status result (always pending for this test parser)
//  */
// cmdParseRslt_t atcmd_testResponseTrace();


/**
 *	@brief Stardard TX (out) data handler used by dataMode. Sends data and checks for OK response.
 */
resultCode_t atcmd_txHndlrDefault();


// /**
//  *	@brief TX (out) data handler that performs a blind send of data.
//  */
// resultCode_t atcmd_txDataHndlrRaw();


// /**
//  *	@brief Stardard RX (in) data handler used by dataMode.
//  */
// resultCode_t atcmd_stdRxDataHndlr();


/**
 * @brief Stream RX data handler accepting data length at RX buffer tail.
 */
resultCode_t atcmd_rxHndlrWithLength();


#ifdef __cplusplus
}
#endif
#endif  // !__LTEMC_atcmd_H__
