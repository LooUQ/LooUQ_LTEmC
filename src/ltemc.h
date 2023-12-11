/** ***************************************************************************
  @file 
  @brief API for control and use of the LooUQ LTEm cellular modem.

  @author Greg Terrell, LooUQ Incorporated

  \loouq
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


#ifndef __LTEMC_H__
#define __LTEMC_H__

#define LTEmC_VERSION "3.0.1"

#include <lq-diagnostics.h>
#include <lq-logging.h>


// #include <lq-types.h>                           /// LooUQ embedded device library typedefs, common across products/libraries
// #include <lq-diagnostics.h>                     /// ASSERT and diagnostic data collection
// #include "ltemc-types.h"                        /// type definitions for LTEm device driver: LTEmC
// #include "lq-platform.h"                        /// platform abstractions (arduino, etc.)
// #include "ltemc-atcmd.h"                        /// command processor interface
// #include "ltemc-mdminfo.h"                      /// modem information
// #include "ltemc-network.h"                      /// cellular provider and packet network 
// #ifdef __cplusplus
// extern "C"
// {
// #endif // __cplusplus

// Internal static buffers you may need to change for your application. Contact LooUQ for details.
// #define IOP_RX_COREBUF_SZ 256
// #define IOP_TX_BUFFER_SZ 1460

#include <lq-types.h>                           /// LooUQ embedded device library typedefs, common across products/libraries
#include "ltemc-types.h"                        /// type definitions for LTEm device driver: LTEmC
//#include <lqdiag.h>                             /// PRINTDBG and ASSERT diagnostic data collection

#include "ltemc-platform.h"                     /// platform abstractions (arduino, etc.)
#include "ltemc-atcmd.h"                        /// command processor interface
#include "ltemc-network.h"                      /// cellular provider and packet network 


/* Add the following LTEmC feature sets as required for your project's main .cpp or .c file 
 * ----------------------------------------------------------------------------------------------- */
// #include "ltemc-gnss.h"                         /// GNSS/GPS location services
// #include "ltemc-sckt.h"                         /// tcp/udp socket communications
// #include "ltemc-tls"                            /// SSL/TLS support
// #include "ltemc-http"                           /// HTTP(S) support: GET/POST requests
// #include "ltemc-mqtt"                           /// MQTT(S) support
// #include "ltemc-filesys.h"                      /// use of BGx module file system functionality
// #include "ltemc-gpio.h"                         /// use of BGx module GPIO expansion functionality


/* LTEmC uses 2 global buffers: the IOP transmit buffer and the ATCMD cmd\core buffer.
 * These are sized in the ltemc-iop.h and ltemc-atcmd.h header files respectively.
 * LooUQ set defaults sizes are:
 *      IOP__txBufferSize = 1800
 *      ATCMD__commandBufferSz = 256
 */
/* ----------------------------------------------------------------------------------- */

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


// typedef void (*eventNotifCallback_func)(uint8_t notifCode, const char *message);

/**
 *	@brief Initialize the LTEm1 modem.
 *	@param ltem_config [in] - The LTE modem gpio pin configuration.
 *  @param applicationCallback [in] - If supplied (not NULL), this function will be invoked for significant LTEm events.
 */
void ltem_create(const ltemPinConfig_t ltem_config, yield_func yieldCB, appEvntNotify_func eventNotifyCB);


/**
 *	@brief Uninitialize the LTEm device structures.
 */
void ltem_destroy();


/**
 *	@brief Set radio priority. 
 *  @param [in] radioPriority The priority consumer for the radio receive path.
 *  @return Result code representing status of operation, OK = 200.
 */
resultCode_t ltem_setRfPriorityMode(ltemRfPriorityMode_t priorityMode);


/**
 *	@brief Set RF priority on BG95/BG77 modules. 
 *  @return Current RF priority mode
 */
ltemRfPriorityMode_t ltem_getRfPriorityMode();


/**
 *	@brief Set RF priority on BG95/BG77 modules. 
 *  @return Current RF priority state
 */
ltemRfPriorityState_t ltem_getRfPriorityState();


/**
 *	@brief Power on and start the modem
 *  @param resetIfPoweredOn [in] Perform a software reset on the modem, if found in a powered on state
 */
bool ltem_start(resetAction_t resetAction);


/**
 *	@brief Powers off the modem without destroying memory objects. Power modem back on with ltem_start()
 */
void ltem_stop();


// /**
//  *	@brief Performs a software restart of LTEm1.
//  *  @param coldStart [in] Set to true if the LTEm is being hard started, from a reset/power ON
//  */
// void ltem_initDevice(bool coldStart);


/**
 *	@brief Performs a reset of LTEm.
 */
bool ltem_reset(bool hardReset);


/**
 *	@brief Turn modem power on/off.
 *	@param [in] powerState New state for modem: true is on, false is off.
 */
void ltem_setPowerState(bool powerState);


/**
 *	@brief Test for responsive BGx.
 *  @return True if modem is ready and responsive.
 */
bool ltem_ping();

/* RF Priority (BG95\BG77)
 ----------------------------------------------------------------------------------------------- */

/**
 *	@brief Set RF priority on BG95/BG77 modules. 
 *	@param [in] setPriority New radio priority.
 *  @return Result code representing status of operation, OK = 200.
 */
resultCode_t ltem_setRfPriorityMode(ltemRfPriorityMode_t priorityMode);


/**
 *	@brief Get RF priority mode on BG95/BG77 modules. 
 *  @return Result code representing status of operation, OK = 200.
 */
ltemRfPriorityMode_t ltem_getRfPriorityMode();


/**
 *	@brief Get RF priority state on BG95/BG77 modules. 
 *  @return Result code representing status of operation, OK = 200.
 */
ltemRfPriorityState_t ltem_getRfPriorityState();


// /**
//  *	@brief Get the current UTC date and time.
//  *  @param [out] dateTime Pointer to a character array (length >= 20 chars) to be updated with current UTC date/time 
//  *  @details Formatted as: 23/09/01,13:48:55
//  */
// void ltem_getDateTimeUtc(char *dateTime);

/**
 *	@brief Get the current UTC date and time.
 */
const char* ltem_getUtcDateTime(char format);


/**
 *	@brief Get local time zone offset.
 */
int8_t ltem_getLocalTimezoneOffset(bool precise);


/**
 *  @brief Get the LTEm1 static device identification/provisioning information.
 *  @return Modem information struct, see mdminfo.h for details.
*/
modemInfo_t *ltem_getModemInfo();


/**
 *  @brief Test for SIM ready
 *  @return True if SIM is inserted and available
*/
bool ltem_isSimReady();


/**
 *  @brief Get the signal strength reported by the LTEm device at a percent
 *  @return The radio signal strength in the range of 0 to 100 (0 is no signal)
*/
uint8_t ltem_signalPercent();


/**
 *  @brief Get the signal strength reported by the LTEm device as RSSI reported
 *  @return The radio signal strength in the range of -51dBm to -113dBm (-999 is no signal)
*/
int16_t ltem_signalRSSI();


/**
 *  @brief Get the signal strength reported by the LTEm device as RSSI reported
 *  @return The raw radio signal level reported by BGx
*/
uint8_t ltem_signalRaw();


/** 
 *  @brief Get the signal strength, as a bar count for visualizations, (like on a smartphone) 
 *  @return The radio signal strength factored into a count of bars for UI display
 * */
uint8_t ltem_signalBars(uint8_t displayBarCount);


/**
 *	@brief Get the LTEmC software version.
 *  \return Version as a const char pointer.
 */
const char* ltem_getSwVersion();


/**
 *	@brief Get the LTEmC software version.
 *  \return Version as a const char pointer.
 */
const char* ltem_getModuleType();


/**
 *	@brief Reads the hardware status and internal application ready field to return device ready state
 *  \return DeviceState: 0=power off, 1=power on, 2=appl ready
 */
deviceState_t ltem_getDeviceState();


/**
 *	@brief Background work task runner. To be called in application Loop() periodically.
 */
void ltem_eventMgr();


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


/**
 *	@brief Registers the address (void*) of your application yield callback handler.
 *  @param yieldCallback [in] Callback function in application code to be invoked when LTEmC is in await section.
 */
void ltem_setYieldCallback(yield_func yieldCB);


/**
 *	@brief Registers the address (void*) of your application event notification callback handler.
 *  @param eventNotifCallback [in] Callback function in application code to be invoked when LTEmC is in await section.
 */
void ltem_setEventNotifCallback(appEvntNotify_func eventNotifyCB);


/**
 *	@brief Function of last resort, catastrophic failure Background work task runner. To be called in application Loop() periodically.
 *  @param notifyType [in] - Enum of broad notification categories.
 *  @param notifyMsg [in] - Message from origination about the issue being reported.
 */
void ltem_notifyApp(uint8_t notifyType, const char *notifyMsg);


#pragma region LooUQ/LTEM internal functions
/*
 ----------------------------------------------------------------------------------------------- */

void LTEM_registerDiagCallback(appDiagCallback_func diagCB);
void LTEM_diagCallback(const char* diagPointDescription);



// uint8_t LTEM__getStreamIndx(dataCntxt_t dataCntxt);


// /**
//  *	@brief Function of last resort, catastrophic failure Background work task runner. To be called in application Loop() periodically.
//  *  @param faultCode [in] - HTTP style error code.
//  */
// void ltem_notifyAssert(uint16_t faultCode)   __attribute__ ((noreturn));

#pragma endregion

#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__LTEMC_H__ */