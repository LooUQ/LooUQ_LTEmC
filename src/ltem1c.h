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
#include "protocols/protocols.h"
#include "atcmd.h"
#include "iop.h"


#define LTEM1_SPI_DATARATE	2000000U
#define LTEM1_PROTOCOL_INDEX_MAX 5

typedef struct
{
    int spiCsPin;
    int spiIrqPin;
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

typedef struct ltem1_modem_config_tag
{
	char imei[16];
	char iccid [21];
	char mfgmodel [21];
	char fwver [41];
	int rssi;
} ltem1_modem_config_t;


typedef struct ltem1_device_tag
{
	ltem1_pinConfig_t *pinConfig;
	sc16is741a_device_t *bridge;
	ltem1_modem_config_t *modemConfig;
	at_command_t *pendingCmd;
    iop_status_t *iopStatus;
	bool urcPending;
	ltem1_apn_t apns[2];
	protocol_session_t protocols[6];
} ltem1_device_t;

//typedef ltem1_device_t *ltem1_device;

extern ltem1_device_t *g_ltem1;
extern ltem1_pinConfig_t FEATHER_BREAKOUT;
extern ltem1_pinConfig_t RPI_BREAKOUT;



ltem1_device ltem1_init(const ltem1_pinConfig_t* ltem1_config, bool startIo, bool enableIrqMode);
void ltem1_uninit(ltem1_device ltem1);
void ltem1_dowork();

/* future static functions, open for testing */
void ltem1_powerOn(ltem1_device modem);
void ltem1_powerOff(ltem1_device modem);

void ltem1_initIO(ltem1_device ltem1, bool enableIrqMode);
/* end-static */


#ifdef __cplusplus
}
#endif // !__cplusplus



#endif  /* !__LTEM1_H__ */