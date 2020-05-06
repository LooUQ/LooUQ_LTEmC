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
#include "platform/platform_stdio.h"
#include "platform/platform_spi.h"
#include "components/nxp_sc16is741a.h"
#include "components/quectel_bg.h"
#include "util.h"
#include "iop.h"
#include "actions.h"
#include "mdminfo.h"
#include "gnss.h"
#include "protocols/protocols.h"
#include "protocols/ip.h"


#define LTEM1_SPI_DATARATE	2000000U


#define ASCII_cCR '\r'
#define ASCII_sCR "\r"
#define ASCII_cCOMMA ','
#define ASCII_cNULL '\0'
#define ASCII_cDBLQUOTE '\"'
#define ASCII_cHYPHEN '-'
#define ASCII_cSPACE ' '
#define ASCII_sCRLF "\r\n"
#define ASCII_sOK "OK\r\n"
#define ASCII_szCRLF 2



typedef enum 
{
    ltem1_functionality_stop = 0,
    ltem1_functionality_base = 1,
    ltem1_functionality_iop = 2,
    ltem1_functionality_atcmd = 3,
    ltem1_functionality_services = 4
} ltem1_functionality_t;


typedef struct
{
    int spiCsPin;
    int irqPin;
    int statusPin;
    int powerkeyPin;
    int resetPin;
    int ringUrcPin;
    int wakePin;
} ltem1_pinConfig_t;


typedef struct ltem1_device_tag
{
    ltem1_functionality_t funcLevel;
	ltem1_pinConfig_t *gpio;
    spi_device_t *spi;
    qbg_readyState_t qbgReadyState;
    uint8_t dataContext;
    iop_t *iop;
	action_t *dAction;
    action_t *pendAction;
	modemInfo_t *modemInfo;
    network_t *network;
	protocols_t *protocols;
} ltem1_device_t;


extern ltem1_device_t *g_ltem1;
extern ltem1_pinConfig_t FEATHER_BREAKOUT;
extern ltem1_pinConfig_t RPI_BREAKOUT;


void ltem1_create(const ltem1_pinConfig_t* ltem1_config, ltem1_functionality_t funcLevel);
void ltem1_destroy();

void ltem1_start(ltem1_functionality_t funcLevel);
void ltem1_stop();
void ltem1_reset(bool restart);

void ltem1_dowork();
void ltem1_faultHandler(const char * fault) __attribute__ ((noreturn));


#ifdef __cplusplus
}
#endif // !__cplusplus



#endif  /* !__LTEM1_H__ */