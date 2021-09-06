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
    #ifndef DBCCOLOR_dflt
    #define DBGCOLOR_dflt 13
    #define DBGCOLOR_info 15
    #define DBGCOLOR_warn 17
    #define DBGCOLOR_error 16

    #define DBGCOLOR_cyan 10
    #define DBGCOLOR_magenta 11
    #define DBGCOLOR_white 12
    #define DBGCOLOR_gray 13
    #define DBGCOLOR_blue 14
    #define DBGCOLOR_green 15
    #define DBGCOLOR_red 16
    #define DBGCOLOR_yellow 17

    #define DBGCOLOR_dCyan 20
    #define DBGCOLOR_dMagenta 21
    #define DBGCOLOR_dBlue 24
    #define DBGCOLOR_dGreen 25
    #define DBGCOLOR_dRed 26
    #define DBGCOLOR_dYellow 27
    #endif

    #ifndef PRINTF
    #define PRINTF(c_, f_, ...) dbg_print((f_), ##__VA_ARGS__)
    #endif
#else
    #ifndef PRINTF
    #define PRINTF(c_, f_, ...) ;
    #endif
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