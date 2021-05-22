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

#include "lqTypes.h"



#include "lqPlatform.h"
#include "ltemc-nxp-sc16is.h"
#include "ltemc-quectel-bg.h"

#include "ltemc-iop.h"
#include "ltemc-atcmd.h"
#include "ltemc-mdminfo.h"
#include "ltemc-network.h"

/* Optional services
 ------------------------------------------------------------------------------------- */
#include "ltemc-sockets.h"
#include "ltemc-mqtt.h"
//#include "ltemc-http.h"

#include "ltemc-gnss.h"
#include "ltemc-geo.h"

#include <ltemc-filesys.h>
/* ----------------------------------------------------------------------------------- */


/** 
 *  \brief Struct representing the LTEm1c model. The struct behind the g_ltem1 global variable with all driver controls.
 * 
 *  Most subsystems are linked to this struct with pointers to allow for better abstraction and optional subsystems
*/
typedef struct ltemDevice_tag
{
    // ltem1Functionality_t funcLevel;  ///< Enum value indicating services enabled during ltem1c startup.
	ltemPinConfig_t pinConfig;          ///< GPIO pin configuration for required GPIO and SPI interfacing.
    spiDevice_t *spi;                   ///< SPI device (methods signatures compatible with Arduino).
    qbgReadyState_t qbgReadyState;      ///< Ready state of the BGx module
    appNotify_func appNotifyCB;         ///< Notification callback to application
    uint8_t dataContext;                ///< The primary APN context with the network carrier for application transfers.
    volatile iop_t *iop;                ///< IOP subsystem controls.
    atcmd_t *atcmd;                     ///< Action subsystem controls.
    bool cancellationRequest;           ///< For RTOS implementations, token to request cancellation of long running task\action.
	modemInfo_t *modemInfo;             ///< Data structure holding persistent information about application modem state.
    network_t *network;                 ///< Data structure representing the cellular network.

    /* optional services                only taking room for some pointers if not implemented */
	void *sockets;                      ///< IP sockets subsystem (TCP\UDP\SSL).
    void (*scktWork_func)();            ///< Sockets background do work function
    void *mqtt;                         ///< MQTT protocol subsystem.
    void (*mqttWork_func)();            ///< MQTT background do work function
} ltemDevice_t;


extern ltemDevice_t *g_ltem;            ///< The LTEm "object". Since this is C99, like the instance object.


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

void ltem_create(const ltemPinConfig_t ltem_config, appNotify_func appNotifyCB);
void ltem_destroy();

void ltem_start(uint16_t protocolBitMap);
void ltem_stop();
void ltem_reset();
bool ltem_chkHwReady();
qbgReadyState_t ltem_getReadyState();

void ltem_doWork();
void ltem_notifyApp(uint8_t notifyType, const char *notifyMsg);
void ltem_setYieldCb(platform_yieldCB_func_t yieldCb_func);

// semi-private functions, not intended for most application but not static for special needs
void ltem__initIo();


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__LTEMC_H__ */