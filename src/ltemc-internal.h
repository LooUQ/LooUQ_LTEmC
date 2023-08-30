/** ****************************************************************************
  \file 
  \brief LTEmC INTERNAL type/enum/struct definitions
  \author Greg Terrell, LooUQ Incorporated

  \loouq

  \warning Updates should be only be done as directed by LooUQ staff.

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


#ifndef __LTEMC_INTERNAL_H__
#define __LTEMC_INTERNAL_H__

#define PRODUCT "LM" 

// Common macro functions used across LTEmC environment
#define PERIOD_FROM_SECONDS(period)  (period * 1000)
#define PERIOD_FROM_MINUTES(period)  (period * 1000 * 60)
#define ELAPSED(start, timeout) ((start == 0) ? 0 : millis() - start > timeout)
#define STRCMP(x,y)  (strcmp(x,y) == 0)

// Internal static buffers you may need to change for your application. Contact LooUQ for details.
// #define IOP_RX_COREBUF_SZ 256
// #define IOP_TX_BUFFER_SZ 1460

#include "ltemc.h"
#include <lq-str.h>                         /// most LTEmC modules use LooUQ string functions
#include <lq-bBuffer.h>

#include "ltemc-quectel-bg.h"
#include "ltemc-nxp-sc16is.h"
#include "ltemc-iop.h"



/* ================================================================================================================================
 * LTEmC Global Structure
 *
 * LTEmC device is created as a global singleton variable
 * ==============================================================================================================================*/



/* Metric Type Definitions
 * ------------------------------------------------------------------------------------------------------------------------------*/
typedef struct ltemMetrics_tag
{
    // metrics
    uint32_t cmdInvokes;

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
    char streamType;                            /// stream type
    /*
     * NOTE: Does NOT follow exact struct field layout of the other streams, shares 1st field to validate type before casting 
     */
    uint8_t handle;
    dataRxHndlr_func dataRxHndlr;               /// function to handle data streaming, initiated by atcmd dataMode (RX only)
    appRcvProto_func appRecvDataCB;
} fileCtrl_t;


 /** 
 *  \brief Struct representing the LTEmC model. The struct behind the g_ltem1 global variable with all driver controls.
 * 
 *  Most subsystems are linked to this struct with pointers to allow for better abstraction and optional subsystems
 */
typedef struct ltemDevice_tag
{
	ltemPinConfig_t pinConfig;                  /// GPIO pin configuration for required GPIO and SPI interfacing
    bool cancellationRequest;                   /// For RTOS implementations, token to request cancellation of long running task/action
    deviceState_t deviceState;                  /// Device state of the BGx module
    appEvntNotify_func appEvntNotifyCB;         /// Event notification callback to parent application
    char moduleType[ltem__moduleTypeSz];        /// c-str indicating module type. BG96, BG95-M3, BG77, etc. (so far)

    platformSpi_t* platformSpi;
    //void *spi;                                  /// SPI device (methods signatures compatible with Arduino)
    
    iop_t *iop;                                 /// IOP subsystem controls
    atcmd_t *atcmd;                             /// Action subsystem controls
    modemSettings_t *modemSettings;             /// Settings to control radio and cellular network initialization
	modemInfo_t *modemInfo;                     /// Data structure holding persistent information about application modem state
    providerInfo_t *providerInfo;               /// Data structure representing the cellular network provider and the networks (PDP contexts it provides)
    streamCtrl_t* streams[ltem__streamCnt];     /// Data streams: protocols or file system
    fileCtrl_t* fileCtrl;

    ltemMetrics_t metrics;                      /// metrics for operational analysis and reporting
    uint16_t isrInvokeCnt;
} ltemDevice_t;


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

#pragma region ATCMD LTEmC Internal Functions
/* LTEmC internal, not intended for user application consumption.
 * --------------------------------------------------------------------------------------------- */

// /**
//  *	\brief Checks recv buffer for command response and sets atcmd structure data with result.
//  */
// resultCode_t ATCMD_readResult();

/**
 *  \brief Default AT command result parser.
*/
cmdParseRslt_t ATCMD_okResponseParser();

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


#pragma endregion
/* ------------------------------------------------------------------------------------------------
 * End NTWK LTEmC Internal Functions */




/*
---------------------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__LTEMC_INTERNAL_H__ */