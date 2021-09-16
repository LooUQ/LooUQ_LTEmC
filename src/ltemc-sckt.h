/******************************************************************************
 *  \file ltemc-sckt.h
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

#ifndef __LTEMC_SCKT_H__
#define __LTEMC_SCKT_H__

#include <lq-types.h>
#include "ltemc-streams.h"
#include "ltemc-internal.h"


/** 
 *  \brief Callback function for data received event. Notifies application that new data is available and needs serviced.
 * 
 *  The *data and dataSz values are for convenience, since the application supplied the buffer to LTEmC.
 * 
 *  \param peerId [in] Data peer (data context or filesys) 
 *  \param handle [in] Subordinate data ID to give application information about received data
 *  \param data [in] Pointer to received data buffer
 *  \param dataSz [in] The number of bytes available
*/
typedef void (*scktRecvFunc_t)(streamPeer_t peerId, void *data, uint16_t dataSz);


/** 
 *  \brief Struct representing the state of a TCP/UDP/SSL socket stream.
*/
typedef struct scktCtrl_tag
{
    uint8_t ctrlMagic;                  ///< magic flag to validate incoming requests 
    dataContext_t dataCntxt;            ///< Data context where this control operates
    protocol_t protocol;                ///< Controls's protocol : UDP/TCP/SSL.
    bool useTls;                        ///< flag indicating SSL/TLS applied to stream
    rxDataBufferCtrl_t recvBufCtrl;     ///< RX smart buffer 

    scktRecvFunc_t dataRecvCB;          ///< callback to application, signals data ready
    uint32_t doWorkLastTck;             ///< last check for URC/dataPending
    //uint32_t doWorkTimeout;             ///< set at init for doWork ASSERT, if timeout reached chance for a data overflow on socket
    bool flushing;                      ///< True if the socket was opened with cleanSession and the socket was found already open.
    bool dataPending;                   ///< The data pipeline has data (or the likelihood of data), triggered when BGx reports data pending (URC "recv").
    int16_t irdRemaining;               ///< SIGNED number of outstanding IRD bytes to receive, -1 is unset value
    uint32_t statsTxCnt;                ///< Number of atomic TX sends
    uint32_t statsRxCnt;                ///< Number of atomic RX segments (URC/IRD)
    
} scktCtrl_t;


// /** 
//  *  \brief Struct representing the sockets service.
// */
// typedef volatile struct sockets_tag
// {
//     socketCtrl_t socketCtrls[SOCKET_COUNT];   ///< Array of socket connections.
// } sockets_t;



#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

// void sckt_create();
void sckt_initControl(scktCtrl_t *scktCtrl, dataContext_t dataCntxt, protocol_t protocol, uint8_t *recvBuf, uint16_t recvBufSz, scktRecvFunc_t recvCallback);

resultCode_t sckt_open(scktCtrl_t *scktCtrl, const char *host, uint16_t rmtPort, uint16_t lclPort, bool cleanSession);

void sckt_close(scktCtrl_t *scktCtrl);
bool sckt_flush(scktCtrl_t *scktCtrl);
void sckt_closeAll();

bool sckt_getState(scktCtrl_t *scktCtrl);

resultCode_t sckt_send(scktCtrl_t *scktCtrl, const char *data, uint16_t dataSz);


// private to LTEmc
void sckt__doWork();


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__LTEMC_SCKT_H__ */
