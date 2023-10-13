/** ****************************************************************************
  \file 
  \author Greg Terrell, LooUQ Incorporated

  \loouq

--------------------------------------------------------------------------------

    This project is released under the GPL-3.0 License.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 
***************************************************************************** */


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

typedef void (*scktAppRecv_func)(dataCntxt_t dataCntxt, char* dataPtr, uint16_t dataSz, bool isFinal);



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
    char streamType;                            // stream type
    dataCntxt_t dataCntxt;                      // integer representing the source of the stream; fixed for protocols, file handle for FS
    dataHndlr_func dataRxHndlr;                 // function to handle data streaming, initiated by eventMgr() or atcmd module
    urcEvntHndlr_func urcEvntHndlr;             // function to determine if "potential" URC event is for an open stream and perform reqd actions

    /* Above section of <stream>Ctrl structure is the same for all LTEmC implemented streams/protocols TCP/HTTP/MQTT etc. 
    */
    uint8_t pdpCntxt;
    appRcvProto_func appRecvDataCB;             // callback into host application with data (cast from generic func* to stream specific function)
    char hostUrl[PSZ(sckt__urlHostSz)];         // remote host URL/IP address
    uint16_t hostPort;
    uint16_t lclPort;
    bool useTls;
    scktState_t state;

    bool flushing;                              // True if the socket was opened with cleanSession and the socket was found already open.
    uint16_t irdPending;                        // Char count of remaining for current IRD/SSLRECV flow. Starts at reported IRD value and counts down
    uint32_t statsTxCnt;                        // Number of atomic TX sends
    uint32_t statsRxCnt;                        // Number of atomic RX segments (URC/IRD)
} scktCtrl_t;



#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


/**
 *	@brief Create a socket data control(TCP/UDP/SSL).
 *  @param scktCtrl [in/out] Pointer to socket control structure
 *  @param pdpContextId [in] - The PDP context supporting this data connection TCP/UDP can run over non-default PDP contexts
 *	@param socketId [in] - Data context (0-5) to host socket
 *	@param protocol [in] - The IP protocol to use for the connection (TCP/UDP/SSL clients)
 *  @param recvBuf [in] - Pointer to application created receive buffer
 *  @param recvBufSz [in] - Size of allocated receive buffer
 *  @param recvCallback [in] - The callback function in your application to be notified of received data ready
 *  @return socket result code similar to http status code, OK = 200
 */
// void sckt_initControl(scktCtrl_t *scktCtrl, uint8_t pdpContextId, dataCntxt_t dataCntxt, protocol_t protocol, uint8_t *recvBuf, uint16_t recvBufSz, scktRecvFunc_t recvCallback);
void sckt_initControl(scktCtrl_t *scktCtrl, dataCntxt_t dataCntxt, streamType_t protocol, scktAppRecv_func recvCallback);


/**
 *	@brief Set connection parameters for a socket connection (TCP/UDP)
 *  @param scktCtrl [in/out] Pointer to socket control structure
 *	@param hostUrl [in] - The IP address (string) or domain name of the remote host to communicate with
 *  @param hostPort [in] - The port number at the remote host
 *  @param lclPort [in] - The port number on this side of the conversation, set to 0 to auto-assign
 */
void sckt_setConnection(scktCtrl_t *scktCtrl, uint8_t pdpCntxt, const char *hostUrl, const uint16_t hostPort, uint16_t lclPort);


/**
 *	@brief Open a data connection (socket) to d data to an established endpoint via protocol used to open socket (TCP/UDP/TCP INCOMING)
 *  @param scktCtrl [in/out] Pointer to socket control structure
 *  @param cleanSession [in] - If the port is found already open, TRUE: flushes any previous data from the socket session
 *  @return Result code similar to http status code, OK = 200
 */
resultCode_t sckt_open(scktCtrl_t *scktCtrl, bool cleanSession);


/**
 *	@brief Close an established (open) connection socket
 *	@param scktCtrl [in] - Pointer to socket control struct governing the sending socket's operation
 */
void sckt_close(scktCtrl_t *scktCtrl);


/**
 *	@brief Close an established (open) connection socket by context number.
 *  @details This is provided for LTEm use in the case of a connection close/loss.
 *	@param cntxtNm [in] - Data context of the stream service.
 */
void SCKT_closeCntxt(uint8_t cntxtNm);


/**
 *	@brief Reset open socket connection. This function drains the connection's data pipeline 
 *	@param scktCtrl [in] - Pointer to socket control struct governing the sending socket's operation
 *  @return True if flush socket data initiated
 */
bool sckt_flush(scktCtrl_t *scktCtrl);



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


/**
 *	@brief Fetch receive data by host application
 
 *	@param scktCtrl [in] - Pointer to socket control being operated on.
 *	@param recvBffr [in] - A char pointer to the data buffer for received chars
 *  @param dataSz [in] - The size of the buffer or request
 *  @return Number of chars outstanding in receive flow. This number can increase, if new data received while processing.
 */
uint16_t sckt_fetchRecv(scktCtrl_t *scktCtrl, char *recvBffr, uint16_t bffrSz);



/**
 *	@brief Cancel an active receive flow and discard any recieved bytes.
 *  @details This is a blocking call, returns after all outstanding bytes are retrieved from module and discarded. Connection can continue receiving new bytes if not closed.
 * 
 *	@param scktCtrl [in] Pointer to socket control being operated on.
 */
void sckt_cancelRecv(scktCtrl_t *scktCtrl);


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__LTEMC_SCKT_H__ */
