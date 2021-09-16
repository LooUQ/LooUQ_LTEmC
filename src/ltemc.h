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

#include <lq-types.h>
#include <lq-diagnostics.h>

#include "ltemc-filecodes.h"
#include "lq-platform.h"
#include "ltemc-quectel-bg.h"
#include "ltemc-iop.h"
#include "ltemc-atcmd.h"
#include "ltemc-mdminfo.h"
#include "ltemc-network.h"


/* LTEmC uses 2 global buffers: the IOP transmit buffer and the ATCMD cmd\core buffer.
 * These are sized in the ltemc-iop.h and ltemc-atcmd.h header files respectively.
 * LooUQ set defaults sizes are:
 *      IOP__txBufferSize = 1800
 *      ATCMD__commandBufferSz = 256
 */

// // optional services
// #include "ltemc-sckt.h"
// #include "ltemc-mqtt.h"
// #include "ltemc-http.h"
// #include "ltemc-gnss.h"
// #include "ltemc-geo.h"

/* ----------------------------------------------------------------------------------- */

// typedef enum ltemcOptions_tag              // binary-OR'd list of buildable options
// {
//     ltemcOptions_none = 0x0000,         
//     ltemcOptions_sockets = 0x0001,         
//     ltemcOptions_mqtt = 0x0002,
//     ltemcOptions_gnss = 0x0004,
//     ltemcOptions_geofence = 0x0008,
//     ltemcOptions_http = 0x0010,
//     ltemcOptions_file = 0x0020
// } ltemcOptions_t;



// /** 
//  *  \brief Struct representing the LTEmC model. The struct behind the g_ltem1 global variable with all driver controls.
//  * 
//  *  Most subsystems are linked to this struct with pointers to allow for better abstraction and optional subsystems
// */
// typedef struct ltemDevice_tag
// {
//     // ltem1Functionality_t funcLevel;  ///< Enum value indicating services enabled during ltemC startup.
// 	ltemPinConfig_t pinConfig;          ///< GPIO pin configuration for required GPIO and SPI interfacing.
//     spiDevice_t *spi;                   ///< SPI device (methods signatures compatible with Arduino).
//     // uint16_t faultCode;                 ///< debugging fault code set by driver ASSERT flows
//     qbgReadyState_t qbgReadyState;      ///< Ready state of the BGx module
//     appNotifyFunc_t appNotifyCB;         ///< Notification callback to application
//     uint8_t pdpContext;                 ///< The primary packet data protocol (PDP) context with the network carrier for application transfers.
//     volatile iop_t *iop;                ///< IOP subsystem controls.
//     atcmd_t *atcmd;                     ///< Action subsystem controls.
// 	modemInfo_t *modemInfo;             ///< Data structure holding persistent information about application modem state.
//     network_t *network;                 ///< Data structure representing the cellular network.
//     bool cancellationRequest;           ///< For RTOS implementations, token to request cancellation of long running task/action.

//     // /* optional services                only taking room for some pointers if not implemented */
// 	// void *sockets;                      ///< IP sockets subsystem (TCP/UDP/SSL).
//     // void (*scktWork_func)();            ///< Sockets background do work function
//     // void *mqtt;                         ///< MQTT protocol subsystem.
//     // void (*mqttWork_func)();            ///< MQTT background do work function

//     // array of function pointers to each protocol doWork(), create protoCtrl factory will init()
//     // needed for sockets, ...
// } ltemDevice_t;


// extern ltemDevice_t *g_ltem;            ///< The LTEm "object". Since this is C99, like the instance object.


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

void ltem_create(const ltemPinConfig_t ltem_config, eventNotifFunc_t eventNotifCallback);
void ltem_destroy();

void ltem_start();
void ltem_stop();
void ltem_reset();
bool ltem_chkHwReady();
qbgReadyState_t ltem_getReadyState();

void ltem_doWork();
void ltem_notifyApp(uint8_t notifyType, const char *notifyMsg);
void ltem_notifyAssert(uint16_t faultCode)   __attribute__ ((noreturn));

void ltem_setEventNotifCallback(eventNotifFunc_t eventNotifCallback);
void ltem_setYieldCallback(platform_yieldCB_func_t yieldCallback);

#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__LTEMC_H__ */