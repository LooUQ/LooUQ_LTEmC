/** ***************************************************************************
  @file 
  @brief Modem socket (UDP/TCP) communication functions/services.

  @author Greg Terrell, LooUQ Incorporated

  \loouq
-------------------------------------------------------------------------------

LooUQ-LTEmC // Software driver for the LooUQ LTEm series cellular modems.
Copyright (C) 2017-2023 LooUQ Incorporated

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
Also add information on how to contact you by electronic and paper mail.

**************************************************************************** */


#ifndef __LTEMC_SCKT_H__
#define __LTEMC_SCKT_H__

#include <lq-types.h>
#include "ltemc-types.h"




/** 
 *  @brief Callback function for data received event. Marshalls received data to application.

 *  @param dataCntxt [in] Data context (socket) with new received data available.
 *  @param [in] dataPtr Pointer to the received data available to the application.
 *  @param [in] dataSz Size of the data block present at the dataPtr location.
 *  @param [in] isFinal True if this block of data is the last block in the current receive flow.
*/

typedef void (*scktAppRcvr_func)(dataCntxt_t dataCntxt, char* dataPtr, uint16_t dataSz, bool isFinal);



/** 
 *  @brief Typed numeric constants for the sockets subsystem
 */
enum sckt__constants
{
    sckt__urlHostSz = 128,
    sckt__resultCode_alreadyOpen = 563,
    sckt__defaultOpenTimeoutMS = 60000,
    sckt__irdRequestMaxSz = 1500,
    sckt__irdRequestPageSz = sckt__irdRequestMaxSz / 2,

    sckt__readTrailerSz = 6,                // /r/nOK/r/n
    sckt__readTimeoutMs = 1000
};

/**
 * @brief State of a socket connection 
 */
typedef enum scktState_tag
{
    scktState_closed = 0,
    scktState_flushPending,
    scktState_open
} scktState_t;


/** 
 *  @brief Struct representing the state of a TCP/UDP/SSL socket stream.
*/
typedef struct scktCtrl_tag
{
    char streamType;                            ///< stream type
    dataCntxt_t dataCntxt;                      ///< integer representing the source of the stream; fixed for protocols, file handle for FS
    dataHndlr_func dataRxHndlr;                 ///< function to handle data streaming, initiated by eventMgr() or atcmd module
    urcEvntHndlr_func urcEvntHndlr;             ///< function to determine if "potential" URC event is for an open stream and perform reqd actions
    closeStream_func closeStreamCB;             ///< function to close stream and update stream control structure (usually invoked after URC detected)

    /* Above section of <stream>Ctrl structure is the same for all LTEmC implemented streams/protocols TCP/HTTP/MQTT etc. 
    */
    uint8_t pdpCntxt;                           ///< Packet data context hosting this data connection
    appRcvr_func appRcvrCB;                     ///< callback into host application with data (cast from generic func* to stream specific function)
    char hostUrl[SET_PROPLEN(sckt__urlHostSz)]; ///< remote host URL/IP address
    uint16_t hostPort;                          ///< remote host port number
    uint16_t lclPort;                           ///< local port number
    bool useTls;                                ///< use TLS for connection
    scktState_t state;                          ///< current connection state

    bool flushing;                              ///< True if the socket was opened with cleanSession and the socket was found already open.
    uint16_t irdPending;                        ///< Char count of remaining for current IRD/SSLRECV flow. Starts at reported IRD value and counts down
    uint32_t statsTxCnt;                        ///< Number of atomic TX sends
    uint32_t statsRxCnt;                        ///< Number of atomic RX segments (URC/IRD)
} scktCtrl_t;



#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


/**
 *	@brief Create a socket data control(TCP/UDP/SSL).

 *  @param [in] scktCtrl Pointer to socket control structure
 *  @param [in] pdpCntxt PDP context supporting this data connection. NOTE TCP/UDP can run over non-default PDP contexts
 *  @param [in] protocol TCP/UDP or SSL/TLS
 *  @param [in] appRcvrCB Callback function in your application to be notified of received data ready
 */
// void sckt_initControl(scktCtrl_t *scktCtrl, uint8_t pdpContextId, dataCntxt_t dataCntxt, protocol_t protocol, uint8_t *recvBuf, uint16_t recvBufSz, scktRecvFunc_t recvCallback);
void sckt_initControl(scktCtrl_t *scktCtrl, uint8_t pdpCntxt, streamType_t protocol, scktAppRcvr_func appRcvrCB);


/**
 *	@brief Set connection parameters for a socket connection (TCP/UDP)
 *
 *  @param scktCtrl [in/out] Pointer to socket control structure
 *  @param [in] pdpCntxt PDP context supporting this data connection. NOTE TCP/UDP can run over non-default PDP contexts
 *	@param [in] hostUrl IP address (string) or domain name of the remote host to communicate with
 *  @param [in] hostPort Port number at the remote host
 *  @param [in] lclPort Port number on this side of the conversation, set to 0 to auto-assign
 */
void sckt_setConnection(scktCtrl_t *scktCtrl, uint8_t pdpCntxt, const char *hostUrl, const uint16_t hostPort, uint16_t lclPort);


/**
 *	@brief Open a data connection (socket) to d data to an established endpoint via protocol used to open socket (TCP/UDP)
 *
 *  @param scktCtrl [in/out] Pointer to socket control structure
 *  @param cleanSession [in] - If the port is found already open, TRUE: flushes any previous data from the socket session
 *  @return Result code similar to http status code, OK = 200
 */
resultCode_t sckt_open(scktCtrl_t *scktCtrl, bool cleanSession);


/**
 *	@brief Close an established (open) connection socket
 *
 *	@param [in] scktCtrl Pointer to socket control struct governing the sending socket's operation
 */
void sckt_close(scktCtrl_t *scktCtrl);


/**
 *	@brief Close an established (open) connection socket by context number.
 *  @details This is provided for LTEm use in the case of a connection close/loss.
 * 
 *	@param [in] cntxtNm Data context of the stream service.
 */
void SCKT_closeCntxt(uint8_t cntxtNm);


/**
 *	@brief Reset open socket connection. This function drains the connection's data pipeline 

 *	@param [in] scktCtrl Pointer to socket control struct governing the sending socket's operation
 *  @return True if flush socket data initiated
 */
bool sckt_flush(scktCtrl_t *scktCtrl);



/**
 *	@brief Retrieve the state of a socket connection

 *	@param [in] scktCtrl Pointer to socket control struct governing the sending socket's operation
 *  @return True if socket is open
 */
bool sckt_getState(scktCtrl_t *scktCtrl);


/**
 *	@brief Send data to an established endpoint via protocol used to open socket (TCP/UDP)
 
 *	@param [in] scktCtrl Pointer to socket control struct governing the sending socket's operation
 *	@param [in] data A character pointer containing the data to send
 *  @param [in] dataSz The size of the buffer (< 1501 bytes)
 */
resultCode_t sckt_send(scktCtrl_t *scktCtrl, const char *data, uint16_t dataSz);


/**
 *	@brief Fetch receive data by host application
 *
 *	@param [in] scktCtrl Pointer to socket control being operated on.
 *	@param [in] recvBffr Char pointer to the data buffer for received chars
 *  @param [in] bffrSz Size of the buffer or request
 *  @return Number of chars outstanding in receive flow. This number can increase, if new data received while processing.
 */
uint16_t sckt_fetchRecv(scktCtrl_t *scktCtrl, char *recvBffr, uint16_t bffrSz);


/**
 *	@brief Cancel an active receive flow and discard any recieved bytes.
 *  @details This is a blocking call, returns after all outstanding bytes are retrieved from module and discarded. Connection can continue receiving new bytes if not closed.
 * 
 *	@param [in] scktCtrl Pointer to socket control being operated on.
 */
void sckt_cancelRecv(scktCtrl_t *scktCtrl);


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__LTEMC_SCKT_H__ */
