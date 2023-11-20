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
 * @brief LTEmC system constants
 * @details Field sizes, resource allocations/counts, etc.  
 */
enum ltem__constants
{
    ltem__bufferSz_rx = 2000,
    ltem__bufferSz_tx = 1000,
    ltem__swVerSz = 12,
    ltem__errorDetailSz = 18,
    ltem__moduleTypeSz = 8,
    ltem__streamCnt = 4,                    // 6 SSL/TLS capable data contexts + file system allowable, 4 concurrent seams reasonable
    ltem__reportsBffrSz = 160,
    ltem__dateTimeBffrSz = 24,
    ltem__hostUrlSz = 192,


    // ATCMD Module Constants
    atcmd__noTimeoutChange = 0,
    atcmd__defaultTimeout = 800,
    atcmd__cmdBufferSz = 448,               // prev=120, mqtt(Azure) connect=384, new=512 for universal cmd coverage, data mode to us dynamic TX bffr switching
    atcmd__respBufferSz = 128,
    atcmd__respTokenSz = 64,
    atcmd__streamPrefixSz = 12,             // obsolete with universal data mode switch
    atcmd__dataModeTriggerSz = 13
};



/** 
 * @brief Typed numeric constants used in HTTP module.
*/
enum http__constants
{
    // http__getRequestLength = 128,
    // http__postRequestLength = 128,

    http__noResponseHeaders = 0, 
    http__returnResponseHeaders = 1, 
    http__getRequestSz = 448,
    http__postRequestSz = 448,
    http__useDefaultTimeout = 0,
    http__defaultTimeoutBGxSec = 60,
    http__rqstTypeSz = 5,                           /// GET or POST
    http__customHdrSmallWarning = 40,
    http__readToFileBytesPerSecond = 11520,
    http__readToFileTimeoutRatio = 150,
    http__readToFileNameSzMax = 80,
    http__readToFileTimeoutSec = 240,               // Total number of seconds for read to file allowed (atcmd processing)
    http__readToFileInterPcktTimeoutSec = 20        // BGx inter-packet timeout (max interval between two packets)
};




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


// /** 
//  *  \brief AT command response parser result codes.
// */
// typedef enum cmdParseRslt_tag
// {
//     cmdParseRslt_pending = 0x00,

//     cmdParseRslt_preambleMissing = 0x01,
//     cmdParseRslt_countShort = 0x02,
//     cmdParseRslt_moduleError = 0x04,

//     cmdParseRslt_success = 0x40,
//     cmdParseRslt_error = 0x80,
// } cmdParseRslt_t;


// typedef enum dmState_tag
// {
//     dmState_idle = 0,
//     dmState_enabled = 1,
//     dmState_active = 2
// } dmState_t;


// /* SSL/TLS Module Type Definitions
//  * ------------------------------------------------------------------------------------------------------------------------------*/

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


// /** 
//  *  @brief Emum describing the results of a MQTT publish.
// */
// typedef enum mqttResult_tag
// {
//     mqttResult_success = 0,         // Sucessful publish.
//     mqttResult_retransmission = 1,  // Publish Sucessful, with retransmission.
//     mqttResult_failed = 2           // Publish failed.
// } mqttResult_t;


// /** 
//  *  @brief Enum specifying the MQTT QOS for a publish and subscribed topics.
// */
// typedef enum mqttQos_tag
// {
//     mqttQos_0 = 0,                  // QOS 0: At most once (aka send and forget)
//     mqttQos_1 = 1,                  // QOS 1: At least once
//     mqttQos_2 = 2,                  // QOS 2: Exactly once
//     mqttQos_maxAzure = 1            // MS Azure IoTHub max QOS (setting to QOS2 will force disconnect)
// } mqttQos_t;



// /** 
//  *  @brief Streams 
//  *  @details Structures for stream control/processing
//  * ================================================================================================
//  */

// /** 
//  *  @brief Typed numeric constants for stream peers subsystem (sockets, mqtt, http)
//  */
// enum streams__constants
// {
//     streams__ctrlMagic = 0x186F,
//     streams__maxContextProtocols = 5,
//     streams__typeCodeSz = 4,
//     streams__urcPrefixesSz = 60,
//     host__urlSz = 192
// };


/** 
 *  \brief Enum describing the current device/module state
 */
typedef enum deviceState_tag
{
    deviceState_powerOff = 0,        // BGx is powered off, in this state all components on the LTEm1 are powered down.
    deviceState_powerOn = 1,         // BGx is powered ON, while powered on the BGx may not be able to interact fully with the host application.
    deviceState_ready = 2,           // BGx is powered ON and ready for application/services.
    deviceState_error = 99           // error or invalid state detected.
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


// /* ================================================================================================================================
//  * LTEmC module definitions
//  * ==============================================================================================================================*/




/* Modem/Provider/Network Type Definitions
 * ------------------------------------------------------------------------------------------------------------------------------*/

#define NTWK_PROVIDER_RAT_AUTO "00"     // M1 (eMTC) >> NB-IoT >> GSM
#define NTWK_PROVIDER_RAT_GSM "01"
#define NTWK_PROVIDER_RAT_M1 "02"
#define NTWK_PROVIDER_RAT_NB "03"


/** 
 *  \brief Enum describing the priority for radio receive.
*/
typedef enum ltemRfPriority_tag
{
    ltemRfPriority_gnss = 0,
    ltemRfPriority_wwan = 1,
} ltemRfPriority_t;


/** 
 *  \brief Enum describing the mode the BGx module is using to look for available networks (carriers).
*/
typedef enum ntwkScanMode_tag
{
    ntwkScanMode_auto = 0U,             // BGx is considering either GSM or LTE carrier connections.
    ntwkScanMode_gsmonly = 1U,          // GSM only mode: BGx is filtering visible networks and only considering connections to GSM endpoints.
    ntwkScanMode_lteonly = 3U           // LTE only mode: BGx is filtering visible networks and only considering connections to LTE endpoints.
} ntwkScanMode_t;


/** 
 *  \brief Enum describing the available options for an IoT protocol when connecting to the network.
*/
typedef enum ntwkIotMode_tag
{
    ntwkIotMode_M1 = 0U,                // CAT-M1 only mode: BGx is filtering visible networks and only considering CAT-M1 connections.
    ntwkIotMode_NB = 1U,                // NB-IOT only mode: BGx is filtering visible networks and only considering NB-IOT connections.
    ntwkIotMode_M1NB = 2U               // The BGx will connect to either a CAT-M1 or NB-IOT network.
} ntwkIotMode_t;


/** 
 *  \brief Typed numeric constants for network subsystem.
*/
enum ntwk__constants
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


typedef enum pdpProtocol_tag
{
    pdpProtocol_notSet = 0,
    pdpProtocol_IPV4 = 1,
    pdpProtocol_IPV6 = 2,
    pdpProtocol_IPV4V6 = 3,
    pdpProtocol_PPP = 99                    // not supported by LTEmC
} pdpProtocol_t;


/**
 * @brief Authentication methods for packet network (PDP context) where required by network operator
 */
typedef enum pdpCntxtAuthMethods_tag
{
    pdpCntxtAuthMethods_none = 0,
    pdpCntxtAuthMethods_pap = 1,
    pdpCntxtAuthMethods_chap = 2,
    pdpCntxtAuthMethods_papChap = 3
} pdpCntxtAuthMethods_t;


/** 
 *  \brief RF Priority map for BG95/BG77 modules.
*/
typedef enum ltemRfPriorityMode_tag
{
    ltemRfPriorityMode_gnss = 0,
    ltemRfPriorityMode_wwan = 1,
    ltemRfPriorityMode_none = 9
} ltemRfPriorityMode_t;


/** 
 *  \brief RF Priority map for LTEm modem with BG95/BG77 modules.
*/
typedef enum ltemRfPriorityState_tag
{
ltemRfPriorityState_unloaded = 0,           // WWAN/GNSS in unloaded state
ltemRfPriorityState_wwanPending = 1,        // WWAN in pending state
ltemRfPriorityState_gnssPending = 2,        // GNSS in pending state
ltemRfPriorityState_wwanLoaded = 3,         // WWAN in loaded state
ltemRfPriorityState_gnssLoaded = 4          // GNSS in loaded state
} ltemRfPriorityState_t;

// typedef resultCode_t (*genericResultCode_func)();             // generic parameter-less function returning a resultCode_t
// typedef void (*genericVoid_func)();                           // generic parameter-less function void return



/* Function Prototype Definitions 
 * ------------------------------------------------------------------------------------------------------------------------------*/
// typedef void (*doWork_func)();                                      // module background worker
// typedef void (*powerSaveCallback_func)(uint8_t newPowerSaveState);  // callback for return from a powersave sleep
typedef resultCode_t (*dataRxHndlr_func)();                             // callback to application to deliver data from LTEm
// typedef resultCode_t (*urcEvntHndlr_func)();                        // data comes from rxBuffer, this function parses and forwards to application via appRcvProto_func
typedef void (*appRcvProto_func)();                                 // prototype func() for stream recvData callback
// typedef cmdParseRslt_t (*cmdResponseParser_func)();                 // AT response parser template



typedef struct ltemPinConfig_tag
{
    int spiIndx;
    int spiCsPin;
    int spiClkPin;
    int spiMisoPin;
    int spiMosiPin;
    int irqPin;
    int statusPin;
    int powerkeyPin;
    int resetPin;
    int ringUrcPin;
    int connected;
    int wakePin;
} ltemPinConfig_t;


// /** 
//  *  \brief Struct holding cellular and radio settings.
// */
// typedef struct modemSettings_tag
// {
//     char scanSequence[PSZ(ntwk__scanSeqSz)];
//     ntwkScanMode_t scanMode;
//     ntwkIotMode_t iotMode;
//     char pdpNtwkConfig[ntwk__pdpNtwkConfigSz];  // Invoke ready default context config
// } modemSettings_t;


/** 
 *  \brief Struct holding information about the physical BGx module.
*/
typedef struct modemInfo_tag
{
	char imei[PSZ(ntwk__imeiSz)];            // IMEI (15 digits) International Mobile Equipment Identity or IEEE UI (aka MAC, EUI-48 or EUI-64).
	char iccid[PSZ(ntwk__iccidSz)];          // ICCID (up to 24 digits) Integrated Circuit Card ID. Set in the SIM card at manufacture.
    char mfg[PSZ(ntwk__dvcMfgSz)];           // Device manufacturer name
	char model[PSZ(ntwk__dvcModelSz)];       // Device model number
	char fwver[PSZ(ntwk__dvcFwVerSz)];       // Firmware version of the device
    char swver[PSZ(ltem__swVerSz)];          // software driver version
} modemInfo_t;


/** 
 *  \brief Struct representing the state of active PDP contexts (aka: APN or data context).
*/
typedef struct packetNetwork_tag
{
    bool isActive;
    uint8_t pdpContextId;                           // context ID recognized by the carrier (valid are 1 to 16)
    pdpProtocol_t pdpProtocol;                      // IPv4, IPv6, etc.
	char ipAddress[ntwk__ipAddressSz];              // The IP address obtained from the carrier for this context. The IP address of the modem.
} packetNetwork_t;


/** 
 *  \brief Struct respresenting an ACTIVE network carrier/operator.
*/
typedef struct ntwkOperator_tag
{
	char name[PSZ(ntwk__operatorNameSz)];               // Provider name, some carriers may report as 6-digit numeric carrier ID.
	char iotMode[PSZ(ntwk__iotModeNameSz)];             // Network carrier protocol mode: CATM-1 or NB-IOT for BGx.
    uint8_t defaultContext;
    uint8_t pdpCntxtCnt;                                // The number of PDP contexts available
    packetNetwork_t packetNetworks[ntwk__pdpContextCnt];  // Collection of packet network with cell operator. This is typically only 1, but some carriers implement more (ex VZW).
} ntwkOperator_t;





// typedef struct streamCtrl_tag                       // structure to govern LTEmC handling of protocol stream
// {
//     char streamType;                                // stream type
//     dataCntxt_t dataCntxt;                          // integer representing the source of the stream; fixed for protocols, file handle for FS
//     dataRxHndlr_func dataRxHndlr;                   // function to handle data streaming, initiated by eventMgr() or atcmd module
//     urcEvntHndlr_func urcHndlr;                     // function to handle data streaming, initiated by eventMgr() or atcmd module
// } streamCtrl_t;


// /*
//  * ============================================================================================= */


// /** 
//  *  \brief Result structure returned from a action request (await or get).
// */
// typedef struct atcmdResult_tag
// {
//     resultCode_t statusCode;                                        // The HTML style status code, indicates the sucess or failure (type) for the command's invocation.
//     char *response;                                                 // The char c-string containing the full response from the BGx.
//     uint16_t responseCode;                                          // Numeric response value from many "status" action parsers (suffixed with _rc)
// } atcmdResult_t;


#endif  // !__LTEMC_TYPES_H__
