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

#include <stddef.h>
#include <stdbool.h>

// Quectel_QBG_Hardware_Design_V1.2.pdf
// #define QBG_POWERON_DELAY      500U
// #define QBG_POWEROFF_DELAY     1500U
// #define QBG_RESET_DELAY        300U
// #define QBG_BAUDRATE_DEFAULT   115200U

#define QBG_RATSEQ_AUTO    "00"
#define QBG_RATSEQ_GSM     "01"
#define QBG_RATSEQ_CATM1   "02"
#define QBG_RATSEQ_NBIOT   "03"


/** 
 *  \brief Typed numeric constants used in the BGx subsystem
 */
enum BGX__constants
{
    BGX__initCommandCnt = 1,
    BGX__initCommandAttempts = 2,
    BGX__powerOnDelay = 500,
    BGX__powerOffDelay = 1500,
    BGX__resetDelay = 300,
    BGX__baudRate = 115200
};


/** 
 *  \brief Enum describing the current BGx module state
 */
typedef enum qbgDeviceState_tag
{
    qbgDeviceState_powerOff = 0,        ///< BGx is powered off, in this state all components on the LTEm1 are powered down.
    qbgDeviceState_powerOn = 1,         ///< BGx is powered ON, while powered on the BGx may not be able to interact fully with the host application.
    qbgDeviceState_appReady = 2         ///< BGx is powered ON and ready for application/services.
} qbgDeviceState_t;



#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


/**
 * 	@brief Check for BGx power status
 *  @return True if BGx is powered ON
 */
bool qbg_isPowerOn();


/**
 *	@brief Power on the BGx module
 *  @return True if the BGx found previously on or was successfully turned ON
 */
bool qbg_powerOn();


/**
 *	@brief Powers off the BGx module
 */
void qbg_powerOff();


/**
 *	@brief Perform a hardware or software reset of the BGx module
 */
void qbg_reset(bool hwReset);


/**
 *	@brief Initializes the BGx module
 */
void qbg_setOptions();


/**
 *	@brief Attempts recovery command control of the BGx module left in data mode
 */
bool qbg_clrDataState();


/**
 *	@brief Initializes the BGx module.
 *  @return C-string representation of the module name (ex: BG96, BG95)
 */
const char *qbg_getModuleType();


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__LTEMC_QUECTEL_QBG_H__ */
