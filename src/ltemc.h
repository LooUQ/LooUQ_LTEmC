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

#include "ltemc-srcfiles.h"
#include "lq-platform.h"
#include "ltemc-quectel-bg.h"
#include "ltemc-iop.h"
#include "ltemc-atcmd.h"
#include "ltemc-mdminfo.h"
#include "ltemc-network.h"
#include "ltemc-filesys.h"
#include "ltemc-gpio.h"


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


/**
 *	\brief Get the LTEmC software version.
 *  \return Version as a const char pointer.
 */
const char *ltem_ltemcVersion();


// typedef void (*eventNotifCallback_func)(uint8_t notifCode, const char *message);

/**
 *	\brief Initialize the LTEm1 modem.
 *	\param ltem_config [in] - The LTE modem gpio pin configuration.
 *  \param applicationCallback [in] - If supplied (not NULL), this function will be invoked for significant LTEm events.
 */
void ltem_create(const ltemPinConfig_t ltem_config, yield_func yieldCallback, appEventCallback_func eventNotifCallback);


/**
 *	\brief Uninitialize the LTEm device structures.
 */
void ltem_destroy();


/**
 *	\brief Power on and start the modem
 *  \param resetIfPoweredOn [in] Perform a software reset on the modem, if found in a powered on state
 */
void ltem_start(bool resetIfRunning);


/**
 *	\brief Powers off the modem without destroying memory objects. Modem device will require ltem_start() to reinit HW
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
 *	\brief Reads the hardware status and internal application ready field to return device ready state
 *  \return DeviceState: 0=power off, 1=power on, 2=appl ready
 */
qbgDeviceState_t ltem_readDeviceState();


/**
 *	\brief Background work task runner. To be called in application Loop() periodically.
 */
void ltem_doWork();


/**
 *	\brief Registers the address (void*) of your application yield callback handler.
 *  \param yieldCallback [in] Callback function in application code to be invoked when LTEmC is in await section.
 */
void ltem_setYieldCallback(yield_func yieldCallback);


/**
 *	\brief Registers the address (void*) of your application event notification callback handler.
 *  \param eventNotifCallback [in] Callback function in application code to be invoked when LTEmC is in await section.
 */
void ltem_setEventNotifCallback(appEventCallback_func eventNotifCallback);


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


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__LTEMC_H__ */