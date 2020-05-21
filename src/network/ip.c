/******************************************************************************
 *  \file ip.c
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
 *****************************************************************************/

#include "..\ltem1c.h"

#define PROTOCOLS_CMD_BUFFER_SZ 80

static actionResult_t ipOpenCompleteParser(const char *response);
static actionResult_t sslOpenCompleteParser(const char *response);
static actionResult_t openCompleteParser(const char *response, const char *landmark);
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
socketResult_t ip_open(protocol_t protocol, const char *host, uint16_t rmtPort, uint16_t lclPort, receiver_func_t rcvr_func)
{
    if (rcvr_func == NULL) return ACTION_RESULT_ERROR;
    if (protocol > protocol_AnyIP) return ACTION_RESULT_BADREQUEST;

    if (protocol ==  protocol_tcpListener || protocol == protocol_udpService)
        strcpy(host, "127.0.0.1");

    char openCmd[PROTOCOLS_CMD_BUFFER_SZ] = {0};
    char protoName[13] = {0};
    socketId_t socketId = ACTION_RESULT_BUSY;

    for (size_t i = 0; i < LTEM1_SOCKET_COUNT; i++)
    {
        if (g_ltem1->protocols->sockets[i].protocol == protocol_none)
        {
            g_ltem1->protocols->sockets[i].protocol = protocol;
            socketId = i;
            break;
        }
    }
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
    snprintf(openCmd, PROTOCOLS_CMD_BUFFER_SZ, "AT+QIOPEN=%d,%d,\"%s\",\"%s\",%d", g_ltem1->dataContext, socketId, protoName, host, rmtPort);

    char *response [ACTION_DEFAULT_RESPONSE_SZ] = {0};

    action_tryInvoke(openCmd, true);
    actionResult_t cmdResult = action_awaitResult(response, ACTION_DEFAULT_RESPONSE_SZ, 0, ipOpenCompleteParser, true);

    // //testing
    // (*g_ltem1->protocols->sockets[socketNum].ipReceiver_func)(0);
    // //testing

    return (cmdResult == ACTION_RESULT_SUCCESS) ? socketId : cmdResult;
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
        actionResult_t atResult = action_awaitResult(response, ACTION_DEFAULT_RESPONSE_SZ, 0, NULL, true);

        if (atResult == ACTION_RESULT_SUCCESS)
        {
            g_ltem1->protocols->sockets[socketNum].protocol = protocol_none;
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
    actionResult_t actionResult = 200;

    if (g_ltem1->protocols->sockets[socketId].protocol == protocol_none)
        return ACTION_RESULT_BADREQUEST;

    // AT+QISEND command initiates send, signal plan to send with size
    snprintf(sendCmd, 30, "AT+QISEND=%d,%d", socketId, dataSz);
    if ( !action_tryInvoke(sendCmd, true) )
        return ACTION_RESULT_BUSY;
    do
    {
        actionResult = action_getResult(response, ACTION_DEFAULT_RESPONSE_SZ, 0, sendDataPromptParser, false);
    } while (actionResult == ACTION_RESULT_PENDING);

    // after prompt for data, now complete sub-command to actually transfer data
    if (actionResult == ACTION_RESULT_SUCCESS)
    {
        action_sendData(data, dataSz);
        do
        {
            actionResult = action_getResult(response, ACTION_DEFAULT_RESPONSE_SZ, 0, NULL, true);
        } while (actionResult == ACTION_RESULT_PENDING);
    }

    return actionResult;
}



/**
 *   \brief Perform background tasks to move data through pipeline and update status values.
*/
void ip_receiverDoWork()
{
    // check for timed-out IRD request
    if (g_ltem1->action->invokedAt + g_ltem1->action->timeoutMillis < timing_millis() &&
        strncmp(g_ltem1->action->cmdStr, "AT+IRD", 6) == 0 )
    {
        action_tryInvoke(g_ltem1->action->cmdStr, false);
    }

    /*
    * Check each socket for OPEN condition and NOT EMPTY (recv)
    * 
    * -- empty means no URC message and an IRD response with 0 length
    */

    for (uint8_t scktId = 0; scktId < LTEM1_SOCKET_COUNT; scktId++)
    {
        // service each open protocol recv actions
        if (g_ltem1->protocols->sockets[scktId].protocol < protocol_AnyIP)
        {
            uint8_t head = g_ltem1->iop->socketHead[scktId];
            uint8_t tail = g_ltem1->iop->socketTail[scktId];

            if (g_ltem1->iop->rxCtrlBlks[head].occupied)                    // socket has data
            {
                PRINTF_GRAY("\rhead=%d ", head);
                PRINTF_GRAY("tail=%d ", tail);
                PRINTF_GRAY("tail.occ=%d ", g_ltem1->iop->rxCtrlBlks[tail].occupied);

                if (g_ltem1->iop->rxCtrlBlks[tail].occupied)
                {
                    // if (g_ltem1->iop->rxCtrlBlks[tail].isURC || g_ltem1->iop->socketIrdBytes[scktId] > 0)
                    if (g_ltem1->protocols->sockets[scktId].dataPending || g_ltem1->iop->socketIrdBytes[scktId] > 0)
                    {
                        PRINTF_GRAY("dataPending=%d ", g_ltem1->protocols->sockets[scktId].dataPending);
                        PRINTF_GRAY("tail.sz=%d ", g_ltem1->iop->rxCtrlBlks[tail].primSz);
                        PRINTF_GRAY("irdBytes=%d\r", g_ltem1->iop->socketIrdBytes[scktId]);

                        if (g_ltem1->iop->socketIrdBytes[scktId] > 0)               // send buffer data to application
                        {
                            char *data;
                            char rmtHost[16];
                            char rmtPort[6];
                            uint16_t dataSz;

                            dataSz = iop_rxGetSocketQueued(scktId, &data, rmtHost, rmtPort);
                            (*g_ltem1->protocols->sockets[scktId].receiver_func)(scktId, data, dataSz, rmtHost, rmtPort);
                        }

                        g_ltem1->iop->irdSocket = scktId;
                        char irdCmd[12] = {0};
                        snprintf(irdCmd, 12, "AT+QIRD=%d", scktId);
                        bool irdSent = action_tryInvoke(irdCmd, false);             // send IRD request to queue next recv'd data, if success: done with buffer

                        if (irdSent)
                            iop_tailFinalize(scktId);                               // - clear, release, and advance tail
                        // debugging
                        else
                            PRINTF_WARN("IRD DEFERRED");
                        // debugging
                    }
                }
                else
                    iop_tailFinalize(scktId);                               // - tail needs advanced
            }
        }
    }
}



/**
 *	\brief [private] Response parser looking for ">" prompt to send data to network and then initiates the send.
 */
actionResult_t sendDataPromptParser(const char *response)
{
    if (strstr(response, "> ") != NULL)
        return ACTION_RESULT_SUCCESS;

    return ACTION_RESULT_PENDING;
}


#pragma endregion


/* private local (static) functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions


/**
 *	\brief [private] Parser for open connection response, shared by IP/SSL.
 */
static actionResult_t protoOpenCompleteParser(const char *response, const char *landmark) 
{
    char *errorAt;
    char *landmarkAt = strstr(response, landmark);

    if (landmarkAt == NULL)
        return ACTION_RESULT_PENDING;
    
    uint16_t connection = strtol(landmarkAt + strlen(landmark), &errorAt, 10);
    uint16_t errorNum = strtol(errorAt + 1, NULL, 10);
    return  errorNum == 0 ? ACTION_RESULT_SUCCESS : errorNum;
}


/**
 *	\brief [private] TCP/UDP wrapper for open connection parser.
 */
static actionResult_t ipOpenCompleteParser(const char *response) 
{
    return protoOpenCompleteParser(response, "+QIOPEN: ");
}


/**
 *	\brief [private] SSL wrapper for open connection parser.
 */
static actionResult_t sslOpenCompleteParser(const char *response) 
{
    return protoOpenCompleteParser(response, "+QSSLOPEN: ");
}


#pragma endregion
