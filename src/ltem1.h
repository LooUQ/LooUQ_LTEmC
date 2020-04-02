/******************************************************************************
 *  \file crLtem1.h
 *  \author Jensen Miller, Greg Terrell, Greg Terrell
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
#ifndef __CRLTEM1_H__
#define __CRLTEM1_H__

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

#include "platform/platformGpio.h"
#include "platform/platformTiming.h"
#include "components/nxp-sc16is741a.h"

#define LTEM1_SPI_DATARATE	2000000U

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


typedef struct ltem1_device_tag
{
	ltem1_pinConfig_t* pinConfig;
	sc16is741a_device_t* bridge;
	bool urcPending;
	// platformGpioPin spiIrqPin;
	// platformGpioPin powerkeyPin;
	// platformGpioPin resetPin;
	// platformGpioPin wakePin;
	// platformGpioPin statusPin;
	// platformGpioPin connectedPin;
	// platformGpioPin ringUrcPin;
} ltem1_device_t;

typedef ltem1_device_t* ltem1_device;

extern ltem1_pinConfig_t FEATHER_BREAKOUT;
extern ltem1_pinConfig_t RPI_BREAKOUT;


ltem1_device ltem1_init(const ltem1_pinConfig_t* ltem1_config, bool startIo);
void ltem1_uninit(ltem1_device ltem1);


/* future static functions, open for testing */
void ltem1_initIO(ltem1_device ltem1);
void ltem1_powerOn(ltem1_device modem);
void ltem1_powerOff(ltem1_device modem);
/* end-static */


#ifdef __cplusplus
}
#endif // !__cplusplus



#endif  /* !__CRLTEM1_H__ */