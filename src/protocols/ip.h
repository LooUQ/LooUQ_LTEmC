/******************************************************************************
 *  \file ip.h
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

#ifndef __IP_H__
#define __IP_H__

#include "..\ltem1c.h"


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


network_t *ip_createNetwork();
void ip_destroyNetwork();
protocols_t *ip_createProtocols();
void ip_destroyProtocols();

socket_result_t ip_fetchNetworkContexts();
socket_result_t ip_activateContext(uint8_t contextNum);
socket_result_t ip_deactivateContext(uint8_t contextNum);

socket_result_t ip_open(protocol_t protocol, const char *host, uint16_t rmtPort, uint16_t lclPort, void (*ipReceiver_func)(socket_t));
void ip_close(uint8_t socketNum);

socket_result_t ip_send(socket_t socketNum, char *sendBuf, uint16_t sendSz);
uint16_t ip_recv(socket_t socketNum, char *recvBuf, uint16_t recvBufSz);

// protocol_result_t ip_sendUdpReply(uint8_t socketNum, const char *rmtHost, uint16_t rmtPort,  char *sendBuf, uint8_t sendSz);

void ip_receiverDoWork();


#ifdef __cplusplus
}
#endif // !__cplusplus


#endif  /* !__IP_H__ */
