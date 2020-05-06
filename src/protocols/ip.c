/******************************************************************************
 *  \file ip.c
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
 *****************************************************************************/

#include "..\ltem1c.h"

#define PROTOCOLS_CMD_BUFFER_SZ 80

static action_result_t ipOpenCompleteParser(const char *response);
static action_result_t sslOpenCompleteParser(const char *response);
static action_result_t openCompleteParser(const char *response, const char *landmark);

static action_result_t sendPromptParser(const char *response);
static action_result_t recvIrdPromptParser(const char *response);


/* public tcpip functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions

/**
 *	\brief Initialize the IP network contexts structure.
 */
network_t *ip_createNetwork()
{
    network_t *network = calloc(LTEM1_SOCKET_COUNT, sizeof(network_t));
	if (network == NULL)
	{
        ltem1_faultHandler("ipProtocols-could not alloc IP protocol struct");
	}

    pdp_context_t *context = calloc(LTEM1_SOCKET_COUNT, sizeof(pdp_context_t));
	if (context == NULL)
	{
        ltem1_faultHandler("ipProtocols-could not alloc IP protocol struct");
        free(network);
	}

    for (size_t i = 0; i < IOP_PROTOCOLS_COUNT; i++)
    {   
        network->contexts[i].contextState = context_state_inactive;
        network->contexts[i].contextType = context_type_IPV4;
        network->contexts[i].ipAddress[0] = '\0';
    }
    return network;
}


/**
 *	\brief Tear down IP protocols (TCP/UDP/SSL).
 */
void ip_destroyNetwork(void *network)
{
	free(network);
}



/**
 *	\brief Initialize the IP protocols structure.
 */
protocols_t *ip_createProtocols()
{
    protocols_t *protocols = calloc(LTEM1_SOCKET_COUNT, sizeof(protocols_t));
	if (protocols == NULL)
	{
        ltem1_faultHandler("ipProtocols-could not alloc IP protocol struct");
	}

    socket_t *socket = calloc(LTEM1_SOCKET_COUNT, sizeof(socketCtrl_t));
	if (socket == NULL)
	{
        ltem1_faultHandler("ipProtocols-could not alloc IP protocol struct");
        free(protocols);
	}

    for (size_t i = 0; i < IOP_PROTOCOLS_COUNT; i++)
    {   
        protocols->sockets[i].protocol = protocol_none;
        protocols->sockets[i].contextId = g_ltem1->dataContext;
        protocols->sockets[i].ipReceiver_func = NULL;
    }
    return protocols;
}


/**
 *	\brief Tear down IP protocols (TCP/UDP/SSL).
 */
void ip_destroyProtocols(void *ipProtocols)
{
	free(ipProtocols);
}




static action_result_t contextStatusCompleteParser(const char *response)
{
    return action_gapResultParser(response, "+QIACT: ", false, 2, ASCII_sOK);
}


/**
 *	\brief Get APN active status and optionally activate.
 * 
 *  \param[in] contextNum The APN to operate on. Typically 0 or 1
 *  \param[in] activate Attempt to activate the APN if found not active.
 */
socket_result_t ip_fetchNetworkContexts()
{
    #define IP_QIACT_SZ 8

    action_invokeWithParser("AT+QIACT?", contextStatusCompleteParser);
    action_result_t cmdResult = action_awaitResult(NULL);

    if (cmdResult == ACTION_RESULT_SUCCESS)
    {
        if (strlen(g_ltem1->dAction->resultHead) > IP_QIACT_SZ)
        {
            #define TOKEN_BUF_SZ 16
            char *nextContext;
            char *landmarkAt;
            char *continueAt;
            uint8_t landmarkSz = IP_QIACT_SZ;
            char tokenBuf[TOKEN_BUF_SZ];

            nextContext = strstr(g_ltem1->dAction->resultHead, "+QIACT: ");

            // no contexts returned = none active (only active contexts are returned)
            if (nextContext == NULL)
            {
                for (size_t i = 0; i < LTEM1_CONTEXT_COUNT; i++)
                {
                    g_ltem1->network->contexts[i].contextState = context_state_inactive;
                }
                
            }
            while (nextContext != NULL)                         // now parse each pdp context entry
            {
                landmarkAt = nextContext;
                uint8_t cntxt = strtol(landmarkAt + landmarkSz, &continueAt, 10);
                cntxt--;    //adjust for 0 base array

                g_ltem1->network->contexts[cntxt].contextState = (int)strtol(continueAt + 1, &continueAt, 10);
                g_ltem1->network->contexts[cntxt].contextType = (int)strtol(continueAt + 1, &continueAt, 10);
                continueAt = strToken(continueAt + 2, ASCII_cDBLQUOTE, tokenBuf, TOKEN_BUF_SZ);
                if (continueAt != NULL)
                {
                    strncpy(g_ltem1->network->contexts[cntxt].ipAddress, tokenBuf, TOKEN_BUF_SZ);
                }
                nextContext = strstr(nextContext + landmarkSz, "+QIACT: ");
            }
            return ACTION_RESULT_SUCCESS;
        }
        else
        {
            for (size_t i = 0; i < LTEM1_CONTEXT_COUNT; i++)
            {
                g_ltem1->network->contexts[i].contextState = context_state_inactive;
                g_ltem1->network->contexts[i].ipAddress[0] = ASCII_cNULL;
            }
            return PROTOCOL_RESULT_UNAVAILABLE;
        }
    }
    return cmdResult;                           // return parser error
}



/**
 *	\brief Activate APN.
 * 
 *  \param[in] contextNum The APN to operate on. Typically 0 or 1
 */
socket_result_t ip_activateContext(uint8_t contextNum)
{
    char atCmd[PROTOCOLS_CMD_BUFFER_SZ] = {0};
    snprintf(atCmd, PROTOCOLS_CMD_BUFFER_SZ, "AT+QIACT=%d\r", contextNum);
    action_invoke(atCmd);

    action_result_t cmdResult = action_awaitResult(NULL);
    return cmdResult;
}



/**
 *	\brief Deactivate APN.
 * 
 *  \param[in] contextNum The APN to operate on. Typically 0 or 1
 */
socket_result_t ip_deactivateContext(uint8_t contextNum)
{
    char atCmd[PROTOCOLS_CMD_BUFFER_SZ] = {0};
    snprintf(atCmd, PROTOCOLS_CMD_BUFFER_SZ, "AT+QIDEACT=%d\r", contextNum);
    action_invoke(atCmd);

    g_ltem1->network->contexts[contextNum].contextState = context_state_inactive;
    g_ltem1->network->contexts[contextNum].ipAddress[0] = ASCII_cNULL;

    action_result_t cmdResult = action_awaitResult(NULL);
    return cmdResult;
}



/**
 *	\brief Open a data connection (socket) to d data to an established endpoint via protocol used to open socket (TCP/UDP/TCP INCOMING).
 *
 *	\param[in] protocol The IP protocol to use for the connection (TCP/UDP/TCP LISTENER/UDP SERVICE/SSL).
 *	\param[in] host The IP address (string) or domain name of the remote host to communicate with.
 *  \param[in] rmtPort The port number at the remote host.
 *  \param[in] lclPort The port number on this side of the conversation, set to 0 to auto-assign.
 *  \param[in] ipReceiver_func The callback function in your application to be notified of received data ready.
 */
socket_result_t ip_open(protocol_t protocol, const char *host, uint16_t rmtPort, uint16_t lclPort, void (*ipReceiver_func)(socket_t socketNum))
{
    /*
    AT+QIOPEN=1,0,"UDP","97.83.32.119",9001,0,1

    OK

    +QIOPEN: 0,0
    */

    if (ipReceiver_func == NULL) return PROTOCOL_RESULT_ERROR;
    if (protocol > protocol_AnyIP) return PROTOCOL_RESULT_ERROR;

    if (protocol ==  protocol_tcpListener || protocol == protocol_udpService)
        strcpy(host, "127.0.0.1");

    char openCmd[PROTOCOLS_CMD_BUFFER_SZ] = {0};
    char protoName[13] = {0};
    int8_t socketNum = PROTOCOL_RESULT_ERROR;

    for (size_t i = 0; i < LTEM1_SOCKET_COUNT; i++)
    {
        if (g_ltem1->protocols->sockets[i].protocol == protocol_none)
        {
            g_ltem1->protocols->sockets[i].protocol = protocol;
            socketNum = i;
            break;
        }
    }
    g_ltem1->protocols->sockets[socketNum].protocol = protocol;
    g_ltem1->protocols->sockets[socketNum].ipReceiver_func = ipReceiver_func;


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
    snprintf(openCmd, PROTOCOLS_CMD_BUFFER_SZ, "AT+QIOPEN=%d,%d,\"%s\",\"%s\",%d", g_ltem1->dataContext, socketNum, protoName, host, rmtPort);
    action_invokeWithParser(openCmd, ipOpenCompleteParser);
    action_result_t cmdResult = action_awaitResult(NULL);

    //testing
    (*g_ltem1->protocols->sockets[socketNum].ipReceiver_func)(0);
    //testing

    return (cmdResult == ACTION_RESULT_SUCCESS) ? socketNum : cmdResult;
}



/**
 *	\brief Close an established (open) connection socket.
 *
 *	\param[in] socketNum The connection socket to close.
 */
void ip_close(uint8_t socketNum)
{
    char closeCmd[20] = {0};

    // get socket from g_ltem1->protocols
    // if socket openCmd    
    // - close
    snprintf(closeCmd, 20, "AT+QICLOSE=%d", socketNum);
    action_invoke(closeCmd);
    action_result_t atResult = action_awaitResult(NULL);

    if (atResult == ACTION_RESULT_SUCCESS)
    {
        g_ltem1->protocols->sockets[socketNum].protocol = protocol_none;
        g_ltem1->protocols->sockets[socketNum].ipReceiver_func = NULL;
    }
}



/**
 *	\brief Send data to an established endpoint via protocol used to open socket (TCP/UDP/TCP INCOMING).
 *
 *	\param[in] socketNum The connection socket returned from open.
 *	\param[in] sendBuf A buffer pointer containing the data to send.
 *  \param[in] sendSz The size of the buffer (< 1501 bytes).
 */
socket_result_t ip_send(uint8_t socketNum, char *sendData, uint16_t sendSz)
{
    if (g_ltem1->iop->iopState != iop_state_idle)
        return PROTOCOL_RESULT_UNAVAILABLE;

    char sendCmd[30] = {0};
    snprintf(sendCmd, 30, "AT+QISEND=%d,%d", socketNum, sendSz);
    action_invokeWithParser(sendCmd, sendPromptParser);

    action_result_t actionReslt;
    do
    {
        actionReslt = action_getResult(NULL, false);

    } while (actionReslt == ACTION_RESULT_PENDING);

    if (actionReslt == ACTION_RESULT_SUCCESS)
    {
        action_sendData(sendData, sendSz);
        actionReslt = action_awaitResult(g_ltem1->dAction);
    }
    return actionReslt;
}


// TODO collapse into above ip_send

// protocol_result_t ip_sendUdpReply(uint8_t socketNum, const char *rmtHost, uint16_t rmtPort,  char *sendBuf, uint8_t sendSz)
// {
//     // AT+QISEND=2,10,“10.7.89.10”,6969         // UDP SERVICE

//     char sendCmd[60] = {0};
//     snprintf(sendCmd,60, "AT+QISEND=%d,%d,\"%s\",%d", socketNum, sendSz, rmtHost, rmtPort);

    
//     return PROTOCOL_RESULT_SUCCESS;
// }



/**
 *   \brief Pull data from socket into application (ip protocol) 
 * 
 *   \param[in] socketNm The open socket identifier to check for data.
 */ 
uint16_t ip_recv(socket_t socketNm, char *recvBuf, uint16_t recvBufSz)
{
    return iop_rxGetSocketQueued(socketNm, recvBuf, recvBufSz);
}



/**
 *   \brief Perform background tasks to move data through pipeline and update status values.
*/
void ip_receiverDoWork()
{
    /*
    * Check each socket for OPEN condition and NOT EMPTY (recv)
    * 
    * -- empty means no URC message and an IRD response with 0 length
    */

    for (uint8_t scktNm = 0; scktNm < LTEM1_SOCKET_COUNT; scktNm++)
    {
        // service each open protocol recv actions
        if (g_ltem1->protocols->sockets[scktNm].protocol < protocol_AnyIP)
        {
            uint8_t head = g_ltem1->iop->socketHead[scktNm];
            uint8_t tail = g_ltem1->iop->socketTail[scktNm];

            if (g_ltem1->iop->rxCtrlBlks[head].occupied)                    // socket has data
            {
                PRINTF_WARN("\rhead=%d ", head);
                PRINTF_WARN("tail=%d ", tail);
                PRINTF_WARN("tail.occ=%d ", g_ltem1->iop->rxCtrlBlks[tail].occupied);

                // if (!g_ltem1->iop->rxCtrlBlks[tail].occupied )
                //     tail = head;

                if (g_ltem1->iop->rxCtrlBlks[tail].occupied)
                {
                    //process tail
                    if (g_ltem1->iop->rxCtrlBlks[tail].isURC || g_ltem1->iop->socketIrdBytes[scktNm] > 0)
                    {
                        PRINTF_WARN("isURC=%d ", g_ltem1->iop->rxCtrlBlks[tail].isURC);
                        PRINTF_WARN("tail.sz=%d ", g_ltem1->iop->rxCtrlBlks[tail].primSz);
                        PRINTF_WARN("irdBytes=%d\r", g_ltem1->iop->socketIrdBytes[scktNm]);

                        g_ltem1->iop->irdSocket = scktNm;
                        char irdCmd[12] = {0};                                  // do not await IRD response (ISR does that)
                        snprintf(irdCmd, 12, "AT+QIRD=%d", scktNm);
                        action_invokeWithParser(irdCmd, NULL);                  // send IRD request to get recv'd data
                    }
                    if (g_ltem1->iop->socketIrdBytes[scktNm] > 0)           // process IRD (to application notification)
                    {
                        (*g_ltem1->protocols->sockets[scktNm].ipReceiver_func)(scktNm);
                    }
                }
                iop_tailFinalize(scktNm);                                   // - tail needs advanced, cleared
            }
        }
    }
}


#pragma endregion


/* private local (static) functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions


/**
 *	\brief [private] Parser for open connection response, shared by IP/SSL.
 */
static action_result_t protoOpenCompleteParser(const char *response, const char *landmark) 
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
static action_result_t ipOpenCompleteParser(const char *response) 
{
    return protoOpenCompleteParser(response, "+QIOPEN: ");
}

/**
 *	\brief [private] SSL wrapper for open connection parser.
 */
static action_result_t sslOpenCompleteParser(const char *response) 
{
    return protoOpenCompleteParser(response, "+QSSLOPEN: ");
}


/**
 *	\brief [private] Response parser looking for ">" prompt to send data to network and then initiates the send.
 */
static action_result_t sendPromptParser(const char *response)
{
    char *landmarkAt = strstr(response, "> ");
    if (landmarkAt != NULL)
        return ACTION_RESULT_SUCCESS;

    return ACTION_RESULT_PENDING;
}



// /**
//  *	\brief [private] BG96 response parser looking for IRD prompt to read received data from network.
//  */
// static action_result_t recvIrdPromptParser(const char *response)
// {
//      //Data length is 12. The remote IP address is 10.7.76.34 and remote port is 7687.
//     // +QIRD: 12,“10.7.76.34”,7687\r\n<~~~DATA~~~>\r\n\r\nOK

// }


#pragma endregion
