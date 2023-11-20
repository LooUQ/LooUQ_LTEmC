/** ***************************************************************************
  @file 
  @brief Modem command/response and data transfer functions.

  @author Greg Terrell, LooUQ Incorporated

  \loouq

  @warning Internal dependencies, changes only as directed by LooUQ staff.

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

#ifndef __LTEMC_ATCMD_H__
#define __LTEMC_ATCMD_H__

#ifdef __cplusplus
extern "C" {
#endif


// #include <stdint.h>
// #include <stdbool.h>
// #include <stddef.h>
// #include <string.h>
// #include <stdlib.h>

#include "ltemc-iTypes.h"


/**
 * @brief Configure atcmd automatic datamode processing
 * 
 * @param [in] contextKey Data context this data mode process configuration applies to
 * @param [in] trigger Character string that prefixes wait for data
 * @param [in] dataHndlr Handler function that services the data transfer 
 * @param [in] dataLoc Pointer to the data to be sent
 * @param [in] dataSz Size of the data block (opperates in transparent data mode, no EOT char/phrase)
 * @param [in] applRecvDataCB Handler function to receive/parse incoming data 
 * @param [in] runParser If true, registered command response parser is invoked after successful data mode processing
 */
void atcmd_configDataMode(uint16_t contextKey, const char* trigger, dataRxHndlr_func rxDataHndlr, char* txDataLoc, uint16_t txDataSz, appRcvProto_func applRecvDataCB, bool skipParser);


// /**
//  * @brief Set the TX end-of-transmission (EOT) signally character
//  * @details The EOT character is automatically sent by the IOP module when the TX side of the UART goes idle. Cleared automatically on send.
//  * 
//  * @param eotChar 
//  */
// void atcmd_setDataModeEot(uint8_t eotChar);

/**
 * @brief Invokes a BGx AT command using default option values (automatic locking).
 * @param [in] cmdStrTemplate The command string to send to the BG96 module.
 * @param [in] variadic ... parameter list to integrate into the cmdStrTemplate.
 * @return True if action was invoked, false if not
 */
bool atcmd_tryInvoke(const char *cmdTemplate, ...);


/**
 * @brief Waits for atcmd result, periodically checking recv buffer for valid response until timeout.
 */
resultCode_t atcmd_awaitResult();


/**
 * @brief Resets atCmd struct (BGx AT command structure) and optionally releases lock.
 * @param [in] releaseLock If false, clears ATCMD internal state, but leaves the command lock state unchanged
 */
void ATCMD_reset(bool releaseLock);


/**
<<<<<<< Updated upstream
 * @brief Closes (completes) a BGx AT command structure and frees action resource (release action lock).
 */
void ATCMD_close();


/**
 * @brief Returns the atCmd result code or 0 if command is pending completion
 * @return HTTP style result code.
 */
resultCode_t atcmd_getResult();


/**
 * @brief Returns the atCmd result code, 0xFFFF or cmdParseRslt_pending if command is pending completion
=======
 *	@brief Returns the atCmd result code, 0xFFFF or cmdParseRslt_pending if command is pending completion
>>>>>>> Stashed changes
 */
const char* atcmd_getCommand();


// /**
//  * @brief Returns the atCmd parsed response (preamble to finale) if completed. An empty C-string will return prior to completion.
//  * @return Char pointer to the command response (note: this is stripped of preamble and finale strings)
//  */
// char* atcmd_getRawResponse();


/**
<<<<<<< Updated upstream
 * @brief Returns the string captured from the last command response with prefixing white-space and any preamble removed.
 * @return Const char pointer to the command response (note: this is stripped of preamble and finale strings)
=======
 *	@brief Returns the atCmd result code or 0 if command is pending completion
 *  @return HTTP style result code.
 */
resultCode_t atcmd_getResultCode();


/**
 *	@brief Returns the string captured from the last command response with prefixing white-space and any preamble removed.
 *  @return Char pointer to the command response
>>>>>>> Stashed changes
 */
const char* atcmd_getRawResponse();


/**
<<<<<<< Updated upstream
 * @brief Returns the string captured from the last command response skipping any preamble
 * @return Const char pointer to the command response (note: this is stripped of preamble and finale strings)
=======
 *	@brief Returns the atCmd parsed response (preamble to finale) if completed. An empty C-string will return prior to completion.
 *  @return Char pointer to the parsed command response (note: this is stripped of preamble and finale strings)
>>>>>>> Stashed changes
 */
const char* atcmd_getResponseData();


/**
 * @brief Returns true if the last atCmd response contained a requested preamble. If no preamble specified will return false.
 * @return True if preamble string was detected in the AT command response
 */
bool atcmd_wasPreambleFound();


/**
 * @brief Returns a token from the result of the last module command
 * @param tokenIndx 
 * @return Pointer to token from LTEm internal buffer
 */
char* atcmd_getToken(uint8_t tokenIndx);


/**
 * @brief Returns the BGx reported CME/CMS error code. Use this function to get details on a resultCode_t = 500
 * @return Pointer to a error value reported by the BGx module. Note that not all BGx errors have a detailed descriptor.
 */
char *atcmd_getErrorDetail();


/**
 * @brief Returns the BGx reported CME/CMS error code as a numeric value.
 * @return Numeric CM* error code, 999 otherwise.
 */
uint16_t atcmd_getErrorDetailCode();


/**
 * @brief Returns the atCmd last execution duration in milliseconds
 * @return The number of milliseconds. If the command timed out this will be approximately timeout value.
 */
uint32_t atcmd_getDuration();


/**
 * @brief Sends ^Z character to ensure BGx is not in text mode.
 */
void atcmd_exitTextMode();


/**
 * @brief Sends break sequence to transition BGx out of fixed-size data mode to command mode (up to 1500 char).
 */
void atcmd_exitDataMode();


/**
 * @brief Sends +++ sequence to transition BGx out of transparent data mode to command mode.
 */
void atcmd_exitTransparentMode();




#ifdef __cplusplus
}
#endif
#endif  // !__LTEMC_ATCMD_H__
