/******************************************************************************
 *  \file platform_debug.h
 *  \author Greg Terrell
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
#ifndef __PLATFORM_DEBUG_H__
#define __PLATFORM_DEBUG_H__


#define DBGBUFFER_SZ 120

#ifdef _DEBUG
    #ifdef JLINK_RTT
    #include <jlink_rtt.h>
    // #define Serial JlinkRtt
    #define PRINTFC(c_, f_, ...) rtt_printf((rtt_color_t)c_, (f_), ##__VA_ARGS__)
    #define PRINTF(c_, f_, ...) rtt_printf((rtt_color_t)c_, (f_), ##__VA_ARGS__)
    #else
    #define PRINTF(c_, f_, ...) dbg_print((f_), ##__VA_ARGS__)
    #define PRINTFC(c_, f_, ...) dbg_print((f_), ##__VA_ARGS__)
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#define PRINTFC(c_, f_, ...) ;
#endif



#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


void dbg_print(const char *msg, ...);


#ifdef __cplusplus
}
#endif // __cplusplus

#endif  /* !__PLATFORM_DEBUG_H__ */