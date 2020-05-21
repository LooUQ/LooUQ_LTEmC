/******************************************************************************
 *  \file network.h
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

// #define PROTOCOL_RESULT_SUCCESS         200
// #define PROTOCOL_RESULT_ERROR           500
// #define PROTOCOL_RESULT_UNAVAILABLE     503

#define LTEM1_SOCKET_COUNT 6
#define LTEM1_CONTEXT_COUNT 3
#define SOCKET_CLOSED 255
#define NTWK_DEFAULT_CONTEXT 255

typedef uint8_t socketId_t; 
typedef uint16_t socketResult_t;


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
    protocol_mqtts = 0x31,

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


#define NTWKOPERATOR_OPERNAME_SZ 29
#define NTWKOPERATOR_NTWKMODE_SZ 11

typedef struct networkOperator_tag
{
	char operName[29];
	char ntwkMode[11];
} networkOperator_t;

#define PDPCONTEXT_APNNAME_SZ 21
#define PDPCONTEXT_IPADDRESS_SZ 16

typedef struct pdpContext_tag
{
    context_state_t contextState;
    context_type_t contextType;
	char apnName[21];
	char ipAddress[16];
} pdpContext_t;


typedef struct network_tag
{
    networkOperator_t *networkOperator;
    pdpContext_t contexts[LTEM1_CONTEXT_COUNT];
} network_t;


typedef void (*receiver_func_t)(socketId_t scktId, const char * data, uint16_t dataSz, const char * rmtHost, const char * rmtPort);


typedef volatile struct socketCtrl_tag
{
    protocol_t protocol;
    bool dataPending;
    uint8_t contextId;
    uint16_t recvBufSz;
    receiver_func_t receiver_func;
} socketCtrl_t;


typedef struct protocols_tag
{
    socketCtrl_t sockets[LTEM1_SOCKET_COUNT];
} protocols_t;


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


network_t *ntwk_createNetwork();
void ntwk_destroyNetwork();
protocols_t *ntwk_createProtocols();
void ntwk_destroyProtocols();

networkOperator_t ntwk_getOperator();

socketResult_t ntwk_fetchDataContexts();
socketResult_t ntwk_activateContext(uint8_t contxtId);
socketResult_t ntwk_deactivateContext(uint8_t contxtId);
pdpContext_t ntwk_getDataContext(uint8_t contxtId);

void ntwk_closeContext(uint8_t contxtId);

#ifdef __cplusplus
}
#endif // !__cplusplus



#endif  /* !__PROTOCOLS_H__ */