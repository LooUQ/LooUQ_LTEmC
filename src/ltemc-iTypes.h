/** ***************************************************************************
  @file 
  @brief LTEm public API typedef/function prototype declarations.

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


#ifndef __LTEMC_ITYPES_H__
#define __LTEMC_ITYPES_H__

/* LTEmC is C99, this header is only referenced internally
 * https://stackoverflow.com/questions/13642827/cstdint-vs-stdint-h
*/

// TODO pull conditional, leave only std*.h

// #ifdef __cplusplus
// #include <cstdint>
// #include <cstdlib>
// #include <cstdbool>
// #else
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
// #endif // __cplusplus


#include <string.h>
#include <stdlib.h>

#include <lq-types.h>
#include <lq-str.h>                         // most LTEmC modules use LooUQ string functions
#include <lq-bBuffer.h>

#include "ltemc-types.h"
#include "ltemc.h"
#include "ltemc-quectel-bg.h"
#include "ltemc-nxp-sc16is.h"
#include "ltemc-iop.h"

#define PRODUCT "LM" 




// /* ATCMD Module Type Definitions
//  * ------------------------------------------------------------------------------------------------------------------------------*/

// /** 
//  *  \brief Typed constants for AT-CMD module.
// */
// enum atcmd__constants
// {
//     atcmd__noTimeoutChange = 0,
//     atcmd__defaultTimeout = 800,

//     atcmd__cmdBufferSz = 448,                       // prev=120, mqtt(Azure) connect=384, new=512 for universal cmd coverage, data mode to us dynamic TX bffr switching
//     atcmd__respBufferSz = 128,
//     atcmd__respTokenSz = 64,

//     atcmd__streamPrefixSz = 12,                     // obsolete with universal data mode switch
//     atcmd__dataModeTriggerSz = 13
// };


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
    cmdParseRslt_error = 0x80,
} cmdParseRslt_t;


typedef enum dmState_tag
{
    dmState_idle = 0,
    dmState_enabled = 1,
    dmState_active = 2
} dmState_t;


// /* SSL/TLS Module Type Definitions
//  * ------------------------------------------------------------------------------------------------------------------------------*/

// /** 
//  *  @brief Enum of available SSL version options for an SSL connection
// */
// typedef enum tlsVersion_tag
// {
//     tlsVersion_none = 255,
//     tlsVersion_ssl30 = 0,
//     tlsVersion_tls10 = 1,
//     tlsVersion_tls11 = 2,
//     tlsVersion_tls12 = 3,
//     tlsVersion_any = 4,                                 // BGx default

//     tlsVersion_default = 4
// } tlsVersion_t;


// /** 
//  *  @brief Enum of the available cipher suites for TLS processing
// */
// typedef enum tlsCipher_tag
// {
//     tlsCipher_rsaAes256CbcSha = 0X0035,
//     tlsCipher_rsaAes128CbcSha = 0X002F,
//     tlsCipher_rsaRc4128Sha = 0x0005,
//     tlsCipher_rsaRc4128Md5 = 0x0004,
//     tlsCipher_rsa3desEdeCbcSha = 0x000A,
//     tlsCipher_rsaAes256CbcSha256 = 0x003D,

//     tlsCipher_ecdheRsaRc4128Sha = 0xC011,
//     tlsCipher_ecdheRsa3desEdeCbcSha = 0xC012,
//     tlsCipher_ecdheRsaAes128CbcSha = 0xC013,
//     tlsCipher_ecdheRsaAes256CbcSha = 0xC014,
//     tlsCipher_ecdheRsaAes128CbcSha256 = 0xC027,
//     tlsCipher_ecdheRsaAes256CbcSha384 = 0xC028,
//     tlsCipher_ecdheRsaAes128GcmSha256 = 0xC02F,

//     tlsCipher_any = 0xFFFF,                             // BGx default

//     tlsCipher_default = 0xFFFF
// } tlsCipher_t;


// /** 
//  *  @brief Enum of the options for certificate expiration date/time checking
// */
// typedef enum tlsCertExpiration_tag
// {
//     tlsCertExpiration_check = 0,
//     tlsCertExpiration_ignore = 1,                 // BGx default

//     tlsCertExpiration_default = 1
// } tlsCertExpiration_t;


// /** 
//  *  @brief Enum of the certification validation options
// */
// typedef enum tlsSecurityLevel_tag
// {
//     tlsSecurityLevel_noAuthentication = 0,              // BGx default
//     tlsSecurityLevel_serverAuthentication = 1,
//     tlsSecurityLevel_serverClientAuthentication = 2,

//     tlsSecurityLevel_default = 0
// } tlsSecurityLevel_t;


// /** 
//  *  @brief Streams 
//  *  @details Structures for stream control/processing
//  * ================================================================================================
//  */

typedef enum streamType_tag
{
    streamType_UDP = 'U',
    streamType_TCP = 'T',
    streamType_SSLTLS = 'S',
    streamType_MQTT = 'M',
    streamType_HTTP = 'H',
    streamType_file = 'F',
    streamType_SCKT = 'K',
    streamType__ANY = 0
} streamType_t;


/* IOP Module Type Definitions
 * ------------------------------------------------------------------------------------------------------------------------------*/

/**
 *	\brief Typed numeric Input/Output Processor subsystem contants.
 */
enum IOP__Constants
{
    //                                 /* can be reduced based on you protocol selections and your data segment sizes */
    // IOP__txBufferSize = 1800,       // size should be equal or greater than length of largest data transmission
    // IOP__txCmdBufferSize = 192,
    // IOP__rxCoreBufferSize = 192,

    IOP__uartBaudRate = 115200,     // baud rate between BGx and NXP UART
    IOP__uartFIFOBufferSz = 64,
    IOP__uartFIFOFillPeriod = (int)(1 / (double)IOP__uartBaudRate * 10 * IOP__uartFIFOBufferSz * 1000) + 1,

    IOP__rxDefaultTimeout = IOP__uartFIFOFillPeriod * 2,
    IOP__urcDetectBufferSz = 40
};



/** 
 *  @brief Typed numeric constants for stream peers subsystem (sockets, mqtt, http)
 */
enum streams__constants
{
    streams__ctrlMagic = 0x186F,
    streams__maxContextProtocols = 5,
    streams__typeCodeSz = 4,
    streams__urcPrefixesSz = 60
};


// /* ================================================================================================================================
//  * LTEmC module definitions
//  * ==============================================================================================================================*/



// /** 
//  *  \brief Typed numeric constants for network subsystem.
// */
// enum ntwk
// {
//     ntwk__pdpContextCnt = 4,            // varies by carrier: Verizon=2, (Aeris)ATT=3
//     ntwk__operatorNameSz = 20,
//     ntwk__iotModeNameSz = 11,
//     ntwk__pdpProtoSz = 7,
//     ntwk__ipAddressSz = 40,
//     ntwk__pdpNtwkConfigSz = 60,

//     ntwk__scanSeqSz = 12,
//     ntwk__imeiSz = 15,
//     ntwk__iccidSz = 24,

//     ntwk__dvcMfgSz = 18,
//     ntwk__dvcModelSz = 18,
//     ntwk__dvcFwVerSz = 20,
// };

// // // BGx representation
// // #define PDP_PROTOCOL_IPV4 "IP"
// // #define PDP_PROTOCOL_IPV6 "IPV6"
// // #define PDP_PROTOCOL_IPV4V6 "IPV4V6"
// // #define PDP_PROTOCOL_PPP "PPP"


// typedef enum pdpProtocol_tag
// {
//     pdpProtocol_notSet = 0,
//     pdpProtocol_IPV4 = 1,
//     pdpProtocol_IPV6 = 2,
//     pdpProtocol_IPV4V6 = 3,
//     pdpProtocol_PPP = 99                    // not supported by LTEmC
// } pdpProtocol_t;


// // /** 
// //  *  \brief Enum of the two available PDP contexts for BGx provided by network carriers.
// // */
// // typedef enum pdpProtocolType_tag
// // {
// //     pdpProtocolType_IPV4 = 1,      // IP v4, 32-bit address (ex: 192.168.37.52)
// //     pdpProtocolType_IPV6 = 2,      // IP v6, 128-bit address (ex: 2001:0db8:0000:0000:0000:8a2e:0370:7334)
// //     pdpProtocolType_IPV4V6 = 3,
// //     pdpProtocolType_PPP = 9
// // } pdpProtocolType_t;



// /* Function Prototype Definitions 
//  * ------------------------------------------------------------------------------------------------------------------------------*/

// typedef void (*doWork_func)();                                       // module background worker
// typedef void (*powerSaveCallback_func)(uint8_t newPowerSaveState);   // callback for return from a powersave sleep
typedef resultCode_t (*dataRxHndlr_func)();                             // rxBuffer to stream: this function parses and forwards to application via appRcvProto_func
typedef resultCode_t (*urcEvntHndlr_func)();                            // callback to stream specific URC handler
// typedef void (*appRcvProto_func)();                                  // prototype func() for stream recvData callback
typedef cmdParseRslt_t (*cmdResponseParser_func)();                     // AT response parser template

// HTTP and filesystem run synchronous, they do not have URC events
void MQTT_urcHandler();                                                 // parse/handle MQTT async URC events
void SCKT_urcHandler();                                                 // parse/handle SCKT async URC events


/** 
 *  \brief Struct holding cellular and radio settings.
*/
typedef struct modemSettings_tag
{
    char scanSequence[PSZ(ntwk__scanSeqSz)];
    ntwkScanMode_t scanMode;
    ntwkIotMode_t iotMode;
    char pdpNtwkConfig[ntwk__pdpNtwkConfigSz];  // Invoke ready default context config
} modemSettings_t;


// /** 
//  *  \brief Struct holding information about the physical BGx module.
// */
// typedef struct modemInfo_tag
// {
// 	char imei[PSZ(ntwk__imeiSz)];            // IMEI (15 digits) International Mobile Equipment Identity or IEEE UI (aka MAC, EUI-48 or EUI-64).
// 	char iccid[PSZ(ntwk__iccidSz)];          // ICCID (up to 24 digits) Integrated Circuit Card ID. Set in the SIM card at manufacture.
//     char mfg[PSZ(ntwk__dvcMfgSz)];           // Device manufacturer name
// 	char model[PSZ(ntwk__dvcModelSz)];       // Device model number
// 	char fwver[PSZ(ntwk__dvcFwVerSz)];       // Firmware version of the device
//     char swver[PSZ(ltem__swVerSz)];          // software driver version
// } modemInfo_t;


// /** 
//  *  \brief Struct representing the state of active PDP contexts (aka: APN or data context).
// */
// typedef struct packetNetwork_tag
// {
//     bool isActive;
//     uint8_t pdpContextId;                           // context ID recognized by the carrier (valid are 1 to 16)
//     pdpProtocol_t pdpProtocol;                      // IPv4, IPv6, etc.
// 	char ipAddress[ntwk__ipAddressSz];              // The IP address obtained from the carrier for this context. The IP address of the modem.
// } packetNetwork_t;


// /** 
//  *  \brief Struct respresenting an ACTIVE network carrier/operator.
// */
// typedef struct ntwkOperator_tag
// {
// 	char name[PSZ(ntwk__operatorNameSz)];               // Provider name, some carriers may report as 6-digit numeric carrier ID.
// 	char iotMode[PSZ(ntwk__iotModeNameSz)];             // Network carrier protocol mode: CATM-1 or NB-IOT for BGx.
//     uint8_t defaultContext;
//     uint8_t pdpCntxtCnt;                                // The number of PDP contexts available
//     packetNetwork_t packetNetworks[ntwk__pdpContextCnt];  // Collection of packet network with cell operator. This is typically only 1, but some carriers implement more (ex VZW).
// } ntwkOperator_t;





typedef struct streamCtrl_tag                       // structure to govern LTEmC handling of protocol stream
{
    char streamType;                                // stream type
    dataCntxt_t dataCntxt;                          // integer representing the source of the stream; fixed for protocols, file handle for FS
    dataRxHndlr_func dataRxHndlr;                   // function to handle data streaming, initiated by eventMgr() or atcmd module
    // urcEvntHndlr_func urcHndlr;                     // function to handle data streaming, initiated by eventMgr() or atcmd module
} streamCtrl_t;


// /*
//  * ============================================================================================= */

/** 
 *  \brief Struct for the IOP subsystem state. During initialization a pointer to this structure is reference in g_ltem1.
 * 
 *  Each of the protocols (sockets, MQTT, HTTP) have unique behaviors: sockets receive asynchronous alerts but the data
 *  receive process is fully synchronous, HTTP is synchronous on receive tied to page read, MQTT is fully asynchronous 
 *  with msg event receive signaling and data transfer taking place async behind the scenes initiated on interrupt. 
 */
typedef struct iop_tag
{
    volatile char* txSrc;                   // source pointer to TX pending data
    volatile uint16_t txPending;            // outstanding char count for TX
    volatile bool dmActive;                 // interaction with BGx is now in data mode
    volatile uint16_t dmTxEvents;           // number of TX blocks sent during data mode
    volatile bool isrEnabled;               // flag to signal ISR to run normally (true), or return immediately

    uint8_t irqAttached;
    bBuffer_t *rxBffr;                      // receive buffer
    char txEot;                             // if not NULL, char to output on empty TX FIFO; clears automatically on use.
 
    volatile uint32_t isrInvokeCnt;         // number of times the ISR function has been invoked
    volatile uint32_t lastTxAt;             // tick count when TX send started, used for response timeout detection
    volatile uint32_t lastRxAt;             // tick count when RX buffer fill level was known to have change
} iop_t;


typedef struct dataMode_tag                                         // the command processor's automatic transition to deliver a block of data inside a command
{
    dmState_t dmState;                                              // current state of the data mode transition
    uint16_t contextKey;                                            // unique identifier for data flow, could be dataContext(proto), handle(files), etc.
    char trigger[atcmd__dataModeTriggerSz];                         // char sequence that signals the transition to data mode, data mode starts at the following character
    dataRxHndlr_func dataHndlr;                                     // data handler function (TX/RX)
    char* txDataLoc;                                                // location of data buffer (TX only)
    uint16_t txDataSz;                                              // size of TX data or RX request
    bool runParserAfterDataMode;                                    // true = invoke AT response parser after successful datamode. Data mode error always skips parser
    appRcvProto_func applRecvDataCB;                                // callback into app for received data delivery
} dataMode_t;

/**
 * @brief Set parameters for standard AT-Cmd response parser
 */
typedef struct atcmdParserConfig_tag 
{
    char *preamble;
    bool preambleReqd;
    char *delimeters;
    uint8_t tokensReqd;
    char *finale;
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
    uint32_t invokedAt;                                             // Tick value at the command invocation, used for timeout detection.
    
    char rawResponse[PSZ(atcmd__respBufferSz)];                     // response buffer, allows for post cmd execution review of received text (0-filled).
    char respToken[PSZ(atcmd__respTokenSz)];                        // buffer to hold a token string grabbed from response
    cmdParseRslt_t parserResult;                                    // last parser invoke result returned
    bool preambleFound;                                             // true if parser found preamble
    char errorDetail[PSZ(ltem__errorDetailSz)];                     // BGx error code returned, could be CME ERROR (< 100) or subsystem error (generally > 500)

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


// /** 
//  *  \brief Result structure returned from a action request (await or get).
// */
// typedef struct atcmdResult_tag
// {
//     resultCode_t statusCode;                                        // The HTML style status code, indicates the sucess or failure (type) for the command's invocation.
//     char *response;                                                 // The char c-string containing the full response from the BGx.
//     uint16_t responseCode;                                          // Numeric response value from many "status" action parsers (suffixed with _rc)
// } atcmdResult_t;



/** 
 *  @brief Return struct for get TLC information
*/
typedef struct tlsOptions_tag
{
    tlsVersion_t version;
    tlsCipher_t cipher;
    tlsCertExpiration_t certExpCheck;
    tlsSecurityLevel_t securityLevel;
    char trCertPath[80];
} tlsOptions_t;




/**
 * @brief Adds a protocol stream to the LTEm streams table
 * @details ASSERTS that no stream is occupying the stream control's data context
 * 
 * @param streamCtrl The stream to add to the LTEm stream table
 */
void ltem_addStream(streamCtrl_t *streamCtrl);


/**
 * @brief Remove a stream from the LTEm streams table, excludes it from further processing
 * @details ASSERTS that the stream parameter matches the stream in the LTEm table
 * 
 * @param streamCtrl The stream to remove from the LTEm stream table
 */
void ltem_deleteStream(streamCtrl_t *streamCtrl);


/**
 * @brief Get a stream control from data context, optionally filtering on stream type.
 * 
 * @param context The data context for the stream 
 * @param streamType Protocol of the stream
 * @return streamCtrl_t* Pointer of a generic stream, can be cast (after type validation) to a specific protocol control
 */
streamCtrl_t* ltem_getStreamFromCntxt(uint8_t context, streamType_t streamType);














/* Metric Type Definitions
 * ------------------------------------------------------------------------------------------------------------------------------*/
typedef struct ltemMetrics_tag
{
    // metrics
    uint32_t cmdInvokeCnt;
    uint32_t isrInvokeCnt;
    uint32_t isrReadCnt;
    uint32_t isrWriteCnt;
} ltemMetrics_t;


/**
 * @brief enum describing the last receive event serviced by the ISR
 */
typedef enum recvEvent_tag
{
    recvEvent_none = 0,
    recvEvent_data,
    recvEvent_timeout
} recvEvent_t;


typedef struct fileCtrl_tag
{
    char streamType;                            // stream type
    /*
     * NOTE: Does NOT follow exact struct field layout of the other streams, shares 1st field to validate type before casting 
     */
    uint8_t handle;
    dataRxHndlr_func dataRxHndlr;               // function to handle data streaming, initiated by atcmd dataMode (RX only)
    appRcvProto_func appRecvDataCB;
} fileCtrl_t;

/**
 * @brief Static char arrays to simplify passing string responses back to user application.
 */
typedef struct ltemStatics_tag
{
    char dateTimeBffr[PSZ(ltem__dateTimeBffrSz)];   // reused by date/time functions for parsing formats
    char reportBffr[PSZ(ltem__reportsBffrSz)];      // reused by *Rpt() functions
} ltemStatics_t;



/** 
 *  \brief Struct representing the LTEmC model. The struct behind the g_ltem1 global variable with all driver controls.
 * 
 *  Most subsystems are linked to this struct with pointers to allow for better abstraction and optional subsystems
 */
typedef struct ltemDevice_tag
{
	ltemPinConfig_t pinConfig;                          // GPIO pin configuration for required GPIO and SPI interfacing
    bool cancellationRequest;                           // For RTOS implementations, token to request cancellation of long running task/action
    bool hostConfigured;                                // Host resources configured for LTEm use
    deviceState_t deviceState;                          // Device state of the BGx module
    bool appEventNotifyEnabled;                         // Flag to control forwarding of LTEm notifications back to application
    appEvntNotify_func appEvntNotifyCB;                 // Event notification callback to parent application
    appDiagCallback_func appDiagnosticCB;               // Callback to application (platform specific) diagnostics function (stack, memory or other system state)

    platformSpi_t* platformSpi;
    iop_t *iop;                                         // IOP subsystem controls
    atcmd_t *atcmd;                                     // Action subsystem controls
    modemSettings_t *modemSettings;                     // Settings to control radio and cellular network initialization
	modemInfo_t *modemInfo;                             // Data structure holding persistent information about modem device
    ntwkOperator_t *ntwkOperator;                       // Data structure representing the cellular network provider and the networks (PDP contexts it provides)
    streamCtrl_t* streams[ltem__streamCnt];             // Data streams: protocols or file system
    urcEvntHndlr_func urcEvntHndlrs[ltem__streamCnt];   // function to determine if "potential" URC event is for an open stream and perform reqd actions
    fileCtrl_t* fileCtrl;

    ltemMetrics_t metrics;                              // metrics for operational analysis and reporting
    ltemStatics_t statics;
} ltemDevice_t;


/* LTEmC Global Singleton Instance
------------------------------------------------------------------------------------------------- */
extern ltemDevice_t g_lqLTEM;


/* ================================================================================================================================
 * LTEmC Global Structure */

extern ltemDevice_t g_lqLTEM;                   // The LTEm "object".

/* LTEmC device is created as a global singleton variable
 * ==============================================================================================================================*/




#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


// LTEM Internal
// void LTEM_initIo();
// void LTEM_registerDoWorker(doWork_func *doWorker);
// void LTEM_registerUrcHandler(urcHandler_func *urcHandler);

/* LTEmC internal, not intended for user application consumption.
 * --------------------------------------------------------------------------------------------- */
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
 * @brief Returns the atCmd parser result code, 0xFFFF or cmdParseRslt_pending if command is pending completion
 * @return The PARSER result from the last interation of the parser execution. This is generally not applicable to end-user applications.
 */
cmdParseRslt_t atcmd_getParserResult();


/* Response Parsers
 * --------------------------------------------------------------------------------------------- */

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
cmdParseRslt_t atcmd_stdResponseParser(const char *preamble, bool preambleReqd, const char *delimiters, uint8_t tokensReqd, uint8_t valueIndx, const char *finale, uint16_t lengthReqd);


// /**
//  * @brief LTEmC internal testing parser to capture incoming response until timeout. This is generally not used by end-user applications.
//  * @return Parse status result (always pending for this test parser)
//  */
// cmdParseRslt_t ATCMD_testResponseTrace();


/**
 * @brief Stardard TX (out) data handler used by dataMode. Sends data and checks for OK response.
 */
resultCode_t atcmd_stdTxDataHndlr();


/**
 * @brief TX (out) data handler that performs a blind send of data.
 */
resultCode_t atcmd_txDataHndlrRaw();


/**
 * @brief Stardard RX (in) data handler used by dataMode.
 */
resultCode_t atcmd_stdRxDataHndlr();



// /**
//  *	\brief Checks recv buffer for command response and sets atcmd structure data with result.
//  */
// resultCode_t ATCMD_readResult();

/**
 *  \brief Default AT command result parser.
*/
cmdParseRslt_t ATCMD_okResponseParser();

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

/* LTEmC INTERNAL prompt parsers 
 * ------------------------------------------------------------------------- */

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
/* ------------------------------------------------------------------------------------------------
 * End ATCMD LTEmC Internal Functions */


#pragma region NTWK LTEmC Internal Functions
/* LTEmC internal, not intended for user application consumption.
 * --------------------------------------------------------------------------------------------- */


/**
 *	\brief Initialize BGx Radio Access Technology (RAT) options.
 */
void NTWK_initRatOptions();


/**
 *	\brief Apply the default PDP context config to BGx.
 */
void NTWK_applyDefaulNetwork();



/** 
 *  @brief Configure the TLS/SSL settings for a context
 *  @details The TLS/SSL context is loosely associated with the protocol context. LTEmC maintains a 1-to-1 map for consistency.
 *  @param [in] contxt TLS/SSL context to configure
 *  @return TLS options structure with the settings currently applied to the specified context
 */
tlsOptions_t tlsGetOptions(uint8_t sckt);






#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  // !__LTEMC_ITYPES_H__
