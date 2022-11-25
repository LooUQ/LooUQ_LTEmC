/******************************************************************************
 *  \file ltemc-quectel-bg.h
 *  \author Jensen Miller, Greg Terrell
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
 * Manages module genaral and non-protocol cellular radio functions
 *****************************************************************************/

#ifndef __LTEMC_QUECTEL_QBG_H__
#define __LTEMC_QUECTEL_QBG_H__

#include "ltemc-internal.h"

// #define BGX_RATSEQ_AUTO    "00"
// #define BGX_RATSEQ_GSM     "01"
// #define BGX_RATSEQ_CATM1   "02"
// #define BGX_RATSEQ_NBIOT   "03"

/* Quectel_QBG_Hardware_Design_V1.2.pdf
#define QBG_POWERON_DELAY      500U
#define QBG_POWEROFF_DELAY     1500U
#define QBG_RESET_DELAY        300U
#define QBG_BAUDRATE_DEFAULT   115200U
*/


/** 
 *  \brief Typed numeric constants used in the BGx subsystem
 */
enum BGX__constants
{
    BGX__powerOnDelay = 500,
    BGX__powerOffDelay = 1500,
    BGX__resetDelay = 500,
    BGX__baudRate = 115200
};




#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


/**
 * 	@brief Check for BGx power status
 *  @return True if BGx is powered ON
 */
bool QBG_isPowerOn();


/**
 *	@brief Power on the BGx module
 */
void QBG_powerOn();


/**
 *	@brief Powers off the BGx module
 */
void QBG_powerOff();


/**
 *	@brief Perform a hardware or software reset of the BGx module
 */
void QBG_reset(resetAction_t resetAction);


/**
 *	@brief Initializes the BGx module
 */
void QBG_setOptions();


/**
 *	@brief Attempts recovery command control of the BGx module left in data mode
 */
bool QBG_clrDataState();


/**
 *	@brief Initializes the BGx module.
 *  @return C-string representation of the module name (ex: BG96, BG95)
 */
const char *QBG_getModuleType();


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__LTEMC_QUECTEL_QBG_H__ */
