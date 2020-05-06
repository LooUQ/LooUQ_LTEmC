/******************************************************************************
 *  \file protocols.h
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

#ifndef __PROTOCOLS_H__
#define __PROTOCOLS_H__

#include "..\ltem1c.h"

#define PROTOCOL_RESULT_SUCCESS         200
#define PROTOCOL_RESULT_ERROR           500
#define PROTOCOL_RESULT_UNAVAILABLE     503

#define LTEM1_SOCKET_COUNT 6
#define LTEM1_CONTEXT_COUNT 3
#define SOCKET_CLOSED 255


typedef uint8_t socket_t; 
typedef uint16_t socket_result_t;


typedef enum 
{
    protocol_tcp = 0x00,
    protocol_udp = 0x01,
    protocol_tcpListener = 0x02,
    protocol_udpService = 0x03,
    protocol_ssl = 0x05,
    protocol_AnyIP = 0x05,

    protocol_http = 0x20,
    protocol_https = 0x21,

    protocol_mqtt = 0x30,

    protocol_none = 0xFF
} protocol_t;


typedef enum 
{
    context_state_inactive = 0,
    context_state_active = 1
} context_state_t;


typedef enum
{
    context_type_IPV4 = 1,
    context_type_IPV6 = 2
} context_type_t;


typedef struct pdp_context_tag
{
    context_state_t contextState;
    context_type_t contextType;
	char apnName[21];
	char ipAddress[16];
} pdp_context_t;


typedef struct network_tag
{
    pdp_context_t contexts[LTEM1_CONTEXT_COUNT];
} network_t;


typedef struct socketCtrl_tag
{
    protocol_t protocol;
    uint8_t contextId;
    uint16_t recvBufSz;
    void (*ipReceiver_func)(socket_t);
} socketCtrl_t;


typedef struct protocols_tag
{
    socketCtrl_t sockets[LTEM1_SOCKET_COUNT];
} protocols_t;


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus





#ifdef __cplusplus
}
#endif // !__cplusplus



#endif  /* !__PROTOCOLS_H__ */