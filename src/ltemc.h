/** ****************************************************************************
  \file 
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


#ifndef __LTEMC_H__
#define __LTEMC_H__

#define LTEmC_VERSION "3.1.0"


/* Communication Buffers
 *
 * LTEmC utilizes a buffers for general communications:
 * A command composition buffer, sized in ltem_types.h with atcmd__cmdBufferSz 
 * A command diagnostics history buffer, sized in ltem_types.h with atcmd__cmdBufferSz
 * A command response buffer, sized in ltem_types.h with atcmd__respBufferSz
 * A general purpose RX (receive) buffer, sized in ltem_types.h with ltem__bufferSz_rx
 * 
 * The RX buffer is implemented as a ring-buffer with block I/O optimized for the LTEm 
 * modem hardware. It must be large enough to hold a complete I/O event (typical 1K block
 * for file reads, 1.5K for MQTT, etc.). The default 2K buffer size should be considered
 * a minimum value.
 * 
 * The response buffer is used to collect/parse most BGx module command responses. If a 
 * command is going to respond with a large amount of info (aka a data transfer), the 
 * system will utilize the AT-CMD modules dataMode where the receiver will need to 
 * provide a data buffer. This is used with all of the stream (file, http, etc) subsystems.
 * 
 * If your application design requires a change in the internal buffers sizing, please
 * consult with LooUQ. We can be reached at answers@loouq.com
*/


#include <lq-types.h>                           /// LooUQ embedded device library typedefs, common across products/libraries
#include "ltemc-types.h"                        /// type definitions for LTEm device driver: LTEmC
#include <lqdiag.h>                             /// PRINTDBG and ASSERT diagnostic data collection

#include "ltemc-platform.h"                     /// platform abstractions (arduino, etc.)
#include "ltemc-atcmd.h"                        /// command processor interface
#include "ltemc-network.h"                      /// cellular provider and packet network 


/* Add the following LTEmC feature sets AS REQUIRED for your project, in your source code.
 * LooUQ recommends that you examine the appropriate example in the LooUQ_LTEmC\test folder.
 * ----------------------------------------------------------------------------------------------- */
// #include "ltemc-gnss.h"                         /// GNSS/GPS location services
// #include "ltemc-sckt.h"                         /// tcp/udp socket communications
// #include "ltemc-tls"                            /// SSL/TLS support
// #include "ltemc-http"                           /// HTTP(S) support: GET/POST requests
// #include "ltemc-mqtt"                           /// MQTT(S) support
// #include "ltemc-filesys.h"                      /// use internal flash file system
// #include "ltemc-gpio.h"                         /// use internal GPIO expansion (LTEm3f only)


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


/* Create and control LTEm instance
 * --------------------------------------------------------------------------------------------- */

/**
 *	@brief Initialize the LTEm1 modem.
 *	@param ltem_config [in] - The LTE modem gpio pin configuration.
 *  @param applicationCallback [in] - If supplied (not NULL), this function will be invoked for significant LTEm events.
 */
void ltem_create(const ltemPinConfig_t ltem_config, yield_func yieldCB, appEvntNotify_func eventNotifyCB);


/**
 *	@brief Power on and start the modem
 *  @param resetIfPoweredOn [in] Perform a software reset on the modem, if found in a powered on state
 */
bool ltem_start(resetAction_t resetAction);


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
 *	@brief Powers off the modem without destroying instance and configuration. Power modem back on with ltem_restart()
 *  @note Use ltem_restart() to restore LTEm operations
 */
void ltem_stop();


/**
 *	@brief Uninitialize the LTEm device structures.
 */
void ltem_disconnect();


/**
 *	@brief Uninitialize the LTEm device structures.
 */
void ltem_discard();


/* Configure LTEm/Application Integration
 * --------------------------------------------------------------------------------------------- */

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


/* Get information about the modem, SIM or signal strength
 * --------------------------------------------------------------------------------------------- */

/**
 *	@brief Test for responsive modem.
 *  @return True if modem is ready and responsive.
 */
bool ltem_ping();


/**
 *	@brief Get the LTEmC software version.
 *  @return Version as a const char pointer.
 */
const char* ltem_getModuleType();


/**
 *	@brief Get the LTEmC software version.
 *  @return Version as a const char pointer.
 */
const char* ltem_getSwVersion();


/**
 *	@brief Get the current UTC date and time.
 *  @param [out] dateTime Pointer to a character array (length >= 20 chars) to be updated with current UTC date/time 
 *  @details Formatted as: 23/09/01,13:48:55
 */
void ltem_getDateTimeUtc(char *dateTime);


/**
 *  @brief Get the LTEm static device identification/provisioning information.
 *  @return LTEm (modem) information struct, see mdminfo.h for details.
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
 *	@brief Reads the hardware status and internal application ready field to return device ready state
 *  @return DeviceState: 0=power off, 1=power on, 2=appl ready
 */
deviceState_t ltem_getDeviceState();



/* Streams (protocols)
 * Configure a protocol to use. Note the file stream is automatically configured and not added by
 * user application.
 * --------------------------------------------------------------------------------------------- */

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
 *	@brief Background work task runner. To be called in application Loop() periodically.
 *  @details Required for async receives: MQTT (topic subscribe), UDP/TCP/SSL/TLS sockets
 */
void ltem_eventMgr();


/* Control radio multiplexing on BG95/BG77 based modems
 * --------------------------------------------------------------------------------------------- */

/**
 *	@brief Set radio priority. 
 *  @param [in] radioPriority The priority consumer for the radio receive path.
 *  @return Result code representing status of operation, OK = 200.
 */
resultCode_t ltem_setRfPriority(ltemRfPrioritySet_t radioPriority);


/**
 *	@brief Set RF priority on BG95/BG77 modules. 
 *  @return Result code representing status of operation, OK = 200.
 */
ltemRfPriorityState_t ltem_getRfPriority();



/* Power-saving: Control PCM and eDRX settings
 * --------------------------------------------------------------------------------------------- */

/**
 *	@brief Put modem in PSM mode
 */
//void ltem_enterPsm();



#pragma region LTEM internal functions
/*
 ----------------------------------------------------------------------------------------------- */

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