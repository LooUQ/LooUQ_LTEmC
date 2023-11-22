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
// #include <lq-str.h>                         // most LTEmC modules use LooUQ string functions
// #include <lq-bBuffer.h>

// #include "ltemc-types.h"
// #include "ltemc.h"

#include "ltemc-nxp-sc16is.h"                   // core LTEmC modules
#include "ltemc-quectel-bg.h"
#include "ltemc-iop.h"
#include "ltemc-atcmd.h"

#include "ltemc.h"
#include "ltemc-network.h"                      // mandatory services modules
#include "ltemc-files.h"
#include "ltemc-tls.h"

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
    streamType_SCKT = 'K',
    streamType__ANY = 0
} streamType_t;


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






/**
 * @brief Generic "function" pointer
 * @details Used as placeholder for streams received data application callback. Each stream has a specialized callback function
 * to deliver data to the application (app), however there is a shared struct for the common stream header. When the stream 
 * control is init, the stream specific app receive function is cast to signature below, then cast back to stream specific
 * function signature at invoke.
 */
typedef void (*genericAppRecv_func)(void);


/**
 * @brief Stream internal function declarations
 * @details Each stream can have a URC handler (asynchronous events) and a data receiver (synchronous data transfer). These are
 * declared below (in that order).
 */
typedef resultCode_t (*urcEvntHndlr_func)();                            // callback into stream specific URC handler (async recv)


/**
 * @brief Data handler: generic function signature that can be a stream sync receiver or a general purpose ATCMD data mode handler
 */
typedef void (*dataHndlr_func)();                                       // callback into stream data handler (sync transfer)



// typedef void (*doWork_func)();                                       // module background worker
// typedef void (*powerSaveCallback_func)(uint8_t newPowerSaveState);   // callback for return from a powersave sleep
// typedef resultCode_t (*dataRxHndlr_func)();                             // rxBuffer to stream: this function parses and forwards to application via appRcvProto_func
// typedef void (*appRcvProto_func)();                                  // prototype func() for stream recvData callback




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

typedef struct streamCtrlHdr_tag
{
    char streamType;                            // stream type
    dataCntxt_t dataCntxt;                      // integer representing the source of the stream; fixed for protocols, file handle for FS
    dataHndlr_func dataRxHndlr;                 // function to handle data streaming, initiated by eventMgr() or atcmd module
} streamCtrlHdr_t;



// /*
//  * ============================================================================================= */

/**
 * @brief Adds a protocol stream to the LTEm streams table
 * @details ASSERTS that no stream is occupying the stream control's data context
 * 
 * @param streamCtrl The stream to add to the LTEm stream table
 */
void LTEM_registerStream(streamCtrlHdr_t *streamHdr);


/**
 * @brief Remove a stream from the LTEm streams table, excludes it from further processing
 * @details ASSERTS that the stream parameter matches the stream in the LTEm table
 * 
 * @param streamCtrl The stream to remove from the LTEm stream table
 */
void ltem_deregisterStream(streamCtrlHdr_t *streamHdr);


/**
 * @brief Get a stream control from data context, optionally filtering on stream type.
 * 
 * @param dataCntxt The data context for the stream 
 * @param streamType Protocol of the stream
 * @return streamCtrl_t* Pointer of a generic stream, can be cast (after type validation) to a specific protocol control
 */
streamCtrlHdr_t* ltem_findStream(uint8_t dataCntxt, streamType_t streamType);














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


typedef struct streamCtrlHdr_tag
{
    char streamType;                            // stream type
    dataCntxt_t dataCntxt;                      // integer representing the source of the stream; fixed for protocols, file handle for FS
    dataHndlr_func dataRxHndlr;                 // function to handle data streaming, initiated by eventMgr() or atcmd module
} streamCtrlHdr_t;



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

    platformSpi_t* platformSpi;                         // generic SPI interface to use for LTEm command/data I/O
    iop_t *iop;                                         // IOP subsystem controls
    atcmd_t *atcmd;                                     // AT command processor controls
    ntwkSettings_t *ntwkSettings;                       // Settings to control radio and cellular network initialization
	modemInfo_t *modemInfo;                             // Data structure holding persistent information about modem device
    ntwkOperator_t *ntwkOperator;                       // Data structure representing the cellular network provider and the networks (PDP contexts it provides)
    fileCtrl_t* fileCtrl;                               // static singleton stream for filesystem
    streamCtrlHdr_t* streams[ltem__streamCnt];             // Data streams: protocols or file system
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





#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  // !__LTEMC_ITYPES_H__
