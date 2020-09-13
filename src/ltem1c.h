/******************************************************************************
 *  \file ltem1.h
 *  \author Jensen Miller, Greg Terrell
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

#include "platform/platform_gpio.h"
#include "platform/platform_timing.h"
//#include "platform/platform_stdio.h"
#include "platform/platform_spi.h"
#include "nxp_sc16is741a.h"
#include "quectel_bg.h"
#include "util.h"


#define LTEM1_SPI_DATARATE	2000000U

#define ASCII_cCR '\r'
#define ASCII_sCR "\r"
#define ASCII_cCOMMA ','
#define ASCII_cNULL '\0'
#define ASCII_cDBLQUOTE '\"'
#define ASCII_cHYPHEN '-'
#define ASCII_cSPACE ' '
#define ASCII_sCTRLZ "\x1a"
#define ASCII_sCRLF "\r\n"
#define ASCII_sOK "OK\r\n"
#define ASCII_sMQTTTERM "\"\r\n"
#define ASCII_szCRLF 2

#define NOT_NULL 1

// to allow for BGxx error codes starting at 500 to be passed back to application
// ltem1c uses macros and the action_result_t typedef

#define ACTION_RESULT_PENDING         0
#define ACTION_RESULT_SUCCESS       200

#define ACTION_RESULT_ERRORS_BASE   400
#define ACTION_RESULT_BADREQUEST    400
#define ACTION_RESULT_NOTFOUND      404
#define ACTION_RESULT_TIMEOUT       408
#define ACTION_RESULT_CONFLICT      409
#define ACTION_RESULT_ERROR         500

#define ACTION_RESULT_BGERRORS_BASE 600


// action_result_t should be populated with ACTION_RESULT_x constant values or an errorCode (uint >= 400)
typedef uint16_t actionResult_t;

#include "iop.h"
#include "actions.h"
#include "mdminfo.h"
#include "gnss.h"
#include "network.h"
#include "ip.h"
#include "mqtt.h"


typedef enum 
{
    ltem1Functionality_stop = 0,
    ltem1Functionality_base = 1,
    ltem1Functionality_iop = 2,
    ltem1Functionality_actions = 3,
    ltem1Functionality_services = 4
} ltem1Functionality_t;


typedef enum
{
    ltem1Start_powerOff = 0,
    ltem1Start_powerOn = 1
} ltem1Start_t;


typedef struct
{
    int spiCsPin;
    int irqPin;
    int statusPin;
    int powerkeyPin;
    int resetPin;
    int ringUrcPin;
    int wakePin;
} ltem1PinConfig_t;


typedef struct ltem1Device_tag
{
    ltem1Functionality_t funcLevel;
	ltem1PinConfig_t *gpio;
    spiDevice_t *spi;
    qbgReadyState_t qbgReadyState;
    uint8_t dataContext;
    bool cancellationRequest;
    volatile iop_t *iop;
    volatile action_t *action;
	modemInfo_t *modemInfo;
    network_t *network;
	protocols_t *protocols;
    mqtt_t *mqtt;
} ltem1Device_t;


extern ltem1Device_t *g_ltem1;
extern ltem1PinConfig_t FEATHER_BREAKOUT;
extern ltem1PinConfig_t RPI_BREAKOUT;


void ltem1_create(const ltem1PinConfig_t* ltem1_config, ltem1Start_t ltem1Start, ltem1Functionality_t funcLevel);
void ltem1_destroy();

void ltem1_start();
void ltem1_stop();
void ltem1_reset();

void ltem1_doWork();
void ltem1_faultHandler(uint16_t statusCode, const char * fault) __attribute__ ((noreturn));


#ifdef __cplusplus
}
#endif // !__cplusplus



#endif  /* !__LTEM1_H__ */