/******************************************************************************
 *  \file platform_stdio.h
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
#ifndef __PLATFORM_STDIO_H__
#define __PLATFORM_STDIO_H__

#include <stdio.h>
#include <stdarg.h>


#ifdef _DEBUG
#define PRINTF(c_, f_, ...) dbg_printf(c_, (f_), ##__VA_ARGS__)
#else
#define PRINTF(c_, f_, ...) ;
#endif


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

typedef enum print_color_tag {
    dbgColor_none = 0,
    dbgColor_info = 1,
    dbgColor_warn = 2,
    dbgColor_error = 3,

    dbgColor_cyan = 10,
    dbgColor_magenta = 11,
    dbgColor_white = 12,
    dbgColor_gray = 13,
    dbgColor_blue = 14,

    dbgColor_dCyan = 20,
    dbgColor_dMagenta = 21,

    dbgColor_green = 1,
    dbgColor_dGreen = 25

} print_color_t;


// #if defined(_DEBUG)
// #define PRINTF(f_, ...) dbg_printf(dbgColor_none, (f_), ##__VA_ARGS__)
// // #define PRINTF_INFO(f_, ...) dbg_printf(dbgColor_info, (f_), ##__VA_ARGS__)
// // #define PRINTF_WARN(f_, ...) dbg_printf(dbgColor_warn, (f_), ##__VA_ARGS__)
// // #define PRINTF_ERR(f_, ...) dbg_printf(dbgColor_error, (f_), ##__VA_ARGS__)
// // #define PRINTF_DBG(f_, ...) dbg_printf(dbgColor_magenta, (f_), ##__VA_ARGS__)

// // #define PRINTF_CYAN(f_, ...) dbg_printf(dbgColor_cyan, (f_), ##__VA_ARGS__)
// // #define PRINTF_WHITE(f_, ...) dbg_printf(dbgColor_white, (f_), ##__VA_ARGS__)
// // #define PRINTF_GRAY(f_, ...) dbg_printf(dbgColor_gray, (f_), ##__VA_ARGS__)

// #else
// #define PRINTF(f_, ...) ;
// #endif


void dbg_printf(uint8_t color, const char *fmt, ...);

#ifdef __cplusplus
}
#endif // __cplusplus



#endif  /* !__PLATFORM_STDIO_H__ */