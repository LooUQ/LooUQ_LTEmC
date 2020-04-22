/******************************************************************************
 *  \file quectel_bg96.h
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

#ifndef __QUECTEL_BG96_H__
#define __QUECTEL_BG96_H__


// Quectel_BG96_Hardware_Design_V1.2.pdf
#define BG96_POWERON_DELAY      500U
#define BG96_POWEROFF_DELAY     1500U
#define BG96_RESET_DELAY        400U
#define BG96_BAUDRATE_DEFAULT   115200U

#define BG96_RATSEQ_AUTO    "00"
#define BG96_RATSEQ_GSM     "01"
#define BG96_RATSEQ_CATM1   "02"
#define BG96_RATSEQ_NBIOT   "03"

typedef enum
{
    bg96_readyState_powerOff = 0,
    bg96_readyState_powerOn = 1,
    bg96_readyState_appReady = 2
} bg96_readyState_t;


typedef enum
{
    BG96_NW_SCAN_MODE_AUTO = 0U,
    BG96_NW_SCAN_MODE_GSMONLY = 1U,
    BG96_NW_SCAN_MODE_LTEONLY = 3U
} bg96_nw_scan_mode_t;


typedef enum
{
    BG96_NW_IOT_MODE_M1 = 0U,
    BG96_NW_IOT_MODE_NB1 = 1U,
    BG96_NW_IOT_MODE_M1NB1 = 2U
} bg96_nw_iot_mode_t;


// void bg96_create();
// void bg96_destroy();

void bg96_start();
// void bg96_stop();

void bg96_setNwScanSeq(const char* sequence);
void bg96_setNwScanMode(bg96_nw_scan_mode_t mode);
void bg96_setIotOpMode(bg96_nw_iot_mode_t mode);


#endif  /* !__QUECTEL_BG96_H__ */
