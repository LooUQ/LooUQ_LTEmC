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

typedef enum
{
    qbg_readyState_powerOff = 0,
    qbg_readyState_powerOn = 1,
    qbg_readyState_appReady = 2
} qbg_readyState_t;


typedef enum
{
    qbg_nw_scan_mode_auto = 0U,
    qbg_nw_scan_mode_gsmonly = 1U,
    qbg_nw_scan_mode_lteonly = 3U
} qbg_nw_scan_mode_t;


typedef enum
{
    qbg_nw_iot_mode_m1 = 0U,
    qbg_nw_iot_mode_nb1 = 1U,
    qbg_nw_iot_mode_m1nb1 = 2U
} qbg_nw_iot_mode_t;


// void qbg_create();
// void qbg_destroy();

void qbg_start();
// void qbg_stop();

void qbg_powerOn();
void qbg_powerOff();

void qbg_setNwScanSeq(const char *sequence);
void qbg_setNwScanMode(qbg_nw_scan_mode_t mode);
void qbg_setIotOpMode(qbg_nw_iot_mode_t mode);

//void qbg_queueUrcStateMsg(const char *message);
void qbg_processUrcStateQueue();

#endif  /* !__QUECTEL_QBG_H__ */
