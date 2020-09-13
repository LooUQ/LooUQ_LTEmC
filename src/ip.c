/******************************************************************************
 *  \file ip.c
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
 *****************************************************************************/

#include "ltem1c.h"

#define _DEBUG
#include "dbgprint.h"

#define PROTOCOLS_CMD_BUFFER_SZ 80

static actionResult_t ipOpenCompleteParser(const char *response);
static actionResult_t sslOpenCompleteParser(const char *response);
static actionResult_t recvIrdPromptParser(const char *response);

/* public tcpip (ip) functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions


/**
 *	\brief Open a data connection (socket) to d data to an established endpoint via protocol used to open socket (TCP/UDP/TCP INCOMING).
 *
 *	\param[in] protocol The IP protocol to use for the connection (TCP/UDP/TCP LISTENER/UDP SERVICE/SSL).
 *	\param[in] host The IP address (string) or domain name of the remote host to communicate with.
 *  \param[in] rmtPort The port number at the remote host.
 *  \param[in] lclPort The port number on this side of the conversation, set to 0 to auto-assign.
 *  \param[in] ipReceiver_func The callback function in your application to be notified of received data ready.
 */
socketResult_t ip_open(socketId_t socketId, protocol_t protocol, const char *host, uint16_t rmtPort, uint16_t lclPort, receiver_func_t rcvr_func)
{
    char openCmd[PROTOCOLS_CMD_BUFFER_SZ] = {0};
    char protoName[13] = {0};

    if (socketId >= IOP_SOCKET_COUNT ||
        g_ltem1->protocols->sockets[socketId].protocol != protocol_void ||
        protocol > protocol_AnyIP ||
        rcvr_func == NULL
        )
        return ACTION_RESULT_BADREQUEST;

    g_ltem1->protocols->sockets[socketId].protocol = protocol;
    g_ltem1->protocols->sockets[socketId].receiver_func = rcvr_func;

    switch (protocol)
    {
    case protocol_udp:
        strcpy(protoName, "UDP");
        break;
    case protocol_tcp:
        strcpy(protoName, "TCP");
        break;
    case protocol_udpService:
        strcpy(protoName, "UDP SERVICE");
        break;
    case protocol_tcpListener:
        strcpy(protoName, "TCP LISTENER");
        break;
    default:
        break;
    }
    if (protocol ==  protocol_tcpListener || protocol == protocol_udpService)
    {
        strcpy(host, "127.0.0.1");
    }

    snprintf(openCmd, PROTOCOLS_CMD_BUFFER_SZ, "AT+QIOPEN=%d,%d,\"%s\",\"%s\",%d", g_ltem1->dataContext, socketId, protoName, host, rmtPort);
    char *response [ACTION_DEFAULT_RESPONSE_SZ] = {0};

    action_tryInvoke(openCmd, true);
    actionResult_t cmdResult = action_awaitResult(response, ACTION_DEFAULT_RESPONSE_SZ, 0, ipOpenCompleteParser);

    // //testing
    // (*g_ltem1->protocols->sockets[socketNum].ipReceiver_func)(0);
    // //testing

    return cmdResult;
}



/**
 *	\brief Close an established (open) connection socket.
 *
 *	\param [in] socketNum - The connection socket to close.
 */
void ip_close(uint8_t socketNum)
{
    char closeCmd[20] = {0};
    char response[ACTION_DEFAULT_RESPONSE_SZ] = {0};

    // get socket from g_ltem1->protocols
    // if socket openCmd    
    // - close
    snprintf(closeCmd, 20, "AT+QICLOSE=%d", socketNum);
    if (action_tryInvoke(closeCmd, true))
    {
        actionResult_t atResult = action_awaitResult(response, ACTION_DEFAULT_RESPONSE_SZ, 0, NULL);

        if (atResult == ACTION_RESULT_SUCCESS)
        {
            g_ltem1->protocols->sockets[socketNum].protocol = protocol_void;
            g_ltem1->protocols->sockets[socketNum].receiver_func = NULL;
        }
    }

    // TODO dry pipeline for socket
}



/**
 *	\brief Send data to an established endpoint via protocol used to open socket (TCP/UDP/TCP INCOMING).
 *
 *	\param [in] - socketNum The connection socket returned from open.
 *	\param [in] - sendBuf A buffer pointer containing the data to send.
 *  \param [in] - sendSz The size of the buffer (< 1501 bytes).
 */
socketResult_t ip_send(socketId_t socketId, const char *data, uint16_t dataSz, const char *rmtHost, const char *rmtPort)
{
    char sendCmd[30] = {0};
    char response[ACTION_DEFAULT_RESPONSE_SZ] = {0};
    actionResult_t actionResult = ACTION_RESULT_SUCCESS;

    if (g_ltem1->protocols->sockets[socketId].protocol == protocol_void)
        return ACTION_RESULT_BADREQUEST;

    // AT+QISEND command initiates send, signal plan to send along with send bytes, send has subcommand so don't automatically close
    action_setAutoClose(false);
    snprintf(sendCmd, 30, "AT+QISEND=%d,%d", socketId, dataSz);
    if ( !action_tryInvoke(sendCmd, true) )
        return ACTION_RESULT_CONFLICT;
    do
    {
        actionResult = action_getResult(response, ACTION_DEFAULT_RESPONSE_SZ, 0, iop_txDataPromptParser);
    } while (actionResult == ACTION_RESULT_PENDING);
    action_setAutoClose(true);

    // after prompt for data, now complete sub-command to actually transfer data, now automatically close action after data sent
    if (actionResult == ACTION_RESULT_SUCCESS)
    {
        action_sendData(data, dataSz);
        do
        {
            actionResult = action_getResult(response, ACTION_DEFAULT_RESPONSE_SZ, 0, NULL);
        } while (actionResult == ACTION_RESULT_PENDING);
    }

    return actionResult;
}



/**
 *   \brief Perform background tasks to move data through pipeline and update status values.
*/
void ip_recvDoWork()
{
    // // check for timed-out IRD request
    // if (g_ltem1->action->invokedAt + g_ltem1->action->timeoutMillis < timing_millis() &&
    //     strncmp(g_ltem1->action->cmdStr, "AT+IRD", 6) == 0 )
    // {
    //     action_tryInvoke(g_ltem1->action->cmdStr, false);
    // }

    /*
    * Check each socket for OPEN condition and NOT EMPTY (recv)
    * -- empty means no URC message and a received IRD response with length=0
    */

    for (uint8_t scktId = 0; scktId < LTEM1_SOCKET_COUNT; scktId++)
    {
        // if socket is open (protocol_AnyIP) and hasData: move data to application
        if (g_ltem1->protocols->sockets[scktId].protocol < protocol_AnyIP && g_ltem1->protocols->sockets[scktId].hasData)
        {
            PRINTF(dbgColor_gray, "\rhead=%d ", g_ltem1->iop->socketHead[scktId]);
            PRINTF(dbgColor_gray, "tail=%d ", g_ltem1->iop->socketTail[scktId]);
            PRINTF(dbgColor_gray, "tail.occ=%d\r", IOP_RXCTRLBLK_ISOCCUPIED(g_ltem1->iop->socketTail[scktId]) && g_ltem1->iop->rxCtrlBlks[g_ltem1->iop->socketTail[scktId]].dataReady);


            // get data from IOP
            char *data;
            char rmtHost[16];
            char rmtPort[6];
            uint16_t dataSz;
            dataSz = iop_rxGetSocketQueued(scktId, &data, rmtHost, rmtPort);
            if (dataSz > 0)
            {
                // push recvd data to application
                (*g_ltem1->protocols->sockets[scktId].receiver_func)(scktId, data, dataSz, rmtHost, rmtPort);
            }

            iop_tailFinalize(scktId);
            g_ltem1->protocols->sockets[scktId].hasData = IOP_RXCTRLBLK_ISOCCUPIED(g_ltem1->iop->socketTail[scktId]) && g_ltem1->iop->rxCtrlBlks[g_ltem1->iop->socketTail[scktId]].dataReady;
        }
    }
}



#pragma endregion


/* private local (static) functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions


/**
 *	\brief [private] TCP/UDP wrapper for open connection parser.
 */
static actionResult_t ipOpenCompleteParser(const char *response) 
{
    return action_serviceResponseParser(response, "+QIOPEN: ", 1);
}


/**
 *	\brief [private] SSL wrapper for open connection parser.
 */
static actionResult_t sslOpenCompleteParser(const char *response) 
{
    return action_serviceResponseParser(response, "+QSSLOPEN: ", 1);
}


#pragma endregion
