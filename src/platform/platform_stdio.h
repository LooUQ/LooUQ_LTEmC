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

#define __DEBUG
#define __SEGGER_RTT

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


typedef enum print_color_tag {
    debug_print_color_none,
    debug_print_color_info,
    debug_print_color_warn,
    debug_print_color_error,    
    debug_print_color_cyan,
    debug_print_color_magenta,
    debug_print_color_white
} print_color_t;


#if defined(__DEBUG)
#define PRINTF(f_, ...) _dbg_printf(debug_print_color_none, (f_), ##__VA_ARGS__)
#define PRINTF_INFO(f_, ...) _dbg_printf(debug_print_color_info, (f_), ##__VA_ARGS__)
#define PRINTF_WARN(f_, ...) _dbg_printf(debug_print_color_warn, (f_), ##__VA_ARGS__)
#define PRINTF_ERR(f_, ...) _dbg_printf(debug_print_color_error, (f_), ##__VA_ARGS__)

#define PRINTF_CY(f_, ...) _dbg_printf(debug_print_color_cyan, (f_), ##__VA_ARGS__)
#define PRINTF_MA(f_, ...) _dbg_printf(debug_print_color_magenta, (f_), ##__VA_ARGS__)
#define PRINTF_WH(f_, ...) _dbg_printf(debug_print_color_white, (f_), ##__VA_ARGS__)

#else
#define DBGPRINTF(f_, ...) 
#endif


void _dbg_printf(print_color_t color, const char *fmt, ...);

#ifdef __cplusplus
}
#endif // __cplusplus



#endif  /* !__PLATFORM_STDIO_H__ */