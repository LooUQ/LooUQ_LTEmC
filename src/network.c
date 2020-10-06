/******************************************************************************
 *  \file network.c
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
 *****************************************************************************/

#include "ltem1c.h"

//#define _DEBUG
#include "dbgprint.h"


#define PROTOCOLS_CMD_BUFFER_SZ 80

static resultCode_t contextStatusCompleteParser(const char *response, char **endptr);
static networkOperator_t getNetworkOperator();


/* public tcpip functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions

/**
 *	\brief Initialize the IP network contexts structure.
 */
network_t *ntwk_create()
{
    network_t *network = calloc(1, sizeof(network_t));
	if (network == NULL)
	{
        ltem1_faultHandler(0, "ipProtocols-could not alloc IP protocol struct");
	}

    network->networkOperator = calloc(1, sizeof(networkOperator_t));

    pdpContext_t *context = calloc(NTWK_CONTEXT_COUNT, sizeof(pdpContext_t));
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
 *   \brief Wait for a network operator name and network mode. Can be cancelled in threaded env via g_ltem1->cancellationRequest.
 * 
 *   \param waitDuration [in] Number of seconds to wait for a network. Supply 0 for no wait.
 * 
 *   \return Struct containing the network operator name (operName) and network mode (ntwkMode).
*/
networkOperator_t ntwk_awaitOperator(uint16_t waitDuration)
{
    networkOperator_t ntwk;
    unsigned long startMillis, endMillis;

    startMillis = timing_millis();
    waitDuration = waitDuration * 1000;
    do 
    {
        ntwk = getNetworkOperator();
        if (ntwk.operName[0] != '\0')
            break;
        timing_delay(1000);
        endMillis = timing_millis();
    } while (endMillis - startMillis < waitDuration || g_ltem1->cancellationRequest);
    //       timed out waiting                      || global cancellation
    return ntwk;
}



/**
 *	\brief Get APN active status and optionally activate.
 * 
 *  \param[in] contextNum The APN to operate on. Typically 0 or 1
 *  \param[in] activate Attempt to activate the APN if found not active.
 */
resultCode_t ntwk_fetchDataContexts()
{
    #define IP_QIACT_SZ 8

    action_tryInvokeAdv("AT+QIACT?", ACTION_RETRIES_DEFAULT, ACTION_TIMEOUT_DEFAULTmillis, contextStatusCompleteParser);
    actionResult_t atResult = action_awaitResult(false);

    if (atResult.statusCode == RESULT_CODE_SUCCESS)
    {
        if (strlen(atResult.response) > IP_QIACT_SZ)
        {
            #define TOKEN_BUF_SZ 16
            char *nextContext;
            char *landmarkAt;
            char *continueAt;
            uint8_t landmarkSz = IP_QIACT_SZ;
            char tokenBuf[TOKEN_BUF_SZ];

            nextContext = strstr(atResult.response, "+QIACT: ");

            // no contexts returned = none active (only active contexts are returned)
            if (nextContext == NULL)
            {
                for (size_t i = 0; i < NTWK_CONTEXT_COUNT; i++)
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
            atResult.statusCode = RESULT_CODE_SUCCESS;
        }
        else
        {
            for (size_t i = 0; i < NTWK_CONTEXT_COUNT; i++)
            {
                g_ltem1->network->contexts[i].contextState = context_state_inactive;
                g_ltem1->network->contexts[i].ipAddress[0] = ASCII_cNULL;
            }
            atResult.statusCode = RESULT_CODE_NOTFOUND;
        }
    }
    action_close();
    return atResult.statusCode;
}



/**
 *	\brief Activate APN.
 * 
 *  \param[in] contextNum The APN to operate on. Typically 0 or 1
 */
resultCode_t ntwk_activateContext(uint8_t contextNum)
{
    char atCmd[PROTOCOLS_CMD_BUFFER_SZ] = {0};

    snprintf(atCmd, PROTOCOLS_CMD_BUFFER_SZ, "AT+QIACT=%d\r", contextNum);
    if (action_tryInvokeAdv(atCmd, ACTION_RETRIES_DEFAULT, ACTION_TIMEOUT_DEFAULTmillis, contextStatusCompleteParser))
    {
        return action_awaitResult(true).statusCode;
    }
    return RESULT_CODE_CONFLICT;
}



/**
 *	\brief Deactivate APN.
 * 
 *  \param[in] contextNum The APN to operate on. Typically 0 or 1
 */
resultCode_t ntwk_deactivateContext(uint8_t contxtId)
{
    ntwk_closeContext(contxtId);

    char atCmd[PROTOCOLS_CMD_BUFFER_SZ] = {0};
    snprintf(atCmd, PROTOCOLS_CMD_BUFFER_SZ, "AT+QIDEACT=%d\r", contxtId);

    if (action_tryInvokeAdv(atCmd, ACTION_RETRIES_DEFAULT, ACTION_TIMEOUT_DEFAULTmillis, contextStatusCompleteParser))
    {
        g_ltem1->network->contexts[contxtId].contextState = context_state_inactive;
        g_ltem1->network->contexts[contxtId].ipAddress[0] = ASCII_cNULL;
        return action_awaitResult(true).statusCode;
    }
    return RESULT_CODE_CONFLICT;
}



/**
 *  \brief Close out all TCP/IP sockets on a context.
*/
void ntwk_closeContext(uint8_t contxtId)
{
    for (size_t i = 0; i < IOP_SOCKET_COUNT; i++)
    {
        if (g_ltem1->sockets->socketCtrls[i].pdpContextId == contxtId)
        {
            ip_close(i);
        }
    }
    
}


#pragma endregion


/* private functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions


/**
 *   \brief Tests for the completion of a network APN context activate action.
 * 
 *   \return standard action result integer (http result).
*/
static resultCode_t contextStatusCompleteParser(const char *response, char **endptr)
{
    return action_defaultResultParser(response, "+QIACT: ", false, 2, ASCII_sOK, endptr);
}



/**
 *   \brief Get the network operator name and network mode.
 * 
 *   \return Struct containing the network operator name (operName) and network mode (ntwkMode).
*/
static networkOperator_t getNetworkOperator()
{
    if (*g_ltem1->network->networkOperator->operName == NULL)
    {
        if (action_tryInvoke("AT+COPS?"))
        {
            actionResult_t atResult = action_awaitResult(false);
            if (atResult.statusCode == RESULT_CODE_SUCCESS)
            {
                char *continueAt;
                uint8_t ntwkMode;
                continueAt = strchr(atResult.response, ASCII_cDBLQUOTE);
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
            action_close();
        }
    }
    return *g_ltem1->network->networkOperator;
}


#pragma endregion
