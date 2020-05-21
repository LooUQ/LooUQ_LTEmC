/******************************************************************************
 *  \file ip.c
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
 *****************************************************************************/

#include "..\ltem1c.h"

#define PROTOCOLS_CMD_BUFFER_SZ 80

static actionResult_t contextStatusCompleteParser(const char *response);

/* public tcpip functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions

/**
 *	\brief Initialize the IP network contexts structure.
 */
network_t *ntwk_createNetwork()
{
    network_t *network = calloc(LTEM1_SOCKET_COUNT, sizeof(network_t));
	if (network == NULL)
	{
        ltem1_faultHandler(0, "ipProtocols-could not alloc IP protocol struct");
	}

    network->networkOperator = calloc(1, sizeof(networkOperator_t));

    pdpContext_t *context = calloc(LTEM1_SOCKET_COUNT, sizeof(pdpContext_t));
	if (context == NULL)
	{
        ltem1_faultHandler(0, "ipProtocols-could not alloc IP protocol struct");
        free(network);
	}

    for (size_t i = 0; i < IOP_SOCKET_COUNT; i++)
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
void ntwk_destroyNetwork(void *network)
{
	free(network);
}



/**
 *	\brief Initialize the IP protocols structure.
 */
protocols_t *ntwk_createProtocols()
{
    protocols_t *protocols = calloc(LTEM1_SOCKET_COUNT, sizeof(protocols_t));
	if (protocols == NULL)
	{
        ltem1_faultHandler(0, "ipProtocols-could not alloc IP protocol struct");
	}

    socketCtrl_t *socket = calloc(LTEM1_SOCKET_COUNT, sizeof(socketCtrl_t));
	if (socket == NULL)
	{
        ltem1_faultHandler(0, "ipProtocols-could not alloc IP protocol struct");
        free(protocols);
	}

    for (size_t i = 0; i < IOP_SOCKET_COUNT; i++)
    {   
        protocols->sockets[i].protocol = protocol_none;
        protocols->sockets[i].contextId = g_ltem1->dataContext;
        protocols->sockets[i].receiver_func = NULL;
    }
    return protocols;
}


/**
 *	\brief Tear down IP protocols (TCP/UDP/SSL).
 */
void ntwk_destroyProtocols(void *ipProtocols)
{
	free(ipProtocols);
}



/**
 *   \brief Get the network operator name and network mode.
 * 
 *   \return Struct containing the network operator name (operName) and network mode (ntwkMode).
*/
networkOperator_t ntwk_getOperator()
{
    char response[ACTION_DEFAULT_RESPONSE_SZ] = {0};

    if (*g_ltem1->network->networkOperator->operName == NULL)
    {
        action_tryInvoke("AT+COPS?", true);
        actionResult_t cmdResult = action_awaitResult(response, ACTION_DEFAULT_RESPONSE_SZ, 0, NULL, true);

        char *continueAt;
        uint8_t ntwkMode;

        if (cmdResult == ACTION_RESULT_SUCCESS)
        {
            continueAt = strchr(response, ASCII_cDBLQUOTE);
            if (continueAt != NULL)
            {
                continueAt = strToken(continueAt + 1, ASCII_cDBLQUOTE, g_ltem1->network->networkOperator->operName, NTWKOPERATOR_OPERNAME_SZ);
                ntwkMode = (uint8_t)strtol(continueAt + 1, &continueAt, 10);
                if (ntwkMode == 8)
                    strcpy(g_ltem1->network->networkOperator->ntwkMode, "CAT-M1");
                else
                    strcpy(g_ltem1->network->networkOperator->ntwkMode, "CAT-NB1");
            }
            else
            {
                g_ltem1->network->networkOperator->operName[0] = NULL;
                g_ltem1->network->networkOperator->ntwkMode[0] = NULL;
            }
        }
    }
    return *g_ltem1->network->networkOperator;
}



/**
 *	\brief Get APN active status and optionally activate.
 * 
 *  \param[in] contextNum The APN to operate on. Typically 0 or 1
 *  \param[in] activate Attempt to activate the APN if found not active.
 */
socketResult_t ntwk_fetchDataContexts()
{
    #define IP_QIACT_SZ 8

    char response[ACTION_DEFAULT_RESPONSE_SZ] = {0};

    action_tryInvoke("AT+QIACT?", true);
    actionResult_t cmdResult = action_awaitResult(response, ACTION_DEFAULT_RESPONSE_SZ, 0, contextStatusCompleteParser, true);

    if (cmdResult == ACTION_RESULT_SUCCESS)
    {
        if (strlen(response) > IP_QIACT_SZ)
        {
            #define TOKEN_BUF_SZ 16
            char *nextContext;
            char *landmarkAt;
            char *continueAt;
            uint8_t landmarkSz = IP_QIACT_SZ;
            char tokenBuf[TOKEN_BUF_SZ];

            nextContext = strstr(response, "+QIACT: ");

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
            return ACTION_RESULT_ERROR;
        }
    }
    return cmdResult;                           // return parser error
}



/**
 *	\brief Activate APN.
 * 
 *  \param[in] contextNum The APN to operate on. Typically 0 or 1
 */
socketResult_t ntwk_activateContext(uint8_t contextNum)
{
    char response[ACTION_DEFAULT_RESPONSE_SZ] = {0};
    char atCmd[PROTOCOLS_CMD_BUFFER_SZ] = {0};

    snprintf(atCmd, PROTOCOLS_CMD_BUFFER_SZ, "AT+QIACT=%d\r", contextNum);
    if (action_tryInvoke(atCmd, true))
    {
        actionResult_t cmdResult = action_awaitResult(response, ACTION_DEFAULT_RESPONSE_SZ, 0, contextStatusCompleteParser, true);
        return cmdResult;
    }
    return ACTION_RESULT_BUSY;
}



/**
 *	\brief Deactivate APN.
 * 
 *  \param[in] contextNum The APN to operate on. Typically 0 or 1
 */
socketResult_t ntwk_deactivateContext(uint8_t contxtId)
{
    char response[ACTION_DEFAULT_RESPONSE_SZ] = {0};

    ntwk_closeContext(contxtId);

    char atCmd[PROTOCOLS_CMD_BUFFER_SZ] = {0};
    snprintf(atCmd, PROTOCOLS_CMD_BUFFER_SZ, "AT+QIDEACT=%d\r", contxtId);
    action_tryInvoke(atCmd, true);

    g_ltem1->network->contexts[contxtId].contextState = context_state_inactive;
    g_ltem1->network->contexts[contxtId].ipAddress[0] = ASCII_cNULL;

    actionResult_t cmdResult = action_awaitResult(response, ACTION_DEFAULT_RESPONSE_SZ, 0, contextStatusCompleteParser, true);
    return cmdResult;
}



/**
 *  \brief Close out all TCP/IP sockets on a context.
*/
void ntwk_closeContext(uint8_t contxtId)
{
    for (size_t i = 0; i < IOP_SOCKET_COUNT; i++)
    {
        if (g_ltem1->protocols->sockets[i].contextId == contxtId)
        {
            ip_close(i);
        }
    }
    
}


#pragma endregion


/* private functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions


static actionResult_t contextStatusCompleteParser(const char *response)
{
    return action_gapResultParser(response, "+QIACT: ", false, 2, ASCII_sOK);
}


#pragma endregion
