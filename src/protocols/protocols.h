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
#ifndef __PROTOCOLS_H__
#define __PROTOCOLS_H__

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

typedef enum {
    protocol_udp = 0x00U,
    protocol_tcp = 0x01U,
    protocol_ssl = 0x02U,
    protocol_http = 0x05U,
    protocol_https = 0x06U,
    protocol_mqtt = 0x10U
} protocol_session_type_t;


typedef struct protocol_session_tag
{
    protocol_session_type_t proto;
    uint8_t apn;
    uint8_t session;
} protocol_session_t;



#ifdef __cplusplus
}
#endif // !__cplusplus



#endif  /* !__PROTOCOLS_H__ */