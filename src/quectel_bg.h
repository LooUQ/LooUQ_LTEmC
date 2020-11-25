/******************************************************************************
 *  \file quectel_qbg.h
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


#include <stddef.h>

#ifndef __QUECTEL_QBG_H__
#define __QUECTEL_QBG_H__


// Quectel_QBG_Hardware_Design_V1.2.pdf
#define QBG_POWERON_DELAY      500U
#define QBG_POWEROFF_DELAY     1500U
#define QBG_RESET_DELAY        300U
#define QBG_BAUDRATE_DEFAULT   115200U

#define QBG_RATSEQ_AUTO    "00"
#define QBG_RATSEQ_GSM     "01"
#define QBG_RATSEQ_CATM1   "02"
#define QBG_RATSEQ_NBIOT   "03"


/** 
 *  \brief Enum describing the current BGx module state.
*/
typedef enum
{
    qbg_readyState_powerOff = 0,        ///< BGx is powered off, in this state all components on the LTEm1 are powered down.
    qbg_readyState_powerOn = 1,         ///< BGx is powered ON, while powered on the BGx may not be able to interact fully with the host application.
    qbg_readyState_appReady = 2         ///< BGx is powered ON and ready for application/services.
} qbgReadyState_t;


/** 
 *  \brief Enum describing the mode the BGx module is using to look for available networks (carriers).
*/
typedef enum
{
    qbg_nw_scan_mode_auto = 0U,         ///< BGx is considering either GSM or LTE carrier connections.
    qbg_nw_scan_mode_gsmonly = 1U,      ///< GSM only mode: BGx is filtering visible networks and only considering connections to GSM endpoints.
    qbg_nw_scan_mode_lteonly = 3U       ///< LTE only mode: BGx is filtering visible networks and only considering connections to LTE endpoints.
} qbg_nw_scan_mode_t;


/** 
 *  \brief Enum describing the available options for an IoT protocol when connecting to the network.
*/
typedef enum
{
    qbg_nw_iot_mode_m1 = 0U,            ///< CAT-M1 only mode: BGx is filtering visible networks and only considering CAT-M1 connections.
    qbg_nw_iot_mode_nb1 = 1U,           ///< NB-IOT only mode: BGx is filtering visible networks and only considering NB-IOT connections.
    qbg_nw_iot_mode_m1nb1 = 2U          ///< The BGx will connect to either a CAT-M1 or NB-IOT network.
} qbg_nw_iot_mode_t;


void qbg_start();

void qbg_powerOn();
void qbg_powerOff();
void qbg_reset();

void qbg_setNwScanSeq(const char *sequence);
void qbg_setNwScanMode(qbg_nw_scan_mode_t mode);
void qbg_setIotOpMode(qbg_nw_iot_mode_t mode);


#endif  /* !__QUECTEL_QBG_H__ */
