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
#include "components/quectel_bg96.h"
#include "iop.h"
#include "atcmd.h"
#include "mdminfo.h"
#include "gnss.h"
#include "util.h"

// #include "protocols/protocols.h"


#define LTEM1_SPI_DATARATE	2000000U
#define LTEM1_PROTOCOL_COUNT 6


#define ASCII_cCR '\r'
#define ASCII_cCOMMA ','
#define ASCII_sCRLF "\r\n"
#define ASCII_sOKTERM "OK\r\n"

#define ASCII_szCRLF 2



typedef enum 
{
    ltem1_functionality_stop = 0,
    ltem1_functionality_base = 1,
    ltem1_functionality_iop = 2,
    ltem1_functionality_atcmd = 3,
    ltem1_functionality_full = 4
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


typedef struct ltem1_apn_tag
{
	uint8_t apn_index;
	char apn_name[21];
	uint32_t ipAddress;
} ltem1_apn_t;


typedef struct ltem1_modemInfo_tag
{
	char imei[16];
	char iccid [21];
	char mfgmodel [21];
	char fwver [41];
} ltem1_modemInfo_t;


typedef struct ltem1_device_tag
{
    bg96_readyState_t bg96ReadyState;
    ltem1_functionality_t funcLevel;
	ltem1_pinConfig_t *gpio;
    spi_device_t *spi;
	ltem1_modemInfo_t *modemInfo;
    iop_state_t *iop;
	atcmd_t *atcmd;
    atcmd_t *pendingCmd;
	ltem1_apn_t apns[2];
	// protocol_t protocols[6];
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