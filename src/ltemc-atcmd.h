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

// #include "ltemc-iTypes.h"
// #include <lq-bBuffer.h>
//#include "ltemc-streams.h"


/* --------------------------------------------------------------------------------------------- */
#pragma region Enum Declarations
/* --------------------------------------------------------------------------------------------- */

/**
 * @brief ATCMD Module Constants
 */
enum atcmd__constants
{
    atcmd__noTimeoutChange = 0,
    atcmd__defaultTimeout = 800,
    atcmd__cmdBufferSz = 448,               // prev=120, mqtt(Azure) connect=384, new=512 for universal cmd coverage, data mode to us dynamic TX bffr switching
    atcmd__respBufferSz = 128,
    atcmd__respTokenSz = 64,
    atcmd__streamPrefixSz = 12,             // obsolete with universal data mode switch
    atcmd__dataModeTriggerSz = 13
};

/**
 * @brief State model for a datamode control
 */
typedef enum dmState_tag
{
    dmState_idle = 0,
    dmState_enabled = 1,
    dmState_active = 2
} dmState_t;


/** 
 *  \brief AT command response parser result codes.
*/
typedef enum cmdParseRslt_tag
{
    cmdParseRslt_pending = 0x00,

    cmdParseRslt_preambleMissing = 0x01,
    cmdParseRslt_countShort = 0x02,
    cmdParseRslt_moduleError = 0x04,

    cmdParseRslt_success = 0x40,
    cmdParseRslt_error = 0x80
} cmdParseRslt_t;


/**
 * @brief Stream types supported by LTEmC 
 */
typedef enum streamType_tag
{
    streamType_none = 0,
    streamType_UDP = 'U',
    streamType_TCP = 'T',
    streamType_SSLTLS = 'S',
    streamType_MQTT = 'M',
    streamType_HTTP = 'H',
    streamType_file = 'F',
    streamType_SCKT = 'K',
    streamType__ANY = '*'
} streamType_t;


/* --------------------------------------------------------------------------------------------- */
#pragma endregion Enum Declarations
/* --------------------------------------------------------------------------------------------- */


/* --------------------------------------------------------------------------------------------- */
#pragma region Function Prototype Declarations
/* --------------------------------------------------------------------------------------------- */

/**
 * @brief AT command RESPONSE parser
 * @details Parses and validates device response to AT commands
 */
typedef cmdParseRslt_t (*cmdResponseParser_func)(void);                 // command response parser


/**
 * @brief Stream URC event handler.
 * @details Each stream MAY have a URC handler for ASYNCHRONOUS events. It MAY have a SYNCRONOUS handler invoked inline to command
 * processing. These handlers processing incoming data from the RX buffer, parse it, and marshal the received data to the application.
 */
typedef resultCode_t (*urcEvntHndlr_func)();                            // callback into stream specific URC handler (async recv)


/**
 * @brief DATA-MODE receiver (ATCMD processor). 
 * @details Each stream type will have one function (possibly multiple functions) that match this prototype and a capable of 
 * processing the stream coming in from LTEm device via the block buffer and can forward to the application.
 */
typedef resultCode_t (*dmRcvr_func)(void);                              // data mode buffer receiver (processes data stream)
/**
 * @brief Data handler: generic function signature that can be a stream sync receiver or a general purpose ATCMD data mode handler
 */
//typedef void (*dataHndlr_func)(void);                                   // callback into stream data handler (sync transfer)
typedef resultCode_t (*dataHndlr_func)(void);                                   // callback into stream data handler (sync transfer)


/** @brief Generic APPLICATION callback data receiver (in stream header) cast to a stream specific receiver signature.
 *  @details Each stream type has a concrete data receive type tailored to the specifics of the stream (defined in <stream>.h)
 *  Examples: MQTT receiver conveys topic, file receiver includes handle
 */
typedef void (*appGenRcvr_func)(void);

/* --------------------------------------------------------------------------------------------- */
#pragma endregion Function Prototype Declarations
/* --------------------------------------------------------------------------------------------- */


/* --------------------------------------------------------------------------------------------- */
#pragma region Structure Declarations
/* --------------------------------------------------------------------------------------------- */

/**
 * @brief Control structure configuring ATCMD dataMode
 * @details Data mode is a preconfigured automatic transition to sending/receiving data within the context of a command. Switching into/out of 
 * data mode is completed by the command processing subsystem as configured in this structure, initialized with ATCMD_configDataMode().
 */
typedef struct dataMode_tag                                         // the command processor's automatic transition to deliver a block of data inside a command
{
    dmState_t dmState;                                              // current state of the data mode transition
    uint16_t contextKey;                                            // unique identifier for data flow, could be dataContext(proto), handle(files), etc.
    char trigger[atcmd__dataModeTriggerSz];                         // char sequence that signals the transition to data mode, data mode starts at the following character
    dmRcvr_func dataHndlr;                                          // data handler function (TX/RX)
    appGenRcvr_func appRecvCB;
    char* txDataLoc;                                                // location of data buffer (TX only)
    uint16_t txDataSz;                                              // size of TX data or RX request
    bool runParserAfterDataMode;                                    // true = invoke AT response parser after successful datamode. Data mode error always skips parser
} dataMode_t;


/**
 * @brief Set parameters for standard AT-Cmd response parser
 */
typedef struct atcmdParserConfig_tag 
{
    const char *preamble;
    bool preambleReqd;
    const char *delimeters;
    uint8_t tokensReqd;
    const char *finale;
    uint16_t lengthReqd;
} atcmdParserConfig_t;


/** 
 *  \brief Structure to control invocation and management of an AT command with the BGx module.
*/
typedef struct atcmd_tag
{
    char cmdStr[atcmd__cmdBufferSz];                                // AT command string to be passed to the BGx module.
    uint16_t timeout;                                               // period in MS until command processor should give up and timeout wait for results
    cmdResponseParser_func responseParserFunc;                      // parser function to analyze AT cmd response
    dataMode_t dataMode;                                            // controls for automatic data mode servicing - both TX (out) and RX (in). Std functions or extensions supported.
    uint32_t ownerTaskKey;                                          // for RTOS applications, holds owner task handle with priority over LTEm for a block of actions
    bool ownerLTEmBackground;                                       // for LTEm internal operations; allows override for background processes to complete LTEm actions
    uint32_t execStart;                                             // Tick value at the command invocation, used for timeout detection.
    
    char rawResponse[PSZ(atcmd__respBufferSz)];                     // response buffer, allows for post cmd execution review of received text (0-filled).
    char respToken[PSZ(atcmd__respTokenSz)];                        // buffer to hold a token string grabbed from response
    cmdParseRslt_t parserResult;                                    // last parser invoke result returned
    bool preambleFound;                                             // true if parser found preamble
    char errorDetail[PSZ(ltemSz__errorDetailSz)];                   // BGx error code returned, could be CME ERROR (< 100) or subsystem error (generally > 500)

    uint32_t execDuration;                                          // duration of command's execution in milliseconds
    resultCode_t resultCode;                                        // consumer API result value (HTTP style), success=200, timeout=408, unmapped BGx errors are offset by 1000
    cmdResponseParser_func lastRespPrsrFunc;                        // parser function used for last command
    atcmdParserConfig_t parserConfig;                               // search pattern for standard atcmd parser

    // temporary or deprecated
    char CMDMIRROR[atcmd__cmdBufferSz];                             // waiting on fix to SPI TX overright
    // bool isOpenLocked;                                           // True if the command is still open, AT commands are single threaded and this blocks a new cmd initiation.
    // char* response;                                              // PTR variable section of response.
    // int32_t retValue;                                            // (deprecated) optional signed int value extracted from response
} atcmd_t;

/**
 * @brief Internal generic stream control matching protocol specific controls (for 1st set of fields)
 */
typedef struct streamCtrl_tag
{
    dataCntxt_t dataCntxt;                  // stream context 
    streamType_t streamType;                // stream type (cast to char from enum )
    urcEvntHndlr_func urcHndlr;             // URC handler (invoke by eventMgr)
    dataHndlr_func dataRxHndlr;             // function to handle data streaming, initiated by eventMgr() or atcmd module
    appGenRcvr_func appRcvr;                // application receiver for incoming network data
} streamCtrl_t;



/* --------------------------------------------------------------------------------------------- */
#pragma endregion Structure Declarations
/* --------------------------------------------------------------------------------------------- */


/* --------------------------------------------------------------------------------------------- */
#pragma region Function Declarations
/* --------------------------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C" {
#endif


// /**
//  * @brief Set the TX end-of-transmission (EOT) signally character
//  * @details The EOT character is automatically sent by the IOP module when the TX side of the UART goes idle. Cleared automatically on send.
//  * 
//  * @param eotChar 
//  */
// void ATCMD_setDataModeEot(uint8_t eotChar);

/**
 * @brief Invokes a BGx AT command using default option values (automatic locking).
 * @param [in] cmdStrTemplate The command string to send to the BG96 module.
 * @param [in] variadic ... parameter list to integrate into the cmdStrTemplate.
 * @return True if action was invoked, false if not
 */
bool ATCMD_tryInvoke(const char *cmdTemplate, ...);


/**
 * @brief Waits for atcmd result, periodically checking recv buffer for valid response until timeout.
 */
resultCode_t ATCMD_awaitResult();

/**
 * @brief Sets command timeout for next invocation of a BGx AT command. 
 * @details If newTimeout is zero (0) no change to timeout is made, the current timeout is returned. Following the next command, the timeout value returns to default value.
 * @param [in] newTimeout Value in milliseconds to wait for a command to complete.
 * @return The value of the existing timeout.
 */
uint16_t ATCMD_ovrrdTimeout(uint16_t newTimeout);


/**
 * @brief Sets response parser for next invocation of a BGx AT command. 
 * @details If newParser is NULL the existing parser is CLEARED, but its location is returned. Following the next command, the parser will revert to the default parser function.
 * @param [in] newParser Address of the parser function to use for the next command. 
 * @return The value of the existing timeout.
 */
cmdResponseParser_func ATCMD_ovrrdParser(cmdResponseParser_func newParser);

/**
 * @brief Returns the atCmd parser result code, 0xFFFF or cmdParseRslt_pending if command is pending completion
 * @return The PARSER result from the last interation of the parser execution. This is generally not applicable to end-user applications.
 */
cmdParseRslt_t ATCMD_getParserResult();

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
void ATCMD_configDataMode(uint16_t contextKey, const char* trigger, dataHndlr_func dataHndlr, char* txDataLoc, uint16_t txDataSz, appGenRcvr_func applRecvDataCB, bool skipParser);


/**
 * @brief Resets atCmd struct (BGx AT command structure) and optionally releases lock.
 * @param [in] releaseLock If false, clears ATCMD internal state, but leaves the command lock state unchanged
 */
void ATCMD_reset(bool releaseLock);


/**
 * @brief Closes (completes) a BGx AT command structure and frees action resource (release action lock).
 */
void ATCMD_close();


/**
 * @brief Set task/thread owner for exclusive use of LTEm (public command actions)
 * 
 * @param ownerHandle Task/thread owning LTEm resources until owner cleared
 */
void ATCMD_setOwner(uint32_t ownerHandle);


/**
 * @brief Get configured task/thread owner with exclusive use of the LTEm API
 * 
 * @return uint32_t Task/thread handle with ownership of LTEm resources
 */
uint32_t ATCMD_getOwner();


/**
 * @brief Clears configured task/thread owner with exclusive use of the LTEm API.
 * @note No warning or error is signalled if no owner currently configured.
 */
void ATCMD_clearOwner();


/* ================================================================================================
 * Return information about last AT-command execution
 * ============================================================================================= */

/**
 * @brief Returns the atCmd result code or 0 if command is pending completion
 * @return HTTP style result code.
 */
resultCode_t ATCMD_getResult();


/**
 * @brief Returns the atCmd result code, 0xFFFF or cmdParseRslt_pending if command is pending completion
 */
const char* ATCMD_getCommand();


/**
 * @brief Returns the string captured from the last command response with prefixing white-space and any preamble removed.
 * @return Const char pointer to the command response (note: this is stripped of preamble and finale strings)
 */
const char* ATCMD_getRawResponse();


/**
 * @brief Returns the string captured from the last command response skipping any preamble
 * @return Const char pointer to the command response (note: this is stripped of preamble and finale strings)
 */
const char* ATCMD_getResponseData();


/**
 * @brief Returns true if the last atCmd response contained a requested preamble. If no preamble specified will return false.
 * @return True if preamble string was detected in the AT command response
 */
bool ATCMD_wasPreambleFound();


/**
 * @brief Returns a token from the result of the last module command
 * @param tokenIndx 
 * @return Pointer to token from LTEm internal buffer
 */
char * ATCMD_getToken(uint8_t tokenIndx);


/**
 * @brief Returns the BGx reported CME/CMS error code. Use this function to get details on a resultCode_t = 500
 * @return Pointer to a error value reported by the BGx module. Note that not all BGx errors have a detailed descriptor.
 */
const char * ATCMD_getErrorDetail();


/**
 * @brief Returns the BGx reported CME/CMS error code as a numeric value.
 * @return Numeric CM* error code, 999 otherwise.
 */
uint16_t ATCMD_getErrorDetailCode();


/**
 * @brief Returns the atCmd last execution duration in milliseconds
 * @return The number of milliseconds. If the command timed out this will be approximately timeout value.
 */
uint32_t ATCMD_getDuration();


/**
 * @brief Sends ^Z character to ensure BGx is not in text mode.
 */
void ATCMD_exitTextMode();


/**
 * @brief Sends break sequence to transition BGx out of fixed-size data mode to command mode (up to 1500 char).
 */
void ATCMD_exitDataMode();


/**
 * @brief Sends +++ sequence to transition BGx out of transparent data mode to command mode.
 */
void ATCMD_exitTransparentMode();


/**
 * @brief Set parameters for standard AT-Cmd response parser
 * @details LTEmC internal function, not static as it is used by several LTEmC modules
 *           Some AT-cmds will omit preamble under certain conditions; usually indicating an empty response (AT+QIACT? == no PDP active). 
 *           Note: If no stop condition is specified, finale, tokensReqd, and lengthReqd are all omitted, the parser will return with 
 *                 the first block of characters received.
 *           The "value" and "response" variables are cached internally to the atCmd structure and can be retrieved with atcmd_getValue()
 *           and atcmd_getResponse() functions respectively.
 * @param [in] preamble - C-string containing the expected phrase that starts response. 
 * @param [in] preambleReqd - True to require the presence of the preamble for a SUCCESS response
 * @param [in] delimiters - (optional: ""=N/A) C-string containing the expected delimiter between response tokens.
 * @param [in] tokensReqd - (optional: 0=N/A, 1=first) The minimum count of tokens between preamble and finale for a SUCCESS response.
 * @param [in] finale - (optional: ""=finale not required) C-string containing the expected phrase that concludes response.
 * @param [in] lengthReqd - (optional: 0=N/A) The minimum character count between preamble and finale for a SUCCESS response.
 */
void ATCMD_configureResponseParser(const char *pPreamble, bool preambleReqd, const char *pDelimeters, uint8_t tokensReqd, const char *pFinale, uint16_t lengthReqd);



/* --------------------------------------------------------------------------------------------- */
#pragma region RESPONSE Parsers
/* --------------------------------------------------------------------------------------------- */

/**
 * @brief Resets atCmd struct and optionally releases lock, a BGx AT command structure. 
 * @details LTEmC internal function, not static as it is used by several LTEmC modules
 *           Some AT-cmds will omit preamble under certain conditions; usually indicating an empty response (AT+QIACT? == no PDP active). 
 *           Note: If no stop condition is specified, finale, tokensReqd, and lengthReqd are all omitted, the parser will return with 
 *                 the first block of characters received.
 *           The "value" and "response" variables are cached internally to the atCmd structure and can be retrieved with atcmd_getValue()
 *           and atcmd_getResponse() functions respectively.
 * @param [in] preamble - C-string containing the expected phrase that starts response. 
 * @param [in] preambleReqd - True to require the presence of the preamble for a SUCCESS response
 * @param [in] delimiters - (optional: ""=N/A) C-string containing the expected delimiter between response tokens.
 * @param [in] tokensReqd - (optional: 0=N/A, 1=first) The minimum count of tokens between preamble and finale for a SUCCESS response.
 * @param [in] valueIndx - (optional: 0=N/A, 1=first) Indicates the 1-n position (chars/tokens) of an integer value to grab from the response.
 * @param [in] finale - (optional: ""=finale not required) C-string containing the expected phrase that concludes response.
 * @param [in] lengthReqd - (optional: 0=N/A) The minimum character count between preamble and finale for a SUCCESS response.
 * @return Parse status result
 */
cmdParseRslt_t ATCMD_stdResponseParser(const char *preamble, bool preambleReqd, const char *delimiters, uint8_t tokensReqd, uint8_t valueIndx, const char *finale, uint16_t lengthReqd);


/**
 * @brief Stardard TX (out) data handler used by dataMode. Sends data and checks for OK response.
 */
resultCode_t ATCMD_txOkDataHndlr();


/**
 * @brief TX (out) data handler that performs a blind send of data.
 */
resultCode_t ATCMD_txBlindDataHndlr();


/**
 * @brief Stardard RX (in) data handler used by dataMode.
 */
resultCode_t ATCMD_rxDataHndlr();


/**
 *  \brief Default AT command result parser.
*/
cmdParseRslt_t ATCMD_okResponseParser();

// /**
//  *  \brief Awaits exclusive access to QBG module command interface.
//  *  \param timeoutMS [in] - Number of milliseconds to wait for a lock.
//  *  @return true if lock aquired prior to the timeout period.
// */
// bool ATCMD_awaitLock(uint16_t timeoutMS);

// /**
//  *	\brief Returns the current atCmd lock state
//  *  \return True if atcmd lock is active (command underway)
//  */
// bool ATCMD_isLockActive();

#pragma endregion ATCMD Response Parsers
/* --------------------------------------------------------------------------------------------- */


/* --------------------------------------------------------------------------------------------- */
#pragma region PROMPT Parsers
/* --------------------------------------------------------------------------------------------- */

/**
 *	\brief Base parser looking for a simple prompt string.
 */
cmdParseRslt_t ATCMD_readyPromptParser(const char *rdyPrompt);

/**
 *	\brief Parser looking for BGx transmit data prompt "> "
 */
cmdParseRslt_t ATCMD_txDataPromptParser();

/**
 *	\brief Parser looking for BGx CONNECT data prompt.
 */
cmdParseRslt_t ATCMD_connectPromptParser();

#pragma endregion
/* --------------------------------------------------------------------------------------------- */


/* --------------------------------------------------------------------------------------------- */
#pragma region STREAM Registration Functions
/* --------------------------------------------------------------------------------------------- */


/**
 * @brief Register a stream; aka add it from the active streams array
 * 
 * @param streamHdr 
 */
void STREAM_register(streamCtrl_t *streamHdr);


/**
 * @brief Deregister a stream; aka remove it from the active streams array
 * 
 * @param streamHdr 
 */
void STREAM_deregister(streamCtrl_t *streamHdr);


/**
 * @brief Find a stream using the data context number and type
 * 
 * @param dataCntxt 
 * @param streamType 
 * @return streamCtrlHdr_t* 
 */
streamCtrl_t* STREAM_find(uint8_t dataCntxt, streamType_t streamType);


#pragma endregion
/* --------------------------------------------------------------------------------------------- */


#ifdef __cplusplus
}
#endif
#pragma endregion Function Declarations
/* --------------------------------------------------------------------------------------------- */

#endif  // !__LTEMC_ATCMD_H__
