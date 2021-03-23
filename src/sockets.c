/******************************************************************************
 *  \file sockets.c
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
 *****************************************************************************/

#define _DEBUG
#include "ltem1c.h"
#include "sockets.h"


// debugging output options             UNCOMMENT one of the next two lines to direct debug (PRINTF) output
#include <jlinkRtt.h>                   // output debug PRINTF macros to J-Link RTT channel
// #define SERIAL_OPT 1                    // enable serial port comm with devl host (1=force ready test)


#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define SOCKETS_CMDBUF_SZ 80
#define IRD_RETRIES 10
#define IRD_REQ_MAXSZ 1500

#define ASCII_sSENDOK "SEND OK\r\n"


// local function declarations
static bool s_requestIrdData(iopDataPeer_t dataPeer, bool applyLock);
static resultCode_t s_tcpudpOpenCompleteParser(const char *response, char **endptr);
static resultCode_t s_sslOpenCompleteParser(const char *response, char **endptr);
static resultCode_t s_socketSendCompleteParser(const char *response, char **endptr);
static resultCode_t s_socketStatusParser(const char *response, char **endptr);


/* public sockets (IP:TCP/UDP/SSL) functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions

static iop_t *iopPtr;
static sockets_t *scktPtr;

/**
 *	\brief Allocate and initialize the IP socket protocol (TCP\UDP\SSL) structure.
 */
void sckt_create()
{
    scktPtr = calloc(1, sizeof(sockets_t));
	if (scktPtr == NULL)
	{
        ltem1_notifyApp(ltem1NotifType_memoryAllocFault,  "ipProtocols-could not alloc IP protocol struct");
	}

    for (size_t i = 0; i < IOP_SOCKET_COUNT; i++)
    {   
        scktPtr->socketCtrls[i].protocol = protocol_void;
        scktPtr->socketCtrls[i].dataBufferIndx = IOP_NO_BUFFER;
        scktPtr->socketCtrls[i].pdpContextId = g_ltem1->dataContext;
        scktPtr->socketCtrls[i].receiver_func = NULL;
        scktPtr->socketCtrls[i].dataBufferIndx = IOP_NO_BUFFER;
    }
    // set global reference to this
    g_ltem1->sockets = scktPtr;
    g_ltem1->scktWork_func = &sckt_doWork;
    // reference IOP peer
    iopPtr = g_ltem1->iop;
    iop_registerProtocol(ltem1OptnModule_sockets, scktPtr);
}



/**
 *	\brief Open a data connection (socket) to d data to an established endpoint via protocol used to open socket (TCP/UDP/TCP INCOMING).
 *
 *	\param socketId [in] - The ID or number specifying the socket connect to open.
 *	\param protocol [in] - The IP protocol to use for the connection (TCP/UDP/TCP LISTENER/UDP SERVICE/SSL).
 *	\param host [in] - The IP address (string) or domain name of the remote host to communicate with.
 *  \param rmtPort [in] - The port number at the remote host.
 *  \param lclPort [in] - The port number on this side of the conversation, set to 0 to auto-assign.
 *  \param cleanSession [in] - If the port is found already open, TRUE: flushes any previous data from the socket session.
 *  \param rcvr_func [in] - The callback function in your application to be notified of received data ready.
 * 
 *  \return socket result code similar to http status code, OK = 200
 */
socketResult_t sckt_open(socketId_t socketId, protocol_t protocol, const char *host, uint16_t rmtPort, uint16_t lclPort, bool cleanSession, receiver_func_t rcvr_func)
{
    char openCmd[SOCKETS_CMDBUF_SZ] = {0};
    char protoName[13] = {0};

    if (socketId >= IOP_SOCKET_COUNT ||
        scktPtr->socketCtrls[socketId].protocol != protocol_void ||
        protocol > protocol_AnyIP ||
        rcvr_func == NULL
        )
    return RESULT_CODE_BADREQUEST;

    uint8_t socketBitMap = 0x01 << socketId;

    switch (protocol)
    {
    case protocol_udp:
        strcpy(protoName, "UDP");
        iopPtr->peerTypeMap.tcpudpSocket = iopPtr->peerTypeMap.tcpudpSocket | socketBitMap;
        snprintf(openCmd, SOCKETS_CMDBUF_SZ, "AT+QIOPEN=%d,%d,\"%s\",\"%s\",%d", g_ltem1->dataContext, socketId, protoName, host, rmtPort);
        action_tryInvokeAdv(openCmd, ACTION_RETRIES_DEFAULT, ACTION_TIMEOUT_DEFAULTmillis, s_tcpudpOpenCompleteParser);
        break;

    case protocol_tcp:
        strcpy(protoName, "TCP");
        iopPtr->peerTypeMap.tcpudpSocket = iopPtr->peerTypeMap.tcpudpSocket | socketBitMap;
        snprintf(openCmd, SOCKETS_CMDBUF_SZ, "AT+QIOPEN=%d,%d,\"%s\",\"%s\",%d", g_ltem1->dataContext, socketId, protoName, host, rmtPort);
        action_tryInvokeAdv(openCmd, ACTION_RETRIES_DEFAULT, ACTION_TIMEOUT_DEFAULTmillis, s_tcpudpOpenCompleteParser);
        break;

    case protocol_ssl:
        strcpy(protoName, "SSL");
        socketBitMap = 0x01 << socketId;
        iopPtr->peerTypeMap.sslSocket = iopPtr->peerTypeMap.sslSocket | socketBitMap;
        snprintf(openCmd, SOCKETS_CMDBUF_SZ, "AT+QSSLOPEN=%d,%d,\"%s\",\"%s\",%d", g_ltem1->dataContext, socketId, protoName, host, rmtPort);
        action_tryInvokeAdv(openCmd, ACTION_RETRIES_DEFAULT, ACTION_TIMEOUT_DEFAULTmillis, s_sslOpenCompleteParser);
        break;

        /* The 2 use cases here are not really supported by the network carriers without premium service */
        // case protocol_udpService:
        //     strcpy(protoName, "UDP SERVICE");
        //     strcpy(host, "127.0.0.1");
        //     break;
        // case protocol_tcpListener:
        //     strcpy(protoName, "TCP LISTENER");
        //     strcpy(host, "127.0.0.1");
        //     break;
    }

    // snprintf(openCmd, SOCKETS_CMDBUF_SZ, "AT+QIOPEN=%d,%d,\"%s\",\"%s\",%d", g_ltem1->dataContext, socketId, protoName, host, rmtPort);
    // action_tryInvokeAdv(openCmd, ACTION_RETRIES_DEFAULT, ACTION_TIMEOUT_DEFAULTmillis, s_tcpudpOpenCompleteParser);

    // await result of open from inside switch() above
    actionResult_t atResult = action_awaitResult(true);

    // finish initialization and run background tasks to prime data pipeline
    if (atResult.statusCode == RESULT_CODE_SUCCESS || atResult.statusCode == SOCKET_RESULT_PREVOPEN)
    {
        scktPtr->socketCtrls[socketId].protocol = protocol;
        scktPtr->socketCtrls[socketId].socketId = socketId;
        scktPtr->socketCtrls[socketId].open = true;
        scktPtr->socketCtrls[socketId].receiver_func = rcvr_func;
    }

    else        // failed to open, reset peerMap bits
    {
        uint8_t socketBitMap = socketBitMap<<socketId;
        iopPtr->peerTypeMap.tcpudpSocket = iopPtr->peerTypeMap.tcpudpSocket & ~socketBitMap;
        iopPtr->peerTypeMap.sslSocket = iopPtr->peerTypeMap.sslSocket & ~socketBitMap;
    }

    if (atResult.statusCode == SOCKET_RESULT_PREVOPEN)
    {
        scktPtr->socketCtrls[socketId].flushing = cleanSession;
        scktPtr->socketCtrls[socketId].dataPending = true;
        PRINTF(dbgColor_white, "Priming rxStream sckt=%d\r", socketId);
        sckt_doWork();
    }
    return atResult.statusCode;
}



/**
 *	\brief Close an established (open) connection socket.
 *
 *	\param socketId [in] - The socket ID or number for the connection to close.
 */
void sckt_close(uint8_t socketId)
{
    char closeCmd[20] = {0};
    uint8_t socketBitMap = 0x01 << socketId;

    if (iopPtr->peerTypeMap.tcpudpSocket & socketBitMap)                            // socket ID is an open TCP/UDP session
    {
        snprintf(closeCmd, 20, "AT+QICLOSE=%d", socketId);                                      // BGx syntax different for TCP/UDP and SSL
        iopPtr->peerTypeMap.tcpudpSocket = iopPtr->peerTypeMap.tcpudpSocket & ~socketBitMap;    // mask off closed socket bit to remove 
    }
    else if (iopPtr->peerTypeMap.sslSocket & socketBitMap)                          // socket ID is an open SSL session
    {
        snprintf(closeCmd, 20, "AT+QSSLCLOSE=%d", socketId);
        iopPtr->peerTypeMap.sslSocket = iopPtr->peerTypeMap.sslSocket & ~socketBitMap;  
    }
    else
        return;

    if (action_tryInvoke(closeCmd))
    {
        if (action_awaitResult(true).statusCode == RESULT_CODE_SUCCESS)
        {
            scktPtr->socketCtrls[socketId].protocol = protocol_void;
            scktPtr->socketCtrls[socketId].socketId = socketId;
            scktPtr->socketCtrls[socketId].open = false;
            scktPtr->socketCtrls[socketId].receiver_func = NULL;
        }
    }
}



/**
 *	\brief Reset open socket connection. This function drains the connection's data pipeline. 
 *
 *	\param socketId [in] - The connection socket to reset.
 *
 *  \return True if flush socket data initiated.
 */
bool sckt_flush(uint8_t socketId)
{
    if (scktPtr->socketCtrls[socketId].protocol == protocol_void)
        return;

    return s_requestIrdData(socketId, true);          // return status; failure is unable to obtain lock
}



/**
 *  \brief Close out all TCP/IP sockets on a context.
 *
 *	\param contxtId [in] - The carrier PDP context hosting the sockets to close.
*/
void sckt_closeAll(uint8_t contxtId)
{
    for (size_t i = 0; i < IOP_SOCKET_COUNT; i++)
    {
        if (scktPtr->socketCtrls[i].pdpContextId == contxtId)
        {
            ip_close(i);
        }
    }
}



bool sckt_getState(uint8_t socketId)
{
    char sendCmd[DFLT_ATBUFSZ] = {0};


    // AT+QISEND command initiates send, signals we plan to send on a socket a number of bytes, send has subcommand so don't automatically close
    snprintf(sendCmd, DFLT_ATBUFSZ, "AT+QISTATE=1,%d", socketId);

    if (!action_tryInvokeAdv(sendCmd, ACTION_RETRIES_DEFAULT, ACTION_RETRY_INTERVALmillis, s_socketStatusParser))
        return RESULT_CODE_CONFLICT;

    actionResult_t atResult = action_awaitResult(true);
    return atResult.statusCode;

}



/**
 *	\brief Send data to an established endpoint via protocol used to open socket (TCP/UDP/TCP INCOMING).
 *
 *	\param socketId [in] - The connection socket returned from open.
 *	\param data [in] - A character pointer containing the data to send.
 *  \param dataSz [in] - The size of the buffer (< 1501 bytes).
 */
socketResult_t sckt_send(socketId_t socketId, const char *data, uint16_t dataSz)
{
    char sendCmd[DFLT_ATBUFSZ] = {0};
    actionResult_t atResult = { .statusCode = RESULT_CODE_SUCCESS };

    // // advance receiver tasks prior to attmpting send
    // // some receive functions are blocking, this increases likelihood of a send without need to retry
    // sckt_doWork();

    if (scktPtr->socketCtrls[socketId].protocol > protocol_AnyIP || !scktPtr->socketCtrls[socketId].open)
        return RESULT_CODE_BADREQUEST;

    // AT+QISEND command initiates send, signals we plan to send on a socket a number of bytes, send has subcommand so don't automatically close
    snprintf(sendCmd, DFLT_ATBUFSZ, "AT+QISEND=%d,%d", socketId, dataSz);

    if (!action_tryInvokeAdv(sendCmd, ACTION_RETRIES_DEFAULT, ACTION_RETRY_INTERVALmillis, iop_txDataPromptParser))
        return RESULT_CODE_CONFLICT;

    atResult = action_awaitResult(false);                   // waiting for data prompt, leaving action open on return if sucessful

    // await data prompt atResult successful, now send data sub-command to actually transfer data, now automatically close action after data sent
    if (atResult.statusCode == RESULT_CODE_SUCCESS)
    {
        action_sendRaw(data, dataSz, 0, s_socketSendCompleteParser);
        atResult = action_awaitResult(true);
    }
    return atResult.statusCode;                             // return sucess -OR- failure from sendRequest\sendRaw action
}



/**
 *   \brief Perform background tasks to move socket data through pipeline, deliver RX data to application and update socket\IOP status values.
*/
void sckt_doWork()
{
    /* push data pipeline forward for existing data buffers 
    ----------------------------------------------------- */

    if (iopPtr->rxDataPeer < iopDataPeer__SOCKET_CNT)             // if any socket open
    {
        for (size_t bufIndx = 0; bufIndx < IOP_RX_DATABUFFERS_MAX; bufIndx++) 
        {
            if (iopPtr->rxDataBufs[bufIndx] == NULL)              // rxDataBufs expands as needed, break if past end of allocated buffers
                break;

            iopBuffer_t *buf = iopPtr->rxDataBufs[bufIndx];       // for convenience

            // check data buffers for missing IRD length, happens after 1st chunk of IRD data is received
            // parse for IRD length
            // *** Example: \r\n+QIRD: 142\r\n  where 142 is the number of chars arriving

            if (buf->dataPeer < iopDataPeer__SOCKET_CNT && buf->irdSz == 0 && buf->head > buf->buffer)     // irdSz not set && buffer has atleast 1 data chunk
            {
                // char dbg[65] = {0};
                // strncpy(dbg, buf->buffer, 64);
                // PRINTF(0,"SdWrcv>>%s<<\r", dbg);

                char *irdSzAt = buf->buffer + 9;                // strlen data prefix from BGx >> "\r\n+QIRD: " = 9
                buf->irdSz = strtol(irdSzAt, &buf->tail, 10);   // parse out data size from IRD response

                if (buf->irdSz > 0)                             // test for data complete
                {
                    buf->tail += 2;                             // move buf->head pointer to data (past line separator)
                                                                // test for buffer fill complete: if complete, hand off to socket
                    buf->dataReady = buf->head - buf->tail == buf->irdSz + 8;           // + 8 : trailing /r/n/r/nOK/r/n
                    if (buf->dataReady)
                    {
                        scktPtr->socketCtrls[iopPtr->rxDataPeer].dataBufferIndx = iopPtr->rxDataBufIndx;   // take buffer from IOP
                        iopPtr->rxDataBufIndx = IOP_NO_BUFFER;                                                            // IOP releases buffer
                    }
                }
                else                                            // irdSz size of 0: recv event completed, pipeline is empty, buffer can be released
                {
                    PRINTFC(dbgColor_dGreen, "closeIRD sckt=%d\r", buf->dataPeer);

                    buf->dataReady = false;
                    scktPtr->socketCtrls[buf->dataPeer].dataBufferIndx = IOP_NO_BUFFER;
                    scktPtr->socketCtrls[buf->dataPeer].dataPending = false;
                    scktPtr->socketCtrls[buf->dataPeer].flushing = false;

                    iop_resetDataBuffer(bufIndx);           // delivered, reset buffer and reset ready
                    iopPtr->rxDataBufIndx = IOP_NO_BUFFER;
                    iopPtr->rxDataPeer = iopDataPeer__NONE;
                    action_close();
                }
            }

            // push tail passed IRD header to actual appl data
            if (buf->dataReady)                                 // buffer data ready, signal to application
            {
                socketCtrl_t sckt = scktPtr->socketCtrls[buf->dataPeer];

                if (!sckt.flushing)
                {
                    // data ready event, send to application
                    // invoke application socket receiver_func: socket number, data pointer, number of bytes in buffer
                    scktPtr->socketCtrls[buf->dataPeer].receiver_func(sckt.socketId, buf->tail, buf->irdSz);
                }

                iop_resetDataBuffer(sckt.dataBufferIndx);                           // delivered, clear buffer
                PRINTFC(dbgColor_dGreen, "SCKT-nextIRD sckt=%d\r", sckt.socketId);
                s_requestIrdData(sckt.socketId, false);                             // check the data pipeline for more data
            }
        }
    }


    /* open a data pipeline from sockets sources
    ------------------------------------------------ */

    // IRD is a data peer, if no data peer active (can only have one) look to see if any sockets have dataPending
    // socket dataPending goes true when URC recv is reported by BGx
    if ((iopPtr->peerTypeMap.sslSocket != 0 || iopPtr->peerTypeMap.tcpudpSocket != 0) && iopPtr->rxDataPeer == iopDataPeer__NONE)
    {
        for (uint8_t sckt = 0; sckt < iopDataPeer__SOCKET_CNT; sckt++)
        {
            if (scktPtr->socketCtrls[sckt].dataPending)
            {
                PRINTFC(dbgColor_dGreen, "SCKT-openIRD sckt=%d\r", sckt);
                s_requestIrdData(sckt, true);           /* Request data (IRD) with action lock */
                
                break;                                  // If the IRD request gets a lock, the IRD process starts for the data pending socket
                                                        // If the request cannot get a lock (maybe a send\transmit cmd is underway) it silently 
                                                        // returns
                                                        // The IRD process is a true BGx action that block other foreground actions until the 
                                                        // pipeline is emptied and no more data is pending. This releases the lock and allows
                                                        // other types of commands to be sent to BGx. 
            }
        }
    }
}


#pragma endregion



#pragma region private local static functions
/* 
 * --------------------------------------------------------------------------------------------- */


/**
 *  \brief [private] Invoke IRD command to request BGx for socket (read) data
*/
static bool s_requestIrdData(iopDataPeer_t dataPeer, bool applyLock)
{
    char irdCmd[24] = {0};

    ASSERT(dataPeer < iopDataPeer__SOCKET_CNT, "Non-socket IRD request");

    if (scktPtr->socketCtrls[dataPeer].protocol == protocol_ssl)
        snprintf(irdCmd, 24, "AT+QSSLRECV=%d,%d", dataPeer, MIN(IRD_REQ_MAXSZ, IOP_RX_DATABUF_SZ));
    else
        snprintf(irdCmd, 24, "AT+QIRD=%d,%d", dataPeer, MIN(IRD_REQ_MAXSZ, IOP_RX_DATABUF_SZ));

    // PRINTF(dbgColor_white, "rqstIrd lck=%d, cmd=%s\r", applyLock, irdCmd);

    if (applyLock && !actn_acquireLock(irdCmd, IRD_RETRIES) )
        return false;

    iopPtr->rxDataPeer = dataPeer;
    iop_txSend(irdCmd, strlen(irdCmd), false);
    iop_txSend(ASCII_sCR, 1, true);
    return true;
}



/**
 *	\brief [private] TCP/UDP wrapper for open connection parser.
 */
static resultCode_t s_tcpudpOpenCompleteParser(const char *response, char **endptr) 
{
    return action_serviceResponseParser(response, "+QIOPEN: ", 1, endptr);
}


/**
 *	\brief [private] SSL wrapper for open connection parser.
 */
static resultCode_t s_sslOpenCompleteParser(const char *response, char **endptr) 
{
    return action_serviceResponseParser(response, "+QSSLOPEN: ", 1, endptr);
}


/**
 *	\brief [private] SSL wrapper for open connection parser.
 */
static resultCode_t s_socketSendCompleteParser(const char *response, char **endptr)
{
    return action_defaultResultParser(response, "", false, 0, ASCII_sSENDOK, endptr);
}

/**
 *	\brief [private] MQTT connect to server response parser.
 *
 *  \param response [in] Character data recv'd from BGx to parse for task complete
 *  \param endptr [out] Char pointer to the char following parsed text
 * 
 *  \return HTTP style result code, 0 = not complete
 */
static resultCode_t s_socketStatusParser(const char *response, char **endptr) 
{
    // BGx +QMTCONN Read returns Status = 3 for connected, service parser returns 203
    return action_serviceResponseParser(response, "+QISTATE: ", 5, endptr) == 202 ? RESULT_CODE_SUCCESS : RESULT_CODE_UNAVAILABLE;
}




#pragma endregion
