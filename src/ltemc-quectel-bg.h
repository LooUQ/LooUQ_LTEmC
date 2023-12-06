/** ***************************************************************************
  @file 
  @brief BGx module functions/services.

  @author Greg Terrell, LooUQ Incorporated

  \loouq

  @warning Internal dependencies, changes only as directed by LooUQ staff.

-------------------------------------------------------------------------------

LooUQ-LTEmC // Software driver for the LooUQ LTEm series cellular modems.
Copyright (C) 2017-2023 LooUQ Incorporated

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
Also add information on how to contact you by electronic and paper mail.

**************************************************************************** */


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
    BGX__powerOffDelay = 800,
    BGX__resetDelay = 800,
    BGX__baudRate = 115200,
    BGX__startVerifyResponseSz = 17
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
bool QBG_setOptions();


/**
 *	@brief Attempts recovery command control of the BGx module left in data mode
 */
bool QBG_clearDataState();


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__LTEMC_QUECTEL_QBG_H__ */
