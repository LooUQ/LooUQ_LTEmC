/** ***************************************************************************
  @file 
  @brief LTEm public type declarations.

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


#ifndef __LTEMC_TYPES_H__
#define __LTEMC_TYPES_H__

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
#include <lq-bBuffer.h>


/**
 * @brief Global LTEmC constants 
 */
enum ltem__constants
{
    ltem__bufferSz_rx = 2000,
    ltem__bufferSz_tx = 1000,

    ltem__swVerSz = 12,
    ltem__moduleTypeSz = 8,
    ltem__streamCnt = 6,                                            ///< 6 SSL/TLS capable data contexts, (only MQTT and SCKT (sockets) have async behaviors)
    ltem__reportsBffrSz = 80,
    ltem__dateTimeBffrSz = 24
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
    streams__lengthWaitDuration = 10,
    streams__contentLengthTrailerSz = 6,
    streams__dataModeMaxPreambleSz = 20,
    host__urlSz = 192
};


/**
 * @brief Pin map for communications between the host MCU and the LTEm device.
 */
typedef struct ltemPinConfig_tag
{
    int spiIndx;                                            ///< The SPI resource index number (SPI1, SPI2, etc.)
    int spiCsPin;                                           ///< SPI chip-select (CS/SS) pin
    int spiClkPin;                                          ///< SPI clock pin
    int spiMisoPin;                                         ///< SPI MISO pin
    int spiMosiPin;                                         ///< SPI MOSI pin
    int irqPin;                                             ///< LTEm interrupt request pin
    int statusPin;                                          ///< LTEm status pin (active high)
    int powerkeyPin;                                        ///< LTEm power toggle pin (pulsed high)
    int resetPin;                                           ///< LTEm reset pin (pulsed high)
    int ringUrcPin;                                         ///< LTEm event pin (signals new URC arrived, NOT used by LTEmC)
    int connected;                                          ///< LTEm connected pin (NOT currently used by LTEmC)
    int wakePin;                                            ///< LTEm wake from power save pin
} ltemPinConfig_t;


/** 
 *  @brief Enum describing the current device/module state
 */
typedef enum deviceState_tag
{
    deviceState_powerOff = 0,                               ///< BGx is powered off, in this state all components on the LTEm1 are powered down.
    deviceState_powerOn = 1,                                ///< BGx is powered ON, while powered on the BGx may not be able to interact fully with the host application.
    deviceState_ready = 2,                                  ///< BGx is powered ON and ready for application/services.
    deviceState_error = 99                                  ///< BGx is in an unknown or invalid state.
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

typedef void (*doWork_func)();                                      ///< software module background worker
typedef void (*powerSaveCallback_func)(uint8_t newPowerSaveState);  ///< power save callback worker


/* Modem/Provider/Network Type Definitions
 * Char values below can be concatenated for a RAT custom search pattern
 * ------------------------------------------------------------------------------------------------------------------------------*/

#define NTWK_PROVIDER_RAT_AUTO "00"                                 ///< (default)  M1 (eMTC) >> NB-IoT >> GSM
#define NTWK_PROVIDER_RAT_GSM "01"                                  ///< GSM Only 
#define NTWK_PROVIDER_RAT_M1 "02"                                   ///< M1 (eMTC) Only
#define NTWK_PROVIDER_RAT_NB "03"                                   ///< NB-IoT Only


/** 
 *  @brief Enum describing the mode the BGx module is using to look for available networks (carriers).
*/
typedef enum ntwkScanMode_tag
{
    ntwkScanMode_auto = 0U,                                         ///< BGx is considering either GSM or LTE carrier connections.
    ntwkScanMode_gsmonly = 1U,                                      ///< GSM only mode: BGx is filtering visible networks and only considering connections to GSM endpoints.
    ntwkScanMode_lteonly = 3U                                       ///< LTE only mode: BGx is filtering visible networks and only considering connections to LTE endpoints.
} ntwkScanMode_t;


/** 
 *  @brief Enum describing the available options for an IoT protocol when connecting to the network.
*/
typedef enum ntwkIotMode_tag
{
    ntwkIotMode_M1 = 0U,                // CAT-M1 only mode: BGx is filtering visible networks and only considering CAT-M1 connections.
    ntwkIotMode_NB = 1U,                // NB-IOT only mode: BGx is filtering visible networks and only considering NB-IOT connections.
    ntwkIotMode_M1NB = 2U               // The BGx will connect to either a CAT-M1 or NB-IOT network.
} ntwkIotMode_t;


/** 
 *  @brief Typed numeric constants for network subsystem.
*/
enum ntwk
{
    ntwk__pdpContextCnt = 4,            // varies by carrier: Verizon=2, (Aeris)ATT=3
    ntwk__operatorNameSz = 20,
    ntwk__iotModeNameSz = 11,
    ntwk__pdpProtoSz = 7,
    ntwk__ipAddressSz = 40,
    ntwk__pdpNtwkConfigSz = 60,

    ntwk__scanSeqSz = 12,
    ntwk__imeiSz = 15,
    ntwk__iccidSz = 24,

    ntwk__dvcMfgSz = 18,
    ntwk__dvcModelSz = 18,
    ntwk__dvcFwVerSz = 20,
};


/**
 * @brief The packet protocol for a PDP context
 */
typedef enum pdpProtocol_tag
{
    pdpProtocol_notSet = 0,
    pdpProtocol_IPV4 = 1,
    pdpProtocol_IPV6 = 2,
    pdpProtocol_IPV4V6 = 3,
    pdpProtocol_PPP = 99                    // not supported by LTEmC
} pdpProtocol_t;

/* BGx representation
#define PDP_PROTOCOL_IPV4 "IP"
#define PDP_PROTOCOL_IPV6 "IPV6"
#define PDP_PROTOCOL_IPV4V6 "IPV4V6"
#define PDP_PROTOCOL_PPP "PPP"                                      ///< NOT SUPPORTED by LTEmC
*/


/**
 * @brief PDP context authentication method
 * @details Never seen in the wild
 */
typedef enum pdpCntxtAuthMethods_tag
{
    pdpCntxtAuthMethods_none = 0,
    pdpCntxtAuthMethods_pap = 1,
    pdpCntxtAuthMethods_chap = 2,
    pdpCntxtAuthMethods_papChap = 3
} pdpCntxtAuthMethods_t;


/** 
 *  @brief RF Priority map for BG95/BG77 modules.
*/
typedef enum ltemRfPriorityMode_tag
{
    ltemRfPriorityMode_gnss = 0,
    ltemRfPriorityMode_wwan = 1,
    ltemRfPriorityMode_none = 9
} ltemRfPriorityMode_t;


/** 
 *  @brief RF Priority map for BG95/BG77 modules.
*/
typedef enum ltemRfPriorityState_tag
{
ltemRfPriorityState_unloaded = 0,                                   ///< WWAN/GNSS in unloaded state
ltemRfPriorityState_wwanPending = 1,                                ///< WWAN in pending state
ltemRfPriorityState_gnssPending = 2,                                ///< GNSS in pending state
ltemRfPriorityState_wwanLoaded = 3,                                 ///< WWAN in loaded state
ltemRfPriorityState_gnssLoaded = 4                                  ///< GNSS in loaded state
} ltemRfPriorityState_t;


/** 
 *  @brief Struct holding cellular and radio settings.
*/
typedef struct modemSettings_tag
{
    uint8_t pdpContextId;                                           ///< Default PDP context number
    char scanSequence[PSZ(ntwk__scanSeqSz)];                        ///< RF scan sequence for locating target network
    ntwkScanMode_t scanMode;                                        ///< RF scan mode
    ntwkIotMode_t iotMode;                                          ///< IoT sub-mode for locating packet context
    char pdpNtwkConfig[ntwk__pdpNtwkConfigSz];                      ///< Invoke ready default context config
} modemSettings_t;


/** 
 *  @brief Struct holding information about the physical BGx module.
*/
typedef struct modemInfo_tag
{
	char imei[PSZ(ntwk__imeiSz)];                           ///< IMEI (15 digits) International Mobile Equipment Identity or IEEE UI (aka MAC, EUI-48 or EUI-64).
	char iccid[PSZ(ntwk__iccidSz)];                         ///< ICCID (up to 24 digits) Integrated Circuit Card ID. Set in the SIM card at manufacture.
    char mfg[PSZ(ntwk__dvcMfgSz)];                          ///< Device manufacturer name
	char model[PSZ(ntwk__dvcModelSz)];                      ///< Device model number
	char fwver[PSZ(ntwk__dvcFwVerSz)];                      ///< Firmware version of the device
    char swver[PSZ(ltem__swVerSz)];                         ///< software driver version
} modemInfo_t;


/** 
 *  @brief Struct representing the state of active PDP contexts (aka: APN or data context).
*/
typedef struct packetNetwork_tag
{
    uint8_t pdpContextId;                                   ///< context ID recognized by the carrier (valid are 1 to 16)
    pdpProtocol_t pdpProtocol;                              ///< IPv4, IPv6, etc.
    char protoName[ntwk__pdpProtoSz];                       ///< text for protocol
	char ipAddress[ntwk__ipAddressSz];                      ///< The IP address obtained from the carrier for this context. The IP address of the modem.
} packetNetwork_t;


/** 
 *  @brief Struct respresenting an ACTIVE network carrier/operator.
*/
typedef struct ntwkOperator_tag
{
	char name[PSZ(ntwk__operatorNameSz)];                   ///< Provider name, some carriers may report as 6-digit numeric carrier ID.
	char iotMode[PSZ(ntwk__iotModeNameSz)];                 ///< Network carrier protocol mode: CATM-1 or NB-IOT for BGx.
    uint8_t defaultContext;                                 ///< Default PDP context for data
    uint8_t pdpCntxtCnt;                                    ///< The number of PDP contexts available
    packetNetwork_t packetNetworks[ntwk__pdpContextCnt];    ///< Collection of packet network with cell operator. This is typically only 1, but some carriers implement more (ex VZW).
} ntwkOperator_t;




/* IOP Module Type Definitions
 * ------------------------------------------------------------------------------------------------------------------------------*/

/**
 *	@brief Typed numeric Input/Output Processor subsystem contants.
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
typedef resultCode_t (*dataHndlr_func)(void * ctrl);                                        ///< Function to marshall data between module and LTEmC
typedef void (*appRcvr_func)(uint16_t streamId, const char *fileData, uint16_t dataSz);     ///< Generic app receive callback with simple context/data/dataSize

typedef resultCode_t (*urcEvntHndlr_func)();                                                ///< URC detection and processing, invoked by event manager
typedef void (*closeStream_func)(uint8_t streamId);                                         ///< Stream close processing (if applicable)


/**
 * @brief Generic stream control base, all of the protocol streams start with the struct below.
 * 
 */
typedef struct streamCtrl_tag
{
    char streamType;                                                ///< stream type
    dataCntxt_t dataCntxt;                                          ///< integer representing the source of the stream; fixed for protocols, file handle for FS
    dataHndlr_func dataHndlr;                                       ///< function to handle data streaming, initiated by eventMgr() or atcmd module
    urcEvntHndlr_func urcHndlr;                                     ///< function to handle data streaming, initiated by eventMgr() or atcmd module
    closeStream_func closeStreamCB;                                 ///< function to close stream and update stream control structure (usually invoked after URC detected)
} streamCtrl_t;


/*
 * ============================================================================================= */

/** 
 *  @brief Struct for the IOP subsystem state.
 */
typedef struct iop_tag
{
    volatile char* txSrc;                                           ///< source pointer to TX pending data
    volatile uint16_t txPending;                                    ///< outstanding char count for TX
    volatile bool isrEnabled;                                       ///< flag to signal ISR to run normally (true), or return immediately

    uint8_t irqAttached;                                            ///< Flag indicating the host IRQ has been wired up
    bBuffer_t *rxBffr;                                              ///< receive buffer
    char txEot;                                                     ///< if not NULL, char to output on empty TX FIFO; clears automatically on use.
 
    volatile uint32_t isrInvokeCnt;                                 ///< number of times the ISR function has been invoked
    volatile uint32_t lastTxAt;                                     ///< tick count when TX send started, used for response timeout detection
    volatile uint32_t lastRxAt;                                     ///< tick count when RX buffer fill level was known to have change
} iop_t;


/* ATCMD Module Type Definitions
 * ------------------------------------------------------------------------------------------------------------------------------*/

/** 
 *  @brief Typed constants for AT-CMD module.
*/
enum atcmd__constants
{
    atcmd__noTimeoutChange = 0,
    atcmd__dRdyTimeoutDefault = 2500,                                   ///< milliseconds to wait for exclusive access to atcmd dispatch ready
    // atcmd__defaultTimeout = 800,
    atcmd__dCmpltTimeoutDefault = 1000,                                 ///< milliseconds to wait (default) for atcmd dispatch to complete

    atcmd__setLockModeManual = 0,
    atcmd__setLockModeAuto = 1,

    atcmd__cmdBufferSz = 384,                                           ///< prev=120, mqtt(Azure) connect=384, new=512 for universal cmd coverage, data mode to us dynamic TX bffr switching
    atcmd__respBufferSz = 128,
    atcmd__respTokenSz = 64,

    atcmd__streamPrefixSz = 12,                                         ///< obsolete with universal data mode switch
    atcmd__dataModeTriggerSz = 13,
    atcmd__parserConfigPreambleSz = 24,
    atcmd__parserConfigDelimetersSz = 4,
    atcmd__parserConfigFinaleSz = 16
};


/** 
 *  @brief AT command response parser result codes.
*/
typedef enum cmdParseRslt_tag
{
    cmdParseRslt_pending = 0x00,

    cmdParseRslt_preambleMissing = 0x01,
    cmdParseRslt_countShort = 0x02,
    cmdParseRslt_moduleError = 0x04,
    cmdParseRslt_timeoutError = 0x08,
    //cmdParseRslt_reserved = 0x10,
    //cmdParseRslt_reserved = 0x20,
    cmdParseRslt_generalError = 0x40,

    cmdParseRslt_errorMask = 0x7F,
    cmdParseRslt_complete = 0x80

} cmdParseRslt_t;


/**
 * @brief Data mode state, dataMode is a LTEm internal facility to automatic trigger and complete a data transfer in or out of the module.
 */
typedef enum dmState_tag
{
    dmState_idle = 0,
    dmState_enabled = 1,
    dmState_triggered = 2
} dmState_t;


/**
 * @brief Pattern match configuration for default ATCMD response parser.
 */
typedef struct parserConfig_tag
{
    bool configSet;                                             ///< config for parser is read and applied
    char preamble[PSZ(atcmd__parserConfigPreambleSz)];          ///< preamble phrase the parser is looking for
    bool preambleReqd;                                          ///< True: the above preamble must be found for a successful parse
    char delimiters[PSZ(atcmd__parserConfigDelimetersSz)];      ///< array of possible delimiters in response to parse
    uint8_t tokensReqd;                                         ///< if >0, the number of tokens that are required to be present for a successful parse
    char finale[PSZ(atcmd__parserConfigFinaleSz)];              ///< the finale phrase that bookends a valid response
    uint16_t lengthReqd;                                        ///< if >0, a minimum length for the response
} parserConfig_t;


/**
 * @brief Data mode configuration; DataMode is the automatic switching functionality in a command processing flow
 */
typedef struct dataMode_tag
{
    dmState_t dmState;                                          ///< data mode state
    streamCtrl_t* streamCtrl;                                   ///< optional control pointer for parent process
    char trigger[atcmd__dataModeTriggerSz];                     ///< char sequence that signals the transition to data mode, data mode starts at the following character
    dataHndlr_func dataHndlr;                                   ///< data handler function (TX/RX)
    char* txDataLoc;                                            ///< location of data buffer (TX only)
    uint16_t txDataSz;                                          ///< size of TX data
    uint16_t rxDataSz;                                          ///< size of RX data (read)
    bool runParserAfterDataMode;                                ///< true = invoke AT response parser after successful datamode. Data mode error always skips parser
    appRcvr_func applRcvrCB;                                    ///< callback into app for received data delivery
} dataMode_t;


typedef cmdParseRslt_t (*cmdResponseParser_func)();             ///< AT response parser template


/** 
 *  @brief Structure to control invocation and management of an AT command with the modem module.
*/
typedef struct atcmd_tag
{
    char cmdStr[atcmd__cmdBufferSz];                            ///< AT command string to be passed to the BGx module.

    bool dispatchReady;                                         ///< True: no command is in-flight and blocking next action
    uint32_t dRdyTimeout;                                       ///< Number of MS to wait for exclusive access for dispatch ready
    uint32_t dCmpltTimeout;                                     ///< Timeout in milliseconds for the command, defaults to 300mS. BGx documentation indicates cmds with longer timeout.
    bool isOpenLocked;                                          ///< True if the command is still open, AT commands are single threaded and this blocks a new cmd initiation.
    bool autoLock;                                              ///< last invoke was auto and should be closed automatically on complete
    uint32_t invokedAt;                                         ///< Tick value at the command invocation, used for timeout detection.
    bool eventMgrInvoked;                                       ///< True: eventMgr has been invoked by the current command prep and should not be recursively invoked
    
    cmdResponseParser_func responseParserFunc;                  ///< parser function to analyze AT cmd response and optionally extract value
    parserConfig_t parserConfig;                                ///< Structure containing the pattern the parser is searching for to signal a valid response
    dataMode_t dataMode;                                        ///< controls for automatic data mode servicing - both TX (out) and RX (in). Std functions or extensions supported.

    char rawResponse[PSZ(atcmd__respBufferSz)];                 ///< response buffer, allows for post cmd execution review of received text (0-filled).
    char* response;                                             ///< PTR variable section of response.
    char respToken[PSZ(atcmd__respTokenSz)];                    ///< buffer to hold a token string grabbed from response

    resultCode_t resultCode;                                    ///< consumer API result value (HTTP style), success=200, timeout=408, single digit BG errors are expected to be offset by 1000
    int16_t resultValue;                                        ///< optional result value returned by some AT commands
    cmdParseRslt_t parserResult;                                ///< last parser invoke result returned
    bool preambleFound;                                         ///< true if parser found preamble
    uint32_t execDuration;                                      ///< duration of command's execution in milliseconds
} atcmd_t;



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
    tlsVersion_any = 4,                                             ///< BGx default

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

    tlsCipher_any = 0xFFFF,                                         ///< BGx default

    tlsCipher_default = 0xFFFF
} tlsCipher_t;


/** 
 *  @brief Enum of the options for certificate expiration date/time checking
*/
typedef enum tlsCertExpiration_tag
{
    tlsCertExpiration_check = 0,
    tlsCertExpiration_ignore = 1,                                   ///< BGx default

    tlsCertExpiration_default = 1
} tlsCertExpiration_t;


/** 
 *  @brief Enum of the certification validation options
*/
typedef enum tlsSecurityLevel_tag
{
    tlsSecurityLevel_noAuthentication = 0,                          //< BGx default
    tlsSecurityLevel_serverAuthentication = 1,
    tlsSecurityLevel_serverClientAuthentication = 2,

    tlsSecurityLevel_default = 0
} tlsSecurityLevel_t;


#endif  // !__LTEMC_TYPES_H__
