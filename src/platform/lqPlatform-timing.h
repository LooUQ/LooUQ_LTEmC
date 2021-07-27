/******************************************************************************
 *  \file platform_timing.h
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
#ifndef __PLATFORM_TIMING_H__
#define __PLATFORM_TIMING_H__



// yield callback allows host application to be signalled when the LTEm1 is awaiting network events

typedef void (*platform_yieldCB_func_t)();

extern platform_yieldCB_func_t platform_yieldCB_func;


// typedef struct lTiming_tag
// {
//     yieldCB_func_t yieldCB_func;        ///< Callback (CB) to host application when driver code is waiting for network events, allows for background\watchdog\etc.
//     bool cancellationRequest;           ///< For RTOS implementations, token to request cancellation of background operation.
// } lTiming_t;


#ifdef __cplusplus
extern "C"
{
#include <cstdint>
#else
#include <stdint.h>
#endif // __cplusplus

/* transition to new names 

// uint32_t lMillis();
// void lYield();

// // platform implementation should support task switching here
// void lDelay(uint32_t delay_ms);

// bool lTimerExpired(uint32_t timerBase, uint32_t timerTimeout);
*/


uint32_t pMillis();
void pYield();

// platform implementation should support task switching here
void pDelay(uint32_t delay_ms);

bool pElapsed(uint32_t timerBase, uint32_t timerTimeout);


#ifdef __cplusplus
}
#endif // __cplusplus



#endif  /* !__PLATFORM_TIMING_H__ */