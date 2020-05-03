/******************************************************************************
 *  \file ltem1.h
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

#ifndef __LTEM1C_UTIL_H__
#define __LTEM1C_UTIL_H__

#include "ltem1c.h"


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


#define IP_DISPLAY(NUM_IP, STRING_IP) \
char STRING_IP##[16]; \
sprintf(STRING_IP, "%d.%d.%d.%d", NUM_IP##.octet[0] , NUM_IP##.octet[1], NUM_IP##.octet[2], NUM_IP##.octet[3]);


union ip_addr_tag
{
    uint8_t octet[4];
	uint32_t packed;
};

typedef union ip_addr_tag ip_addr_t;


#ifdef __cplusplus
}
#endif // !__cplusplus


#endif  /* !__LTEM1C_UTIL_H__ */