/******************************************************************************
 *  \file ip.c
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
 *****************************************************************************/

#include "ltem1c.h"

#define _DEBUG
//#include "dbgprint.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define SOCKETS_CMDBUF_SZ 80
#define IRD_RETRIES 10
#define IRD_REQ_MAXSZ 1500

#define ASCII_sSENDOK "SEND OK\r\n"


// local function declarations
static void requestIrdData(iopDataPeer_t dataPeer, bool applyLock);
static resultCode_t tcpudpOpenCompleteParser(const char *response, char **endptr);
static resultCode_t sslOpenCompleteParser(const char *response, char **endptr);
static resultCode_t socketSendCompleteParser(const char *response, char **endptr);


/* public sockets (IP:TCP/UDP/SSL) functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions


/**
 *	\brief Initialize the IP protocols structure.
 */
sockets_t *sckt_create()
{
    sockets_t *sockets = calloc(1, sizeof(sockets_t));
	if (sockets == NULL)
	{
        ltem1_faultHandler(0, "ipProtocols-could not alloc IP protocol struct");
	}

    // avoiding #define IOP_SOCKET_PEERS
    socketCtrl_t *socket = calloc(iopDataPeer__SOCKET_CNT, sizeof(socketCtrl_t));
	if (socket == NULL)
	{
        ltem1_faultHandler(0, "ipProtocols-could not alloc IP protocol struct");
        free(sockets);
	}

    for (size_t i = 0; i < IOP_SOCKET_COUNT; i++)
    {   
        sockets->socketCtrls[i].protocol = protocol_void;
        sockets->socketCtrls[i].dataBufferIndx = IOP_NO_BUFFER;
        sockets->socketCtrls[i].pdpContextId = g_ltem1->dataContext;
        sockets->socketCtrls[i].receiver_func = NULL;
        sockets->socketCtrls[i].dataBufferIndx = IOP_NO_BUFFER;
    }
    return sockets;
}



/**
 *	\brief Open a data connection (socket) to d data to an established endpoint via protocol used to open socket (TCP/UDP/TCP INCOMING).
 *
 *	\param [in] socketId - The ID or number specifying the socket connect to open.
 *	\param [in] protocol - The IP protocol to use for the connection (TCP/UDP/TCP LISTENER/UDP SERVICE/SSL).
 *	\param [in] host - The IP address (string) or domain name of the remote host to communicate with.
 *  \param [in] rmtPort - The port number at the remote host.
 *  \param [in] lclPort - The port number on this side of the conversation, set to 0 to auto-assign.
 *  \param [in] rcvr_func The callback function in your application to be notified of received data ready.
 */
socketResult_t sckt_open(socketId_t socketId, protocol_t protocol, const char *host, uint16_t rmtPort, uint16_t lclPort, receiver_func_t rcvr_func)
{
    char openCmd[SOCKETS_CMDBUF_SZ] = {0};
    char protoName[13] = {0};

    if (socketId >= IOP_SOCKET_COUNT ||
        g_ltem1->sockets->socketCtrls[socketId].protocol != protocol_void ||
        protocol > protocol_AnyIP ||
        rcvr_func == NULL
        )
        return RESULT_CODE_BADREQUEST;

        uint8_t socketBitMap = 0x01;

    switch (protocol)
    {
    case protocol_udp:
        strcpy(protoName, "UDP");
        socketBitMap = socketBitMap<<socketId;
        g_ltem1->iop->peerTypeMap.tcpudpSocket = g_ltem1->iop->peerTypeMap.tcpudpSocket | socketBitMap;
        break;

    case protocol_tcp:
        strcpy(protoName, "TCP");
        socketBitMap = socketBitMap<<socketId;
        g_ltem1->iop->peerTypeMap.tcpudpSocket = g_ltem1->iop->peerTypeMap.tcpudpSocket | socketBitMap;
        break;

    case protocol_ssl:
        strcpy(protoName, "SSL");
        socketBitMap = socketBitMap<<socketId;
        g_ltem1->iop->peerTypeMap.sslSocket = g_ltem1->iop->peerTypeMap.sslSocket | socketBitMap;
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

    default:
        break;
    }

    snprintf(openCmd, SOCKETS_CMDBUF_SZ, "AT+QIOPEN=%d,%d,\"%s\",\"%s\",%d", g_ltem1->dataContext, socketId, protoName, host, rmtPort);

    action_tryInvokeAdv(openCmd, ACTION_RETRIES_DEFAULT, ACTION_TIMEOUT_DEFAULTmillis, tcpudpOpenCompleteParser);
    actionResult_t atResult = action_awaitResult(true);

    // finish initialization and run background tasks to prime data pipeline
    if (atResult.statusCode == RESULT_CODE_SUCCESS || atResult.statusCode == SOCKET_RESULT_PREVOPEN)
    {
        g_ltem1->sockets->socketCtrls[socketId].protocol = protocol;
        g_ltem1->sockets->socketCtrls[socketId].socketId = socketId;
        g_ltem1->sockets->socketCtrls[socketId].open = true;
        g_ltem1->sockets->socketCtrls[socketId].receiver_func = rcvr_func;

        // force dataPending with initializing to issue data request (IRD) to flush data pipeline
        //g_ltem1->sockets->socketCtrls[socketId].initializing = true;
        //g_ltem1->sockets->socketCtrls[socketId].dataPending = true;
        // while (g_ltem1->sockets->socketCtrls[socketId].dataPending)
        // {
        //     sckt_doWork();
        // }
    }
    else        // failed to open, reset peerMap bits
    {
        uint8_t socketBitMap = socketBitMap<<socketId;
        g_ltem1->iop->peerTypeMap.tcpudpSocket = g_ltem1->iop->peerTypeMap.tcpudpSocket & ~socketBitMap;
        g_ltem1->iop->peerTypeMap.sslSocket = g_ltem1->iop->peerTypeMap.sslSocket & ~socketBitMap;
    }

    return atResult.statusCode;
}



/**
 *	\brief Close an established (open) connection socket.
 *
 *	\param [in] socketId - The socket ID or number for the connection to close.
 */
void sckt_close(uint8_t socketId)
{
    char closeCmd[20] = {0};

    uint8_t socketBitMap = socketBitMap<<socketId;
    g_ltem1->iop->peerTypeMap.tcpudpSocket = g_ltem1->iop->peerTypeMap.tcpudpSocket & ~socketBitMap;
    g_ltem1->iop->peerTypeMap.sslSocket = g_ltem1->iop->peerTypeMap.sslSocket & ~socketBitMap;

    snprintf(closeCmd, 20, "AT+QICLOSE=%d", socketId);
    if (action_tryInvoke(closeCmd))
    {
        if (action_awaitResult(true).statusCode == RESULT_CODE_SUCCESS)
        {
            g_ltem1->sockets->socketCtrls[socketId].protocol = protocol_void;
            g_ltem1->sockets->socketCtrls[socketId].socketId = socketId;
            g_ltem1->sockets->socketCtrls[socketId].open = false;
            g_ltem1->sockets->socketCtrls[socketId].receiver_func = NULL;
        }
    }
}



/**
 *	\brief Reset open socket connection. This function drains the connection's data pipeline. 
 *
 *	\param [in] socketId - The connection socket to reset.
 */
void sckt_reset(uint8_t socketId)
{
    if (g_ltem1->sockets->socketCtrls[socketId].protocol == protocol_void)
        return;
    requestIrdData(socketId, true);
}



/**
 *	\brief Send data to an established endpoint via protocol used to open socket (TCP/UDP/TCP INCOMING).
 *
 *	\param [in] socketId - The connection socket returned from open.
 *	\param [in] data - A character pointer containing the data to send.
 *  \param [in] dataSz - The size of the buffer (< 1501 bytes).
 */
socketResult_t sckt_send(socketId_t socketId, const char *data, uint16_t dataSz)
{
    char sendCmd[30] = {0};
    actionResult_t atResult = { .statusCode = RESULT_CODE_SUCCESS };

    // // advance receiver tasks prior to attmpting send
    // // some receive functions are blocking, this increases likelihood of a send without need to retry
    // sckt_doWork();

    if (g_ltem1->sockets->socketCtrls[socketId].protocol > protocol_AnyIP || !g_ltem1->sockets->socketCtrls[socketId].open)
        return RESULT_CODE_BADREQUEST;

    // AT+QISEND command initiates send, signal plan to send along with send bytes, send has subcommand so don't automatically close
    snprintf(sendCmd, 30, "AT+QISEND=%d,%d", socketId, dataSz);
    if ( !action_tryInvokeAdv(sendCmd, ACTION_RETRIES_DEFAULT, ACTION_RETRY_INTERVALmillis, iop_txDataPromptParser))
        return RESULT_CODE_CONFLICT;

    atResult = action_awaitResult(false);

    // after prompt for data, now complete sub-command to actually transfer data, now automatically close action after data sent
    if (atResult.statusCode == RESULT_CODE_SUCCESS)
    {
        action_sendRaw(data, dataSz, 0, socketSendCompleteParser);
        atResult = action_awaitResult(true);
    }

    return atResult.statusCode;
}



/**
 *   \brief Perform background tasks to move socket data through pipeline and update status values.
*/
void sckt_doWork()
{
    /* push data pipeline forward for existing data buffers 
    ----------------------------------------------------- */

    if (g_ltem1->iop->rxDataPeer < iopDataPeer__SOCKET_CNT)                // if any socket open
    {
        for (size_t bufIndx = 0; bufIndx < IOP_RX_DATABUFFERS_MAX; bufIndx++) 
        {
            if (g_ltem1->iop->rxDataBufs[bufIndx] == NULL)              // rxDataBufs expands as needed, break if past end of allocated buffers
                break;

            iopBuffer_t *buf = g_ltem1->iop->rxDataBufs[bufIndx];       // for convenience

            // check data buffers for missing IRD length, happens after 1st chunk of IRD data is received
            // parse for IRD length
            // *** Example: \r\n+QIRD: 142\r\n  where 142 is the number of chars arriving

            if (buf->dataPeer < iopDataPeer__SOCKET_CNT && buf->irdSz == 0 && buf->head > buf->buffer)     // irdSz not set && buffer has atleast 1 data chunk
            {
                char *irdSzAt = buf->buffer + 9;                // strlen of "\r\n+QIRD: " = 9
                buf->irdSz = strtol(irdSzAt, &buf->tail, 10);   // parse out data size from IRD response

                if (buf->irdSz > 0)                             // test for data complete
                {
                    buf->tail += 2;                             // move buf->head pointer to data (past line separator)
                                                                // test for buffer fill complete: if complete, hand off to socket
                    buf->dataReady = buf->head - buf->tail == buf->irdSz + 8;           // + 8 : trailing /r/n/r/nOK/r/n
                    if (buf->dataReady)
                    {
                        g_ltem1->sockets->socketCtrls[g_ltem1->iop->rxDataPeer].dataBufferIndx = g_ltem1->iop->rxDataBufIndx;   // take buffer from IOP
                        g_ltem1->iop->rxDataBufIndx = IOP_NO_BUFFER;                                                            // IOP releases buffer
                    }
                }
                else                                            // irdSz size of 0: recv event completed, pipeline is empty, buffer can be released
                {
                    buf->dataReady = false;
                    g_ltem1->sockets->socketCtrls[buf->dataPeer].dataBufferIndx = IOP_NO_BUFFER;
                    g_ltem1->sockets->socketCtrls[buf->dataPeer].dataPending = false;
                    // if (g_ltem1->sockets->socketCtrls[buf->dataPeer].initializing)
                    // {
                    //     g_ltem1->sockets->socketCtrls[buf->dataPeer].initializing = false;
                    //     g_ltem1->sockets->socketCtrls[buf->dataPeer].dataPending = true;    // do one additional IRD to cover any timing race at open
                    // }
                    iop_resetDataBuffer(bufIndx);           // delivered, reset buffer and reset ready
                    g_ltem1->iop->rxDataBufIndx = IOP_NO_BUFFER;
                    g_ltem1->iop->rxDataPeer = iopDataPeer__NONE;
                    action_close();
                }
            }

            // push tail passed IRD header to actual appl data
            if (buf->dataReady)                                 // buffer data ready, signal to application
            {
                socketCtrl_t sckt = g_ltem1->sockets->socketCtrls[buf->dataPeer];

                // data ready event, send to application
                // invoke application socket receiver_func: socket number, data pointer, number of bytes in buffer
                g_ltem1->sockets->socketCtrls[buf->dataPeer].receiver_func(sckt.socketId, buf->tail, buf->irdSz);

                // if ( !g_ltem1->sockets->socketCtrls[buf->dataPeer].initializing)
                //     g_ltem1->sockets->socketCtrls[buf->dataPeer].receiver_func(sckt.socketId, buf->tail, buf->irdSz);

                iop_resetDataBuffer(sckt.dataBufferIndx);           // delivered, clear buffer
                requestIrdData(sckt.socketId, false);               // check the data pipeline for more data
            }
        }
    }


    /* initiate a data pipeline from sockets sources
    ------------------------------------------------ */

    // IRD is a data peer, if no data peer active (can only have one) look to see if any sockets have dataPending
    // socket dataPending goes true when URC recv is reported by BGx
    if ((g_ltem1->iop->peerTypeMap.sslSocket != 0 || g_ltem1->iop->peerTypeMap.tcpudpSocket != 0) && g_ltem1->iop->rxDataPeer == iopDataPeer__NONE)
    {
        for (uint8_t socket = 0; socket < iopDataPeer__SOCKET_CNT; socket++)
        {
            if (g_ltem1->sockets->socketCtrls[socket].dataPending)
            {
                requestIrdData(socket, true);             // request data (IRD) with action lock, init IRD request must blocks cmds 
                break;
            }
        }
    }
}


#pragma endregion


/* private local (static) functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions


/**
 *  \brief Invoke IRD command to request BGx for socket (read) data
*/
static void requestIrdData(iopDataPeer_t dataPeer, bool applyLock)
{
    char irdCmd[20] = {0};
    if (dataPeer < iopDataPeer__SOCKET_CNT)
    {
        if (g_ltem1->sockets->socketCtrls[dataPeer].protocol == protocol_ssl)
            snprintf(irdCmd, 20, "AT+QSSLRECV=%d,%d", dataPeer, MIN(IRD_REQ_MAXSZ, IOP_RX_DATABUF_SZ));
        else
            snprintf(irdCmd, 20, "AT+QIRD=%d,%d", dataPeer, MIN(IRD_REQ_MAXSZ, IOP_RX_DATABUF_SZ));
    }

    if (applyLock && !actn_acquireLock(irdCmd, IRD_RETRIES) )
        return;

    g_ltem1->iop->rxDataPeer = dataPeer;
    iop_txSend(irdCmd, strlen(irdCmd), false);
    iop_txSend(ASCII_sCR, 1, true);
}



/**
 *	\brief [private] TCP/UDP wrapper for open connection parser.
 */
static resultCode_t tcpudpOpenCompleteParser(const char *response, char **endptr) 
{
    return action_serviceResponseParser(response, "+QIOPEN: ", 1, endptr);
}



/**
 *	\brief [private] SSL wrapper for open connection parser.
 */
static resultCode_t sslOpenCompleteParser(const char *response, char **endptr) 
{
    return action_serviceResponseParser(response, "+QSSLOPEN: ", 1, endptr);
}

/**
 *	\brief [private] SSL wrapper for open connection parser.
 */
static resultCode_t socketSendCompleteParser(const char *response, char **endptr)
{
    return action_defaultResultParser(response, "", false, 0, ASCII_sSENDOK, endptr);
}

#pragma endregion
