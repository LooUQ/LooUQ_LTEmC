/******************************************************************************
 *  \file ltemc.h
 *  \author Greg Terrell, Jensen Miller
 *  \license MIT License
 *
 *  Copyright (c) 2020,2021 LooUQ Incorporated.
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

#ifndef __LTEMC_H__
#define __LTEMC_H__


// Internal static buffers you may need to change for your application. Contact LooUQ for details.
// #define IOP_RX_COREBUF_SZ 256
// #define IOP_TX_BUFFER_SZ 1460

#include <lq-types.h>                           /// LooUQ embedded device library typedefs, common across products/libraries
#include <lq-diagnostics.h>                     /// ASSERT and diagnostic data collection
// #include "ltemc-srcfiles.h"                     /// source file manifest for ASSERT use

#include "ltemc-types.h"                        /// type definitions for LTEm device driver: LTEmC

#include "lq-platform.h"                        /// platform abstractions (arduino, etc.)
#include "ltemc-atcmd.h"                        /// command processor interface
#include "ltemc-mdminfo.h"                      /// modem information
#include "ltemc-network.h"                      /// cellular provider and packet network 

/* Add the following LTEmC feature sets as required for your project
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
 */
/* ----------------------------------------------------------------------------------- */


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
void ltem_create(const ltemPinConfig_t ltem_config, yield_func yieldCB, applEvntNotify_func eventNotifyCB);


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
void ltem_doWork();


/**
 *	\brief Registers the address (void*) of your application yield callback handler.
 *  \param yieldCallback [in] Callback function in application code to be invoked when LTEmC is in await section.
 */
void ltem_setYieldCallback(yield_func yieldCB);


/**
 *	\brief Registers the address (void*) of your application event notification callback handler.
 *  \param eventNotifCallback [in] Callback function in application code to be invoked when LTEmC is in await section.
 */
void ltem_setEventNotifCallback(applEvntNotify_func eventNotifyCB);


/**
 *	\brief Function of last resort, catastrophic failure Background work task runner. To be called in application Loop() periodically.
 *  \param notifyType [in] - Enum of broad notification categories.
 *  \param notifyMsg [in] - Message from origination about the issue being reported.
 */
void ltem_notifyApp(uint8_t notifyType, const char *notifyMsg);


// /**
//  *	\brief Function of last resort, catastrophic failure Background work task runner. To be called in application Loop() periodically.
//  *  \param faultCode [in] - HTTP style error code.
//  */
// void ltem_notifyAssert(uint16_t faultCode)   __attribute__ ((noreturn));



// LTEM Internal
void LTEM_initIo();
void LTEM_registerDoWorker(doWork_func *doWorker);



#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__LTEMC_H__ */