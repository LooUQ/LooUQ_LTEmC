/******************************************************************************
 *  \file ltemc-types.h
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020-2022 LooUQ Incorporated.
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

#ifndef __LTEMC_TYPES_H__
#define __LTEMC_TYPES_H__


#include <stdint.h>
#include <stdbool.h>
#include <lq-types.h>

enum bufferSizes
{
                                /* can be reduced based on you protocol selections and your data segment sizes */
    bufferSz__txData = 1800,    // size should be equal or greater than length of largest data transmission
    bufferSz__cmdTx = 192,
    bufferSz__coreRx = 192,

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

enum ltem__constants
{
    ltem__streamCnt = 6,
    ltem__swVerSz = 12,
    ltem__errorDetailSz = 5
};

/** 
 *  \brief Enum describing the current device/module state
 */
typedef enum deviceState_tag
{
    deviceState_powerOff = 0,        ///< BGx is powered off, in this state all components on the LTEm1 are powered down.
    deviceState_powerOn = 1,         ///< BGx is powered ON, while powered on the BGx may not be able to interact fully with the host application.
    deviceState_appReady = 2         ///< BGx is powered ON and ready for application/services.
} deviceState_t;


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
 *  @brief Circular buffer for use in the transmit functions.
*/
typedef struct cbuf_tag
{
    uint8_t * buffer;       ///< The internal char buffer storing the text
    int head;               ///< Integer offset to the position where the next char will be added (pushed).
    int tail;               ///< Integer offset to the consumer position, where the next request will be sourced.
    int maxlen;             ///< The total size of the buffer. 
} cbuf_t;


/** 
 *  @brief Struct for a IOP transmit (TX) buffer control block. Tracks progress of chunk sends to LTEm1.
 *  @details LTEm SPI bridge works with chunks of ~64 bytes (actual transfers are usually 58 - 62 bytes). IOP abstracts SPI chunks from senders.
 */
typedef struct txBufferCtrl_tag
{
    char *txBuf;                        ///< Pointer to the base address of the TX buffer. Fixed, doesn't change with operations.
    char *chunkPtr;                     ///< Pointer to the next "chunk" of data to send to modem.
    uint16_t remainSz;                    ///< Remaining number of bytes in buffer to send to modem.
} txBufferCtrl_t;


/** 
 *  @brief Receive buffer page. Component struct for the rxDataBufferCtrl_t.
 */
typedef volatile struct rxBufferPage_tag
{
    char *_buffer;           ///< base address of page buffer (fixed, does not change)
    char *head;              ///< filled data (in), available for next data in
    char *tail;              ///< data pointer (consumer out)
    char *prevHead;          ///< if the last chunk is copied or consumed immediately used to restore head
} rxBufferPage_t;


/** 
 *  @brief Struct for a IOP smart buffer. Contains the char buffer and controls to marshall data between IOP and consumer (cmd,sockets,mqtt,etc.).
 * 
 *  @details 
 *  bufferSync is a semphore to signal buffer page role swap underway. ISR will sync with this upon entering the RX critical section 
 * 
 *  - Receive consumers (doWork functions) wanting to swap RX buffer pages will set bufferSync
 *  - This will force ISR to check iopPg and _nextIopPg and complete swap if necessary
 *  - Once buffer page swap is done, bufferSync will be reset
 *  - If interrupt fires ISR will check bufferSync prior to servicing a RX event
 * 
 *  _doWork() [consumer] uses IOP_swapRxBufferPage(rxDataBufferCtrl_t *bufPtr), ISR uses IOP_isrCheckBufferSync()
 * 
 *  NOTE: the completion test methods both consider the whole buffer for RX complete, but split buffers are returned to the application as 
 *  each is filled or the entire RX is completed.
 */
typedef volatile struct rxDataBufferCtrl_tag
{
    // _ variables are set at initialization and don't change
    char *_buffer;              ///< data buffer, does not change while used.
    char *_bufferEnd;           ///< end of physical buffer
    uint16_t _bufferSz;         ///< effective buffer size (after split)
    uint16_t _pageSz;           ///< the size of split size
    bool _overflow;             ///< set in ISR if overflow detected

    bool bufferSync;            ///< set when page swap is underway
    uint8_t _nextIopPg;         ///< intended resulting iopPg value

    uint8_t iopPg;              ///< when in split mode, which buffer page is assigned to IOP for filling
    rxBufferPage_t pages[2];    ///< buffer page to support interwoven fill/empty operations
} rxDataBufferCtrl_t;


/** 
 *  @brief Struct for a single page IOP smart buffer. Used by commands (AT cmd) and for capturing BGx async events.
 */
typedef volatile struct rxCoreBufferCtrl_tag
{
    // _variables are set at initialization and don't change
    char *_buffer;              ///< data buffer, does not change while used.
    char *_bufferEnd;           ///< end of physical buffer
    uint16_t _bufferSz;         ///< effective buffer size (after split)
    bool _overflow;             ///< set in ISR if overflow detected; doWork() moves to _prevOverflow, notifies application, then clears

    char *tail;                 ///< consumer out pointer
    char *head;                 ///< data in pointer
    char *prevHead;             ///< if the last chunk is copied or consumed immediately used to restore head
} rxCoreBufferCtrl_t;

/**
 *   \brief Brief inline static function to support doWork() readability
*/
static inline uint16_t IOP_rxPageDataAvailable(rxDataBufferCtrl_t *buf, uint8_t page)
{
    return buf->pages[page].head - buf->pages[page].tail;
}


/** 
 *  @brief Enum of data stream peer types
 */
typedef enum dataStreamType_tag
{
    dataStream_none = 0,
    dataStream_sckt = 2,
    dataStream_mqtt = 3,
    dataStream_http = 4,
    dataStream_file = 5
} dataStreamType_tag;



/** 
 *  @brief Enum of data stream peers: network data contexts + the BGx file system
 *  @details Only data contexts that coincide with SSL contexts are supported.
 */
typedef enum streamPeer_tag
{
    streamPeer_0 = 0,
    streamPeer_1 = 1,
    streamPeer_2 = 2,
    streamPeer_3 = 3,
    streamPeer_4 = 4,
    streamPeer_5 = 5,
    streamPeer_file = 6,
    streamPeer_cnt = 7
} streamPeer_t;


/** 
 *  @brief Base struct containing common properties required of a stream control
 */
typedef struct iopStreamCtrl_tag
{
    uint16_t ctrlMagic;                                     /// magic flag to validate incoming requests 
    dataCntxt_t dataCntxt;                                  /// Data context where this control operates (only SSL/TLS contexts 1-6)
    protocol_t protocol;                                    /// Socket's protocol : UDP/TCP/SSL.
    bool useTls;                                            /// flag indicating SSL/TLS applied to stream
    char hostUrl[host__urlSz];                              /// URL or IP address of host
    uint16_t hostPort;                                      /// IP port number host is listening on (allows for 65535/0)
    rxDataBufferCtrl_t recvBufCtrl;                         /// RX smart buffer 
} iopStreamCtrl_t;


/** 
 *  \brief Struct for the IOP subsystem state. During initialization a pointer to this structure is reference in g_ltem1.
 * 
 *  Each of the protocols (sockets, MQTT, HTTP) have unique behaviors: sockets receive asynchronous alerts but the data
 *  receive process is fully synchronous, HTTP is synchronous on receive tied to page read, MQTT is fully asynchronous 
 *  with msg event receive signaling and data transfer taking place async behind the scenes initiated on interrupt. 
 */
typedef volatile struct iop_tag
{
    cbuf_t *txBuffer;                                           /// transmit buffer (there is just one)
    uint16_t txPend;                                            /// outstanding TX char pending
    rxCoreBufferCtrl_t *rxCBuffer;                              /// URC and command response receive buffer, this is the default RX buffer
    iopStreamCtrl_t* streamPeers[streamPeer_cnt];               /// array of iopStream ctrl pointers, cast to a specific stream type: protocol or file stream
    iopStreamCtrl_t* rxStreamCtrl;                              /// stream data source, if not null
    char urcDetectBuffer[SET_PROPLEN(IOP__urcDetectBufferSz)];

    uint32_t txSendStartTck;                                    /// tick count when TX send started, used for response timeout detection
    uint32_t rxLastFillChgTck;                                  /// tick count when RX buffer fill level was known to have change

    // protocol specific properties
    uint8_t scktMap;                                            /// bitmap indicating open sockets (TCP/UDP/SSL), bit position is the dataContext (IOP event detect shortcut)
    uint8_t scktLstWrk;                                         /// bit mask of last sckt do work IRD inquiry (fairness)
    uint8_t mqttMap;                                            /// bitmap indicating open mqtt(s) connections, bit position is the dataContext (IOP event detect shortcut)
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


typedef cmdParseRslt_t (*cmdResponseParser_func)();                             // AT response parser template


/** 
 *  \brief Structure to control invocation and management of an AT command with the BGx module.
*/
typedef struct atcmd_tag
{
    char cmdStr[bufferSz__cmdTx];                       /// AT command string to be passed to the BGx module.
    uint32_t timeout;                                   /// Timout in milliseconds for the command, defaults to 300mS. BGx documentation indicates cmds with longer timeout.
    bool isOpenLocked;                                  /// True if the command is still open, AT commands are single threaded and this blocks a new cmd initiation.
    bool autoLock;                                      /// last invoke was auto and should be closed automatically on complete
    uint32_t invokedAt;                                 /// Tick value at the command invocation, used for timeout detection.
    char *response;                                     /// PTR into IOP "core" buffer, the response to the command received from the BGx (reset NULL fills buffer).
    char *responseData;                                 /// PTR to response buffer adjusted to after found preamble pattern.
    uint32_t execDuration;                              /// duration of command's execution in milliseconds
    resultCode_t resultCode;                            /// consumer API result value (HTTP style), success=200, timeout=408, single digit BG errors are expected to be offset by 1000
    cmdResponseParser_func responseParserFunc;          /// parser function to analyze AT cmd response and optionally extract value
    cmdParseRslt_t parserResult;                        /// last parser invoke result returned
    bool preambleFound;                                 /// true if parser found preamble
    char errorDetail[SET_PROPLEN(ltem__errorDetailSz)]; /// BGx error code returned, could be CME ERROR (< 100) or subsystem error (generally > 500)
    int32_t retValue;                                   /// optional signed int value extracted from response
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
