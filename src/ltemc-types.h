/** ****************************************************************************
  \file 
  \brief Public API type definitions for consumption of LTEmC services.
  \author Greg Terrell, LooUQ Incorporated

  \loouq

--------------------------------------------------------------------------------

    This project is released under the GPL-3.0 License.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 
***************************************************************************** */


#ifndef __LTEMC_TYPES_H__
#define __LTEMC_TYPES_H__

#ifdef __cplusplus
#include <cstdint>
#include <cstdlib>
#include <cstdbool>
#else
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#endif // __cplusplus

#include <lq-types.h>
#include <lq-cbuffer.h>

enum ltem__constants
{
    ltem__bufferSz_rx = 2000,
    ltem__bufferSz_tx = 1000,

    ltem__swVerSz = 12,
    ltem__errorDetailSz = 18,
    ltem__moduleTypeSz = 8,

    ltem__streamCnt = 4,            /// 6 SSL/TLS capable data contexts + file system allowable, 4 concurrent seams reasonable
    //ltem__urcHandlersCnt = 4        /// max number of concurrent protocol URC handlers (today only http, mqtt, sockets, filesystem)
};


/** 
 *  @brief Typed numeric constants for stream peers subsystem (sockets, mqtt, http)
 */
enum streams__constants
{
    streams__ctrlMagic = 0x186F,
    streams__maxContextProtocols = 5,
    streams__typeCodeSz = 4,
    streams__urcPrefixesSz = 60,
    host__urlSz = 100
};


typedef struct ltemPinConfig_tag
{
    int spiCsPin;
    int irqPin;
    int statusPin;
    int powerkeyPin;
    int resetPin;
    int ringUrcPin;
    int connected;
    int wakePin;
} ltemPinConfig_t;


/** 
 *  \brief Enum describing the current device/module state
 */
typedef enum deviceState_tag
{
    deviceState_powerOff = 0,        /// BGx is powered off, in this state all components on the LTEm1 are powered down.
    deviceState_powerOn = 1,         /// BGx is powered ON, while powered on the BGx may not be able to interact fully with the host application.
    deviceState_appReady = 2         /// BGx is powered ON and ready for application/services.
} deviceState_t;


/** 
 *  @brief Enum of the available dataCntxt indexes for BGx (only SSL/TLS capable contexts are supported).
 */
typedef enum dataCntxt_tag
{
    dataCntxt_0 = 0,
    dataCntxt_1 = 1,
    dataCntxt_2 = 2,
    dataCntxt_3 = 3,
    dataCntxt_4 = 4,
    dataCntxt_5 = 5,
    dataCntxt__cnt = 6,
    dataCntxt__none = 255
} dataCntxt_t;


/* ================================================================================================================================
 * LTEmC module definitions
 * ==============================================================================================================================*/


/* Module Independent Function Definitions 
 * ------------------------------------------------------------------------------------------------------------------------------*/

typedef void (*doWork_func)();                                           // module background worker
typedef void (*powerSaveCallback_func)(uint8_t newPowerSaveState);


/* Modem/Provider/Network Type Definitions
 * ------------------------------------------------------------------------------------------------------------------------------*/

#define NTWK_PROVIDER_RAT_AUTO "00"     /// M1 (eMTC) >> NB-IoT >> GSM
#define NTWK_PROVIDER_RAT_GSM "01"
#define NTWK_PROVIDER_RAT_M1 "02"
#define NTWK_PROVIDER_RAT_NB "03"


/** 
 *  \brief Enum describing the mode the BGx module is using to look for available networks (carriers).
*/
typedef enum ntwkScanMode_tag
{
    ntwkScanMode_auto = 0U,             /// BGx is considering either GSM or LTE carrier connections.
    ntwkScanMode_gsmonly = 1U,          /// GSM only mode: BGx is filtering visible networks and only considering connections to GSM endpoints.
    ntwkScanMode_lteonly = 3U           /// LTE only mode: BGx is filtering visible networks and only considering connections to LTE endpoints.
} ntwkScanMode_t;


/** 
 *  \brief Enum describing the available options for an IoT protocol when connecting to the network.
*/
typedef enum ntwkIotMode_tag
{
    ntwkIotMode_m1 = 0U,                /// CAT-M1 only mode: BGx is filtering visible networks and only considering CAT-M1 connections.
    ntwkIotMode_nb1 = 1U,               /// NB-IOT only mode: BGx is filtering visible networks and only considering NB-IOT connections.
    ntwkIotMode_m1nb1 = 2U              /// The BGx will connect to either a CAT-M1 or NB-IOT network.
} ntwkIotMode_t;


/** 
 *  \brief Typed numeric constants for network subsystem.
*/
enum ntwk
{
    ntwk__pdpContextCnt = 4,            // varies by carrier: Verizon=2, (Aeris)ATT=3
    ntwk__providerNameSz = 20,
    ntwk__iotModeNameSz = 11,
    ntwk__pdpProtoSz = 7,
    ntwk__ipAddressSz = 40,
    ntwk__ntwkConfigSz = 60,

    ntwk__imeiSz = 15,
    ntwk__iccidSz = 20,

    ntwk__dvcFwVerSz = 40,
    ntwk__dvcMfgSz = 40
};


#define PDP_PROTOCOL_IPV4 "IP"
#define PDP_PROTOCOL_IPV6 "IPV6"
#define PDP_PROTOCOL_IPV4V6 "IPV4V6"
#define PDP_PROTOCOL_PPP "PPP"

// /** 
//  *  \brief Enum of the two available PDP contexts for BGx provided by network carriers.
// */
// typedef enum pdpProtocolType_tag
// {
//     pdpProtocolType_IPV4 = 1,      /// IP v4, 32-bit address (ex: 192.168.37.52)
//     pdpProtocolType_IPV6 = 2,      /// IP v6, 128-bit address (ex: 2001:0db8:0000:0000:0000:8a2e:0370:7334)
//     pdpProtocolType_IPV4V6 = 3,
//     pdpProtocolType_PPP = 9
// } pdpProtocolType_t;


typedef enum pdpCntxtAuthMethods_tag
{
    pdpCntxtAuthMethods_none = 0,
    pdpCntxtAuthMethods_pap = 1,
    pdpCntxtAuthMethods_chap = 2,
    pdpCntxtAuthMethods_papChap = 3
} pdpCntxtAuthMethods_t;


/** 
 *  \brief Struct holding cellular and radio settings.
*/
typedef struct modemSettings_tag
{
    char scanSequence[12];
    ntwkScanMode_t scanMode;
    ntwkIotMode_t iotMode;
    char defaultNtwkConfig[ntwk__ntwkConfigSz]; /// Invoke ready default context config
} modemSettings_t;


/** 
 *  \brief Struct holding information about the physical BGx module.
*/
typedef struct modemInfo_tag
{
	char imei[ntwk__imeiSz + 1];            /// IMEI (15 digits) International Mobile Equipment Identity or IEEE UI (aka MAC, EUI-48 or EUI-64).
	char iccid[ntwk__iccidSz + 1];          /// ICCID (20 digits) Integrated Circuit Card ID. Set in the SIM card at manufacture.
	char mfgmodel[ntwk__dvcMfgSz + 1];      /// The device model number
	char fwver[ntwk__dvcFwVerSz + 1];       /// Firmware version of the device
    char swver[ltem__swVerSz + 1];          /// software driver version
} modemInfo_t;


/** 
 *  \brief Struct representing the state of active PDP contexts (aka: APN or data context).
*/
typedef struct networkInfo_tag
{
    bool isActive;
    uint8_t pdpContextId;                           /// context ID recognized by the carrier (valid are 1 to 16)
    char pdpProtocolType[ntwk__pdpProtoSz];         /// IPv4, IPv6, PPP
	char ipAddress[ntwk__ipAddressSz];              /// The IP address obtained from the carrier for this context. The IP address of the modem.
} networkInfo_t;


/** 
 *  \brief Struct respresenting an ACTIVE network carrier/operator.
*/
typedef struct providerInfo_tag
{
	char name[ntwk__providerNameSz];                /// Provider name, some carriers may report as 6-digit numeric carrier ID.
	char iotMode[ntwk__iotModeNameSz];              /// Network carrier protocol mode: CATM-1 or NB-IOT for BGx.
    uint8_t defaultContext;
    uint8_t networkCnt;                             /// The number of networks in networks[]
    networkInfo_t networks[ntwk__pdpContextCnt];    /// Collection of contexts with network carrier. This is typically only 1, but some carriers implement more (ex VZW).
} providerInfo_t;




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
 *  @brief Streams 
 *  @details Structures for stream control/processing
 * ================================================================================================
 */

typedef enum streamType_tag
{
    streamType_UDP = 'U',
    streamType_TCP = 'T',
    streamType_SSLTLS = 'S',
    streamType_MQTT = 'M',
    streamType_HTTP = 'H',
    streamType_file = 'F',
    streamType__ANY = 0,
    streamType__SCKT = 'k'
} streamType_t;


// function prototypes
typedef resultCode_t (*urcEvntHndlr_func)();        // data comes from rxBuffer, this function parses and forwards to application via appRcvProto_func
typedef resultCode_t (*dataRxHndlr_func)();         // data comes from rxBuffer, this function parses and forwards to application via appRcvProto_func
typedef void (*appRcvProto_func)();                 // prototype func() for stream recvData callback


typedef struct streamCtrl_tag
{
    char streamType;                                /// stream type
    dataCntxt_t dataCntxt;                          /// integer representing the source of the stream; fixed for protocols, file handle for FS
    dataRxHndlr_func dataRxHndlr;                   /// function to handle data streaming, initiated by eventMgr() or atcmd module
    urcEvntHndlr_func urcHndlr;                     /// function to handle data streaming, initiated by eventMgr() or atcmd module
} streamCtrl_t;


/*
 * ============================================================================================= */

/** 
 *  \brief Struct for the IOP subsystem state. During initialization a pointer to this structure is reference in g_ltem1.
 * 
 *  Each of the protocols (sockets, MQTT, HTTP) have unique behaviors: sockets receive asynchronous alerts but the data
 *  receive process is fully synchronous, HTTP is synchronous on receive tied to page read, MQTT is fully asynchronous 
 *  with msg event receive signaling and data transfer taking place async behind the scenes initiated on interrupt. 
 */
typedef struct iop_tag
{
    volatile char* txBffr;
    volatile uint16_t txPending;

    cbuffer_t *rxBffr;                      /// receive buffer
    char txEot;                             /// if not NULL, char to output on empty TX FIFO; clears automatically on use.
 
    volatile uint32_t lastTxAt;             /// tick count when TX send started, used for response timeout detection
    volatile uint32_t lastRxAt;             /// tick count when RX buffer fill level was known to have change
} iop_t;


/* ATCMD Module Type Definitions
 * ------------------------------------------------------------------------------------------------------------------------------*/

/** 
 *  \brief Typed constants for AT-CMD module.
*/
enum atcmd__constants
{
    atcmd__noTimeoutChange = 0,
    atcmd__defaultTimeout = 800,

    atcmd__setLockModeManual = 0,
    atcmd__setLockModeAuto = 1,

    atcmd__cmdBufferSz = 448,                       // prev=120, mqtt(Azure) connect=384, new=512 for universal cmd coverage, data mode to us dynamic TX bffr switching
    atcmd__respBufferSz = 120,
    atcmd__streamPrefixSz = 12,                     // obsolete with universal data mode switch
    atcmd__dataModeTriggerSz = 13
};


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

typedef struct dataMode_tag
{
    uint16_t contextKey;                                /// unique identifier for data flow, could be dataContext(proto), handle(files), etc.
    char trigger[atcmd__dataModeTriggerSz];             /// char sequence that signals the transition to data mode, data mode starts at the following character
    dataRxHndlr_func dataHndlr;                         /// data handler function (TX/RX)
    char* txDataLoc;                                    /// location of data buffer (TX only)
    uint16_t txDataSz;                                  /// size of TX data or RX request
    bool skipParser;                                    /// true = no invoke of response parser after successul datamode, error always skips parser
    appRcvProto_func applRecvDataCB;                    /// callback into app for received data delivery
} dataMode_t;


typedef cmdParseRslt_t (*cmdResponseParser_func)();                             // AT response parser template


/** 
 *  \brief Structure to control invocation and management of an AT command with the BGx module.
*/
typedef struct atcmd_tag
{
    char cmdStr[atcmd__cmdBufferSz];                    /// AT command string to be passed to the BGx module.

    // temporary                                        /// waiting on fix to SPI TX overright
    char CMDMIRROR[atcmd__cmdBufferSz];

    uint32_t timeout;                                   /// Timout in milliseconds for the command, defaults to 300mS. BGx documentation indicates cmds with longer timeout.
    bool isOpenLocked;                                  /// True if the command is still open, AT commands are single threaded and this blocks a new cmd initiation.
    bool autoLock;                                      /// last invoke was auto and should be closed automatically on complete
    uint32_t invokedAt;                                 /// Tick value at the command invocation, used for timeout detection.
    
    char rawResponse[atcmd__respBufferSz + 1];          /// response buffer, allows for post cmd execution review of received text (0-filled).
    char* response;                                     /// PTR variable section of response.

    uint32_t execDuration;                              /// duration of command's execution in milliseconds
    resultCode_t resultCode;                            /// consumer API result value (HTTP style), success=200, timeout=408, single digit BG errors are expected to be offset by 1000
    cmdResponseParser_func responseParserFunc;          /// parser function to analyze AT cmd response and optionally extract value
    cmdParseRslt_t parserResult;                        /// last parser invoke result returned
    bool preambleFound;                                 /// true if parser found preamble
    char errorDetail[SET_PROPLEN(ltem__errorDetailSz)]; /// BGx error code returned, could be CME ERROR (< 100) or subsystem error (generally > 500)
    int32_t retValue;                                   /// optional signed int value extracted from response

    // streamCtrl_t* streamCtrl;
    // char streamPrefix[PROPLEN(atcmd__streamPrefixSz)];  /// char sequence prefixing steam data, triggers switch to registered data handler streamRxHndlr()
    // appRcvProto_func applDataCB;

    dataMode_t dataMode;                                /// controls for automatic data mode servicing - both TX (out) and RX (in). Std functions or extensions supported.
} atcmd_t;


/** 
 *  \brief Result structure returned from a action request (await or get).
*/
typedef struct atcmdResult_tag
{
    resultCode_t statusCode;                    /// The HTML style status code, indicates the sucess or failure (type) for the command's invocation.
    char *response;                             /// The char c-string containing the full response from the BGx.
    uint16_t responseCode;                      /// Numeric response value from many "status" action parsers (suffixed with _rc)
} atcmdResult_t;


/* SSL/TLS Module Type Definitions
 * ------------------------------------------------------------------------------------------------------------------------------*/

/** 
 *  @brief Enum of available SSL version options for an SSL connection
*/
typedef enum tlsVersion_tag
{
    tlsVersion_none = 255,
    tlsVersion_ssl30 = 0,
    tlsVersion_tls10 = 1,
    tlsVersion_tls11 = 2,
    tlsVersion_tls12 = 3,
    tlsVersion_any = 4,                                 // BGx default

    tlsVersion_default = 4
} tlsVersion_t;


/** 
 *  @brief Enum of the available cipher suites for TLS processing
*/
typedef enum tlsCipher_tag
{
    tlsCipher_rsaAes256CbcSha = 0X0035,
    tlsCipher_rsaAes128CbcSha = 0X002F,
    tlsCipher_rsaRc4128Sha = 0x0005,
    tlsCipher_rsaRc4128Md5 = 0x0004,
    tlsCipher_rsa3desEdeCbcSha = 0x000A,
    tlsCipher_rsaAes256CbcSha256 = 0x003D,

    tlsCipher_ecdheRsaRc4128Sha = 0xC011,
    tlsCipher_ecdheRsa3desEdeCbcSha = 0xC012,
    tlsCipher_ecdheRsaAes128CbcSha = 0xC013,
    tlsCipher_ecdheRsaAes256CbcSha = 0xC014,
    tlsCipher_ecdheRsaAes128CbcSha256 = 0xC027,
    tlsCipher_ecdheRsaAes256CbcSha384 = 0xC028,
    tlsCipher_ecdheRsaAes128GcmSha256 = 0xC02F,

    tlsCipher_any = 0xFFFF,                             // BGx default

    tlsCipher_default = 0xFFFF
} tlsCipher_t;


/** 
 *  @brief Enum of the options for certificate expiration date/time checking
*/
typedef enum tlsCertExpiration_tag
{
    tlsCertExpiration_check = 0,
    tlsCertExpiration_ignore = 1,                 // BGx default

    tlsCertExpiration_default = 1
} tlsCertExpiration_t;


/** 
 *  @brief Enum of the certification validation options
*/
typedef enum tlsSecurityLevel_tag
{
    tlsSecurityLevel_noAuthentication = 0,              // BGx default
    tlsSecurityLevel_serverAuthentication = 1,
    tlsSecurityLevel_serverClientAuthentication = 2,

    tlsSecurityLevel_default = 0
} tlsSecurityLevel_t;


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


#endif  // !__LTEMC_TYPES_H__
