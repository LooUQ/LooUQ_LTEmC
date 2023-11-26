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
#include <stdlib.h>
// #endif // __cplusplus


#include "ltemc.h"
#include "ltemc-iop.h"
#include "ltemc-atcmd.h"
#include "ltemc-network.h"


// #include <lq-types.h>
// // #include <lq-str.h>                         // most LTEmC modules use LooUQ string functions
// // #include <lq-bBuffer.h>

// // #include "ltemc-types.h"
// // #include "ltemc.h"
// #include "ltemc-iSizes.h"

// #include "ltemc-nxp-sc16is.h"                   // core LTEmC modules
// #include "ltemc-quectel-bg.h"
// #include "ltemc-iop.h"
// #include "ltemc-atcmd.h"

// #include "ltemc-network.h"                      // mandatory services modules
// #include "ltemc-files.h"
// #include "ltemc-tls.h"



// /**
//  * @brief Stream types supported by LTEmC 
//  */
// typedef enum streamType_tag
// {
//     streamType_UDP = 'U',
//     streamType_TCP = 'T',
//     streamType_SSLTLS = 'S',
//     streamType_MQTT = 'M',
//     streamType_HTTP = 'H',
//     streamType_file = 'F',
//     streamType_SCKT = 'K',
//     streamType__ANY = 0
// } streamType_t;


// /** 
//  *  @brief Typed numeric constants for stream peers subsystem (sockets, mqtt, http)
//  */
// enum streams__constants
// {
//     streams__ctrlMagic = 0x186F,
//     streams__maxContextProtocols = 5,
//     streams__typeCodeSz = 4,
//     streams__urcPrefixesSz = 60
// };


/**
 * @brief enum describing the last receive event serviced by the ISR
 */
typedef enum recvEvent_tag
{
    recvEvent_none = 0,
    recvEvent_data,
    recvEvent_timeout
} recvEvent_t;


/** 
 *  @brief Enum of the available streamCntxt indexes for BGx (only SSL/TLS capable contexts are supported).
 */
typedef enum streamCntxt_tag
{
    streamCntxt_0 = 0,
    streamCntxt_1 = 1,
    streamCntxt_2 = 2,
    streamCntxt_3 = 3,
    streamCntxt_4 = 4,
    streamCntxt_5 = 5,
    streamCntxt__cnt = 6,
    streamCntxt_file = 9,
    streamCntxt__none = 255
} streamCntxt_t;




// /**
//  * @brief Generic "function" pointer
//  * @details Used as placeholder for streams received data application callback. Each stream has a specialized callback function
//  * to deliver data to the application (app), however there is a shared struct for the common stream header. When the stream 
//  * control is init, the stream specific app receive function is cast to signature below, then cast back to stream specific
//  * function signature at invoke.
//  */
// typedef void (*genericAppRecv_func)(void);



// typedef void (*doWork_func)();                                       // module background worker
// typedef void (*powerSaveCallback_func)(uint8_t newPowerSaveState);   // callback for return from a powersave sleep
// typedef resultCode_t (*dataRxHndlr_func)();                             // rxBuffer to stream: this function parses and forwards to application via appRcvProto_func
// typedef void (*appRcvProto_func)();                                  // prototype func() for stream recvData callback


// /**
//  * @brief Internal generic stream control matching protocol specific controls (for 1st set of fields)
//  */
// typedef struct streamCtrl_tag
// {
//     streamType_t streamType;                // stream type (cast to char from enum )
//     dataCntxt_t dataCntxt;                  // stream context 
//     int8_t asyncIndx;                       // index of matching struct containing async stream function pointers
// //    appGenRcvr_func appRcvr;                // application receiver for incoming network data
// } streamCtrl_t;


// /**
//  * @brief Asynchronous stream control, registered/deregistered with LTEmC on stream open/close
//  */
// typedef struct asyncCtrl_tag
// {
//     char streamType;                        // stream type; async currently is MQTT and sockets (HTTP/filesystem are synchronous)
//     urcEvntHndlr_func urcHndlr;             // URC handler (invoke by eventMgr)
//     dataHndlr_func dataRxHndlr;             // function to handle data streaming, initiated by eventMgr() or atcmd module
// } asyncCtrl_t;


/* Metric Type Definitions
 * ------------------------------------------------------------------------------------------------------------------------------*/

/**
 * @brief Metric counters maintained by LTEmC 
 */
typedef struct ltemMetrics_tag
{
    // metrics
    uint32_t cmdInvokeCnt;
    uint32_t isrInvokeCnt;
    uint32_t isrReadCnt;
    uint32_t isrWriteCnt;
} ltemMetrics_t;


/**
 * @brief Static char arrays to simplify passing string responses back to user application.
 */
typedef struct ltemStatics_tag
{
    char dateTimeBffr[PSZ(ltemSz__dateTimeBffrSz)];   // reused by date/time functions for parsing formats
    char reportBffr[PSZ(ltemSz__reportsBffrSz)];      // reused by *Rpt() functions
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

    platformSpi_t* platformSpi;                         // generic SPI interface to use for LTEm command/data I/O
    iop_t *iop;                                         // IOP subsystem controls
    atcmd_t *atcmd;                                     // AT command processor controls
    ntwkSettings_t *ntwkSettings;                       // Settings to control radio and cellular network initialization
	modemInfo_t *modemInfo;                             // Data structure holding persistent information about modem device
    ntwkOperator_t *ntwkOperator;                       // Data structure representing the cellular network provider and the networks (PDP contexts it provides)
    streamCtrl_t* streams[ltemSz__streamCnt];           // Data streams: data context protocols
    ltemMetrics_t metrics;                              // metrics for operational analysis and reporting
    ltemStatics_t statics;                              // small collection of static buffers for composition and return of modem information
} ltemDevice_t;


/* LTEmC Global Singleton Instance
------------------------------------------------------------------------------------------------- */
extern ltemDevice_t g_lqLTEM;



/* LTEmC Internal Functions; Excluded from public API
 * ============================================================================================= */
#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

/**
 *	\brief Initialize BGx Radio Access Technology (RAT) options.
 */
void NTWK_initRatOptions();


/**
 *	@brief Apply the a PDP context configuration settings to BGx.
 */
void NTWK_applyPpdNetworkConfig();


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


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  // !__LTEMC_ITYPES_H__
