/******************************************************************************
 *  \file ltemc-sockets.c
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020,2021 LooUQ Incorporated.
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
 ******************************************************************************
 * TCP/IP sockets protocol support: TCP, UDP, SSL, TLS
 *****************************************************************************/

#ifndef __LTEMC_SOCKETS_H__
#define __LTEMC_SOCKETS_H__

#include "ltemc.h"


/* future may support incoming connections, network carriers do not support this without premium plans or VPNs
 * today LooUQ Cloud supports this via an alternate pattern. 
 * typedef void (*receiver_func_t)(socketId_t scktId, const char * data, uint16_t dataSz, const char * rmtHost, const char * rmtPort);
*/

#define SOCKET_COUNT 6
#define SOCKET_CLOSED 255
#define SOCKET_RESULT_PREVOPEN 563
#define SOCKET_SEND_RETRIES 3

typedef uint8_t socketId_t; 
typedef uint16_t socketResult_t;

/** 
 *  \brief typedef for the socket services data receiver function. Connects sockets to the applicatoin (receive).
*/
typedef void (*receiver_func_t)(socketId_t scktId, void *data, uint16_t dataSz);


/** 
 *  \brief Struct representing the state of a TCP\UDP\SSL socket connection.
*/
typedef struct socketCtrl_tag
{
    protocol_t protocol;            ///< Socket's protocol : UDP\TCP\SSL.
    socketId_t socketId;            ///< Socket ID/number.
    bool open;                      ///< Is the socket in an open state.
    bool flushing;                  ///< True if the socket was opened with cleanSession and the socket was found already open.
    bool dataPending;               ///< The data pipeline has data (or the likelihood of data), triggered when BGx reports data pending (URC "recv").
    uint8_t dataBufferIndx;         ///< buffer indx holding data 
    uint8_t pdpContextId;           ///< Which network context is this data flow associated with.
    receiver_func_t receiver_func;  ///< Data receive function for socket data. This func is invoked for every receive event.
} socketCtrl_t;


/** 
 *  \brief Struct representing the sockets service.
*/
typedef volatile struct sockets_tag
{
    socketCtrl_t socketCtrls[SOCKET_COUNT];   ///< Array of socket connections.
} sockets_t;


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


void sckt_create();

socketResult_t sckt_open(socketId_t socketId, protocol_t protocol, const char *host, uint16_t rmtPort, uint16_t lclPort, bool cleanSession, receiver_func_t rcvr_func);
void sckt_close(uint8_t socketId);
bool sckt_flush(uint8_t socketId);
void sckt_closeAll(uint8_t contxtId);

bool sckt_getState(uint8_t socketId);

socketResult_t sckt_send(socketId_t socketId, const char *data, uint16_t dataSz);
void sckt_doWork();


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__LTEMC_SOCKETS_H__ */
