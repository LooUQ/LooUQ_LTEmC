/******************************************************************************
 *  \file ltem1.h
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
#ifndef __LTEM1_H__
#define __LTEM1_H__

#ifdef __cplusplus
extern "C"
{
#include <cstdint>
#include <cstdlib>
#include <cstdbool>
#else
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#endif // __cplusplus

/* Capabilities for build defined here globally */
// Comment out any function not utilized in your applications
#define LTEM1_SOCKETS
#define LTEM1_MQTT
//#define LTEM1_HTTP
//#define LTEM1_FTP
#define LTEM1_GNSS
#define LTEM1_GEOFENCE
//#define LTEM1_FILES
//#define LTEM1_FOTA

#define LTEM1_SOCKET_COUNT 6
#define LTEM1_SPI_DATARATE 2000000U


#define ASCII_cCR '\r'
#define ASCII_sCR "\r"
#define ASCII_cCOMMA ','
#define ASCII_cNULL '\0'
#define ASCII_cDBLQUOTE '\"'
#define ASCII_cHYPHEN '-'
#define ASCII_cSPACE ' '
#define ASCII_sCTRLZ "\032"
#define ASCII_sCRLF "\r\n"
#define ASCII_sOK "OK\r\n"
#define ASCII_sMQTTTERM "\"\r\n"
#define ASCII_szCRLF 2

#define NOT_NULL 1

// to allow for BGxx error codes starting at 500 to be passed back to application
// ltem1c uses macros and the action_result_t typedef

#define RESULT_CODE_PENDING         0
#define RESULT_CODE_SUCCESS       200

#define RESULT_CODE_ERRORS_BASE   400
#define RESULT_CODE_BADREQUEST    400
#define RESULT_CODE_FORBIDDEN     403
#define RESULT_CODE_NOTFOUND      404
#define RESULT_CODE_TIMEOUT       408
#define RESULT_CODE_CONFLICT      409
#define RESULT_CODE_GONE          410
#define RESULT_CODE_PRECONDFAILED 412
#define RESULT_CODE_CANCELLED     499
#define RESULT_CODE_ERROR         500
#define RESULT_CODE_UNAVAILABLE   503
#define RESULT_CODE_CUSTOMBASE    900

// action_result_t should be populated with RESULT_CODE_x constant values or an errorCode (uint >= 400)
typedef uint16_t resultCode_t;

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
#include "gnss.h"
#include "network.h"
#include "sockets.h"
#include "mqtt.h"


/** 
 *  \brief Enum representing the service functionality to turn-on during LTEm1 initialization.
*/
typedef enum 
{
    ltem1Functionality_stop = 0,        ///< Default state, no services set to start.
    ltem1Functionality_base = 1,        ///< Basic services only, GPIO, SPI Bridge, BGx
    ltem1Functionality_iop = 2,         ///< Start IOP (Input/Output Processor)
    ltem1Functionality_actions = 3,     ///< Start actions services (AT commands)
    ltem1Functionality_services = 4     ///< Start application services (protocols: sockets[TCP\UDP\SSL], MQTT, HTTP, etc.)
} ltem1Functionality_t;


/** 
 *  \brief Enum indicating desired state for LTEm1 hardware power-state at initialization.
*/
typedef enum ltem1Start_tag
{
    ltem1Start_powerOff = 0,            ///< At initialization, leave LTEm1 powered down
    ltem1Start_powerOn = 1              ///< At initialization, power LTEm1 ON
} ltem1Start_t;


/** 
 *  \brief Enum indicating desirec state for LTEm1 hardware power-state at initialization.
*/
typedef enum ltem1SpiError_tag
{
    ltem1SpiError_none = 0,
    ltem1SpiError_CommFailure = 1
} ltem1SpiError_t;


/** 
 *  \brief Struct representing the LTEm1c. The struct behind the g_ltem1 global variable with all driver controls.
 * 
 *  Most subsystems are linked to this struct with pointers to allow for better abstraction and optional subsystems
*/
typedef struct ltem1Device_tag
{
    ltem1Functionality_t funcLevel;     ///< Enum value indicating services enabled during ltem1c startup.
	ltem1PinConfig_t pinConfig;         ///< GPIO pin configuration for required GPIO and SPI interfacing.
    spiDevice_t *spi;                   ///< SPI device (methods signatures compatible with Arduino).
    qbgReadyState_t qbgReadyState;      ///< Ready state of the BGx module
    ltem1SpiError_t mdmSpiError;        ///< Error indicates problems with local (SPI) communications between host and modem.
    uint8_t dataContext;                ///< The primary APN context with the network carrier for application transfers.
    volatile iop_t *iop;                ///< IOP subsystem controls.
    action_t *action;                   ///< Action subsystem controls.
    bool cancellationRequest;           ///< For RTOS implementations, token to request cancellation of long running task\action.
	modemInfo_t *modemInfo;             ///< Data structure holding persistent information about application modem state.
    network_t *network;                 ///< Data structure representing the cellular network.
	volatile sockets_t *sockets;        ///< IP sockets subsystem (TCP\UDP\SSL).
    mqtt_t *mqtt;                       ///< MQTT protocol subsystem.
} ltem1Device_t;


extern ltem1Device_t *g_ltem1;              ///< The LTEm1 "object", since this is C99 like the instance class.
extern char g_dbgBuffer[DBGBUFFER_SZ];      ///< global shared buffer for debug output
extern ltem1PinConfig_t FEATHER_BREAKOUT;   ///< obsolete
extern ltem1PinConfig_t RPI_BREAKOUT;       ///< obsolete


void ltem1_create(const ltem1PinConfig_t ltem1_config, ltem1Start_t ltem1Start, ltem1Functionality_t funcLevel);
void ltem1_destroy();

void ltem1_start();
void ltem1_stop();
void ltem1_reset();

void ltem1_doWork();
void ltem1_faultHandler(uint16_t statusCode, const char * fault) __attribute__ ((noreturn));
void ltem1_setYieldCb(platform_yieldCB_func_t yieldCb_func);


#ifdef __cplusplus
}
#endif // !__cplusplus



#endif  /* !__LTEM1_H__ */