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

#define LTEmC_VERSION "3.0.2"


// Internal static buffers you may need to change for your application. Contact LooUQ for details.
// #define IOP_RX_COREBUF_SZ 256
// #define IOP_TX_BUFFER_SZ 1460

#include <lq-types.h>                           /// LooUQ embedded device library typedefs, common across products/libraries
#include <lq-diagnostics.h>                     /// ASSERT and diagnostic data collection

#include "lq-platform.h"                        /// platform abstractions (arduino, etc.)

#include "ltemc-platformPins.h"                 /// LTEmC host-pin configuration
#include "ltemc-types.h"                        /// type definitions for LTEm device driver: LTEmC
#include "ltemc-atcmd.h"                        /// command processor interface
#include "ltemc-mdminfo.h"                      /// modem information
#include "ltemc-network.h"                      /// cellular provider and packet network 

/* Add the following LTEmC feature sets AS REQUIRED for your project
*/
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
 * 
 * These sizes can be adjusted WITH CARE for specific implementations.
 *
 * ----------------------------------------------------------------------------------- */


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


// typedef void (*eventNotifCallback_func)(uint8_t notifCode, const char *message);

/**
 *	\brief Initialize the LTEm1 modem.
 *	\param ltem_config [in] - The LTE modem gpio pin configuration.
 *  \param applicationCallback [in] - If supplied (not NULL), this function will be invoked for significant LTEm events.
 */
void ltem_create(const ltemPinConfig_t ltem_config, yield_func yieldCB, appEvntNotify_func eventNotifyCB);


/**
 *	\brief Uninitialize the LTEm device structures.
 */
void ltem_destroy();

/**
 *  \brief Configure RAT searching sequence
 *  \details Example: scanSequence = "020301" represents: search LTE-M1, then LTE-NB1, then GSM
 *  \param [in] scanSequence Character string specifying the RAT scanning order; 00=Automatic[LTE-M1|LTE-NB1|GSM],01=GSM,02=LTE-M1,03=LTE-NB1
*/
void ltem_setProviderScanSeq(const char *sequence);


/** 
 *  \brief Configure RAT(s) allowed to be searched
 *  \param [in] scanMode Enum specifying what cell network to scan; 0=Automatic,1=GSM only,3=LTE only
*/
void ltem_setProviderScanMode(ntwkScanMode_t mode);


/** 
 *  \brief Configure the network category to be searched under LTE RAT.
 *  \param [in] iotMode Enum specifying the LTE LPWAN protocol(s) to scan; 0=LTE M1,1=LTE NB1,2=LTE M1 and NB1
 */
void ltem_setIotMode(ntwkIotMode_t mode);


/**
 *	\brief Build default data context configuration for modem to use on startup.
 *  \param [in] cntxtId The context ID to operate on. Typically 0 or 1
 *  \param [in] protoType The PDP protocol IPV4, IPV6, IPV4V6 (both).
 *  \param [in] apn The APN name if required by network carrier.
 */
void ltem_setDefaultNetwork(uint8_t pdpContextId, const char *protoType, const char *apn);


/**
 *	\brief Power on and start the modem
 *  \param resetIfPoweredOn [in] Perform a software reset on the modem, if found in a powered on state
 */
void ltem_start(resetAction_t resetAction);


/**
 *	\brief Powers off the modem without destroying memory objects. Power modem back on with ltem_start()
 */
void ltem_stop();


// /**
//  *	\brief Performs a software restart of LTEm1.
//  *  \param coldStart [in] Set to true if the LTEm is being hard started, from a reset/power ON
//  */
// void ltem_initDevice(bool coldStart);


/**
 *	\brief Performs a reset of LTEm.
 */
void ltem_reset(bool hardReset);

/**
 *	\brief Get the LTEmC software version.
 *  \return Version as a const char pointer.
 */
const char *ltem_getSwVersion();


/**
 *	\brief Reads the hardware status and internal application ready field to return device ready state
 *  \return DeviceState: 0=power off, 1=power on, 2=appl ready
 */
deviceState_t ltem_getDeviceState();


/**
 *	\brief Background work task runner. To be called in application Loop() periodically.
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
 *	\brief Registers the address (void*) of your application yield callback handler.
 *  \param yieldCallback [in] Callback function in application code to be invoked when LTEmC is in await section.
 */
void ltem_setYieldCallback(yield_func yieldCB);


/**
 *	\brief Registers the address (void*) of your application event notification callback handler.
 *  \param eventNotifCallback [in] Callback function in application code to be invoked when LTEmC is in await section.
 */
void ltem_setEventNotifCallback(appEvntNotify_func eventNotifyCB);


/**
 *	\brief Function of last resort, catastrophic failure Background work task runner. To be called in application Loop() periodically.
 *  \param notifyType [in] - Enum of broad notification categories.
 *  \param notifyMsg [in] - Message from origination about the issue being reported.
 */
void ltem_notifyApp(uint8_t notifyType, const char *notifyMsg);


#pragma region LTEM internal functions
/*
 ----------------------------------------------------------------------------------------------- */

// uint8_t LTEM__getStreamIndx(dataCntxt_t dataCntxt);


// /**
//  *	\brief Function of last resort, catastrophic failure Background work task runner. To be called in application Loop() periodically.
//  *  \param faultCode [in] - HTTP style error code.
//  */
// void ltem_notifyAssert(uint16_t faultCode)   __attribute__ ((noreturn));

#pragma endregion

#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__LTEMC_H__ */