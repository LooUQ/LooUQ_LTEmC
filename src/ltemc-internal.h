/******************************************************************************
 *  \file ltemc-internal.h
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
 ******************************************************************************
 * Internal define and type definitions for LTEmC modules.
 *****************************************************************************/

#ifndef __LTEMC_INTERNAL_H__
#define __LTEMC_INTERNAL_H__


// Internal static buffers you may need to change for your application. Consult with LooUQ for details.
// #define IOP_RX_COREBUF_SZ 256
// #define IOP_TX_BUFFER_SZ 1460

#include <lq-types.h>
#include <lq-diagnostics.h>
#include "ltemc-srcfiles.h"

#include "lq-platform.h"
#include "ltemc-nxp-sc16is.h"
#include "ltemc-quectel-bg.h"
#include "ltemc-iop.h"
#include "ltemc-atcmd.h"
#include "ltemc-mdminfo.h"
#include "ltemc-network.h"
#include "ltemc-streams.h"

// // optional services
// #include "ltemc-sckt.h"
// #include "ltemc-mqtt.h"
// #include "ltemc-http.h"
// #include "ltemc-gnss.h"
// #include "ltemc-geo.h"

/* LTEmC global fields/properties as Singleton 
 * Kept internal here, future support for multiple LTEmC instances would replace this with function calls to serve instances
 * ------------------------------------------------------------------------------------------------------------------------------*/

 /** 
 *  \brief Struct representing the LTEmC model. The struct behind the g_ltem1 global variable with all driver controls.
 * 
 *  Most subsystems are linked to this struct with pointers to allow for better abstraction and optional subsystems
 */
typedef struct ltemDevice_tag
{
    // ltem1Functionality_t funcLevel;      ///< Enum value indicating services enabled during ltemC startup.
	ltemPinConfig_t pinConfig;              ///< GPIO pin configuration for required GPIO and SPI interfacing.
    bool cancellationRequest;               ///< For RTOS implementations, token to request cancellation of long running task/action.
    qbgDeviceState_t qbgDeviceState;        ///< Device state of the BGx module
    appEventCallback_func appEventCB;       ///< Event notification callback to parent application
    uint8_t instNm;                         ///< LTEm instance number 0=undefined, 1..254
    char moduleType[8];                     ///< c-str indicating module type. BG96, BG95-M3 (so far)

    void *spi;                              ///< SPI device (methods signatures compatible with Arduino).
    void *pdpContext;                       ///< The primary packet data protocol (PDP) context with the network carrier for application transfers.
    void *iop;                              ///< IOP subsystem controls.
    void *atcmd;                            ///< Action subsystem controls.
	void *modemInfo;                        ///< Data structure holding persistent information about application modem state.
    void *network;                          ///< Data structure representing the cellular network.

    moduleDoWorkFunc_t streamWorkers[6];    ///< Stream background doWork functions, registered by Open;



    // spiDevice_t *spi;                   ///< SPI device (methods signatures compatible with Arduino).
    // // uint16_t faultCode;                 ///< debugging fault code set by driver ASSERT flows
    // appNotifyFunc_t appNotifyCB;         ///< Notification callback to application
    // uint8_t pdpContext;                 ///< The primary packet data protocol (PDP) context with the network carrier for application transfers.
    // void *iop;                ///< IOP subsystem controls.
    // atcmd_t *atcmd;                     ///< Action subsystem controls.
	// modemInfo_t *modemInfo;             ///< Data structure holding persistent information about application modem state.
    // network_t *network;                 ///< Data structure representing the cellular network.

    // /* optional services                only taking room for some pointers if not implemented */
	// void *sockets;                      ///< IP sockets subsystem (TCP/UDP/SSL).
    // void (*scktWork_func)();            ///< Sockets background do work function
    // void *mqtt;                         ///< MQTT protocol subsystem.
    // void (*mqttWork_func)();            ///< MQTT background do work function

    // array of function pointers to each protocol doWork(), create protoCtrl factory will init()
    // needed for sockets, ...
} ltemDevice_t;

extern ltemDevice_t g_ltem;            ///< The LTEm "object".

/* ------------------------------------------------------------------------------------------------------------------------------*/


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

// LTEM Internal
void LTEM_initIo();
void LTEM_registerDoWorker(moduleDoWorkFunc_t *doWorker);

// IOP Internal 
void IOP_rxParseForEvents();            // atcmd dependency

#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__LTEMC_INTERNAL_H__ */