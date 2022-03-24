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
 *  @brief Typed numeric constants for the sockets subsystem
 */
enum sckt__constants
{
    sckt__resultCode_previouslyOpen = 563,
    sckt__defaultOpenTimeoutMS = 60000,

    SCKT__IRD_requestMaxSz = 1500
};


/** 
 *  @brief Callback function for data received event. Notifies application that new data is available and needs serviced.
 *  @details The *data and dataSz values are for convenience, since the application supplied the buffer to LTEmC.
 *  @param peerId [in] Data peer (data context or filesys) 
 *  @param handle [in] Subordinate data ID to give application information about received data
 *  @param data [in] Pointer to received data buffer
 *  @param dataSz [in] The number of bytes available
*/
typedef void (*scktRecvFunc_t)(streamPeer_t peerId, void *data, uint16_t dataSz);


/** 
 *  @brief Struct representing the state of a TCP/UDP/SSL socket stream.
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



#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


/**
 *	@brief Create a socket data control(TCP/UDP/SSL).
 *  @param scktCtrl [in/out] Pointer to socket control structure
 *	@param dataCntxt [in] - Data context (0-5) to host socket
 *	@param protocol [in] - The IP protocol to use for the connection (TCP/UDP/SSL clients)
 *  @param recvBuf [in] - Pointer to application created receive buffer
 *  @param recvBufSz [in] - Size of allocated receive buffer
 *  @param recvCallback [in] - The callback function in your application to be notified of received data ready
 *  @return socket result code similar to http status code, OK = 200
 */
void sckt_initControl(scktCtrl_t *scktCtrl, dataContext_t dataCntxt, protocol_t protocol, uint8_t *recvBuf, uint16_t recvBufSz, scktRecvFunc_t recvCallback);


/**
 *	@brief Open a data connection (socket) to d data to an established endpoint via protocol used to open socket (TCP/UDP/TCP INCOMING)
 *  @param sckt [in] - Pointer to the control block for the socket
 *	@param host [in] - The IP address (string) or domain name of the remote host to communicate with
 *  @param rmtPort [in] - The port number at the remote host
 *  @param lclPort [in] - The port number on this side of the conversation, set to 0 to auto-assign
 *  @param cleanSession [in] - If the port is found already open, TRUE: flushes any previous data from the socket session
 *  @return Result code similar to http status code, OK = 200
 */
resultCode_t sckt_open(scktCtrl_t *scktCtrl, const char *host, uint16_t rmtPort, uint16_t lclPort, bool cleanSession);


/**
 *	@brief Close an established (open) connection socket
 *	@param scktCtrl [in] - Pointer to socket control struct governing the sending socket's operation
 */
void sckt_close(scktCtrl_t *scktCtrl);


/**
 *	@brief Reset open socket connection. This function drains the connection's data pipeline 
 *	@param scktCtrl [in] - Pointer to socket control struct governing the sending socket's operation
 *  @return True if flush socket data initiated
 */
bool sckt_flush(scktCtrl_t *scktCtrl);


/**
 *  @brief Close out all TCP/IP sockets on a context
 *	@param contxtId [in] - The carrier PDP context hosting the sockets to close
*/
void sckt_closeAll();


/**
 *	@brief Retrieve the state of a socket connection
 *	@param scktCtrl [in] - Pointer to socket control struct governing the sending socket's operation
 *  @return True if socket is open
 */
bool sckt_getState(scktCtrl_t *scktCtrl);


/**
 *	@brief Send data to an established endpoint via protocol used to open socket (TCP/UDP/TCP INCOMING)
 *	@param scktCtrl [in] - Pointer to socket control struct governing the sending socket's operation
 *	@param data [in] - A character pointer containing the data to send
 *  @param dataSz [in] - The size of the buffer (< 1501 bytes)
 */
resultCode_t sckt_send(scktCtrl_t *scktCtrl, const char *data, uint16_t dataSz);


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__LTEMC_SCKT_H__ */
