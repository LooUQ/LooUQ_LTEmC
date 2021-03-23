/******************************************************************************
 *  \file ltem1c.h
 *  \author Greg Terrell, Jensen Miller
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
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
#ifndef __LTEM1C_H__
#define __LTEM1C_H__

#ifdef __cplusplus
extern "C"
#include <cstdint>
#include <cstdlib>
#include <cstdbool>
#else
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#endif // __cplusplus

#include "lqgtypes.h"

#include "platform/platform_debug.h"

#include "platform/platform_pins.h"
#include "platform/platform_gpio.h"
#include "platform/platform_timing.h"
#include "platform/platform_spi.h"

#include "nxp_sc16is741a.h"
#include "quectel_bg.h"
#include "iop.h"
#include "actions.h"
#include "mdminfo.h"
#include "network.h"

/* Optional services
 ------------------------------------------------------------------------------------- */
#include "sockets.h"
#include "mqtt.h"
#include "gnss.h"
#include "geo.h"
/* ----------------------------------------------------------------------------------- */


/** 
 *  \brief Struct representing the LTEm1c model. The struct behind the g_ltem1 global variable with all driver controls.
 * 
 *  Most subsystems are linked to this struct with pointers to allow for better abstraction and optional subsystems
*/
typedef struct ltem1Device_tag
{
    // ltem1Functionality_t funcLevel;  ///< Enum value indicating services enabled during ltem1c startup.
	ltem1PinConfig_t pinConfig;         ///< GPIO pin configuration for required GPIO and SPI interfacing.
    spiDevice_t *spi;                   ///< SPI device (methods signatures compatible with Arduino).
    qbgReadyState_t qbgReadyState;      ///< Ready state of the BGx module
    appNotify_func appNotifyCB;         ///< Notification callback to application
    uint8_t dataContext;                ///< The primary APN context with the network carrier for application transfers.
    volatile iop_t *iop;                ///< IOP subsystem controls.
    action_t *action;                   ///< Action subsystem controls.
    bool cancellationRequest;           ///< For RTOS implementations, token to request cancellation of long running task\action.
	modemInfo_t *modemInfo;             ///< Data structure holding persistent information about application modem state.
    network_t *network;                 ///< Data structure representing the cellular network.

    /* optional services */
	void *sockets;                      ///< IP sockets subsystem (TCP\UDP\SSL).
    void (*scktWork_func)();            ///< Sockets background do work function
    void *mqtt;                         ///< MQTT protocol subsystem.
    void (*mqttWork_func)();            ///< MQTT background do work function
} ltem1Device_t;


extern ltem1Device_t *g_ltem1;              ///< The LTEm1 "object", since this is C99 like the instance class.


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

void ltem1_create(const ltem1PinConfig_t ltem1_config, appNotify_func appNotifyCB);
void ltem1_destroy();

void ltem1_start();
void ltem1_stop();
void ltem1_reset();
bool ltem1_chkHwReady();
qbgReadyState_t ltem1_getReadyState();

void ltem1_doWork();
void ltem1_notifyApp(uint8_t notifyType, const char *notifyMsg);
void ltem1_setYieldCb(platform_yieldCB_func_t yieldCb_func);


#ifdef __cplusplus
}
#endif // !__cplusplus



#endif  /* !__LTEM1C_H__ */