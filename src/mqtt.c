/******************************************************************************
 *  \file mqtt.c
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
 *
 *****************************************************************************/

//#define _DEBUG
#include "ltem1c.h"
// debugging output options             UNCOMMENT one of the next two lines to direct debug (PRINTF) output
// #include <jlinkRtt.h>                   // output debug PRINTF macros to J-Link RTT channel
// #define SERIAL_OPT 1                    // enable serial port comm with devl host (1=force ready test)


#define WAIT_SECONDS(timeout) (timeout * 1000)
#define MIN(x, y) (((x) < (y)) ? (x) : (y))


#pragma region Static Local Function Declarations
static resultCode_t mqttOpenStatusParser(const char *response, char **endptr);
static resultCode_t mqttOpenCompleteParser(const char *response, char **endptr);
static resultCode_t mqttConnectStatusParser(const char *response, char **endptr);
static resultCode_t mqttConnectCompleteParser(const char *response, char **endptr);
static resultCode_t mqttSubscribeCompleteParser(const char *response, char **endptr);
static resultCode_t mqttPublishCompleteParser(const char *response, char **endptr);
static void urlDecode(char *src, int len);
#pragma endregion


/* public mqtt functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions


/**
 *	\brief Initialize the MQTT service structure.
 */
void mqtt_create()
{
    g_ltem1->mqtt = calloc(1, sizeof(mqtt_t));
	if (g_ltem1->mqtt == NULL)
	{
        ltem1_faultHandler(0, "mqtt-could not alloc mqtt service struct");
	}
    g_ltem1->mqtt->msgId = 1;
    g_ltem1->mqtt->dataBufferIndx = IOP_NO_BUFFER;
}



/**
 *  \brief Query the status of the MQTT server state.
 * 
 *  \param host [in] A char string to match with the currently connected server. Host is not checked if empty string passed.
 *  \param force [in] If true query BGx for current state, otherwise return internal g_ltem1 property
 * 
 *  \returns A mqttStatus_t value indicating the state of the MQTT connection.
*/
mqttStatus_t mqtt_status(const char *host, bool force)
{
    // See BG96_MQTT_Application_Note: AT+QMTOPEN? and AT+QMTCONN?

    mqttStatus_t mqttStatus = mqttStatus_closed;

    if (!force && host[0] == '\0')                          // if not forcing query and no host verification, return current internal prop value
        return g_ltem1->mqtt->state;

    // connect check first to short-circuit efforts
    if (action_tryInvokeAdv("AT+QMTCONN?", ACTION_RETRIES_DEFAULT, ACTION_TIMEOUT_DEFAULTmillis, mqttConnectStatusParser))
    {
        actionResult_t atResult = action_awaitResult(true);
        if (atResult.statusCode == RESULT_CODE_SUCCESS)
            mqttStatus = mqttStatus_connected;
    }
    // if not connected or need host verification: check open
    if (mqtt_status != mqttStatus_connected ||
        host[0] != '\0' ||
        action_tryInvokeAdv("AT+QMTOPEN?", ACTION_RETRIES_DEFAULT, ACTION_TIMEOUT_DEFAULTmillis, mqttOpenStatusParser))
    {
        actionResult_t atResult = action_awaitResult(true);
        if (atResult.statusCode != RESULT_CODE_SUCCESS)
            return mqttStatus;                              // default response: closed until proved otherwise

        if (host[0] == '\0')
            mqttStatus = mqttStatus_open;
        else                                                // host matching requested, check modem response host == requested host
        {
            char *hostNameAt = strchr(atResult.response, ASCII_cDBLQUOTE);
            mqttStatus = (hostNameAt != NULL && strncmp(hostNameAt + 1, host, strlen(host)) == 0) ? mqttStatus_open : mqttStatus_closed;
        }
    }
    return mqttStatus;

    // if (action_tryInvokeAdv("AT+QMTOPEN?", ACTION_RETRIES_DEFAULT, ACTION_TIMEOUT_DEFAULTmillis, mqttOpenStatusParser))
    // {
    //     actionResult_t atResult = action_awaitResult(true);
    //     if (atResult.statusCode != RESULT_CODE_SUCCESS)
    //         return mqttStatus;                              // default response: closed until proved otherwise

    //     if (host[0] == '\0')
    //         mqttStatus = mqttStatus_open;
    //     else                                                // host matching requested, check modem response host == requested host
    //     {
    //         char *hostNameAt = strchr(atResult.response, ASCII_cDBLQUOTE);
    //         mqttStatus = (hostNameAt != NULL && strncmp(hostNameAt + 1, host, strlen(host)) == 0) ? mqttStatus_open : mqttStatus_closed;
    //     }

    //     // if open, test for connected
    //     if (mqttStatus == mqttStatus_open && action_tryInvokeAdv("AT+QMTCONN?", ACTION_RETRIES_DEFAULT, ACTION_TIMEOUT_DEFAULTmillis, mqttConnectStatusParser))
    //     {
    //         actionResult_t atResult = action_awaitResult(true);
    //         if (atResult.statusCode == RESULT_CODE_SUCCESS)
    //             mqttStatus = mqttStatus_connected;
    //     }
    // }
    // return mqttStatus;
}



/**
 *  \brief Open a remote MQTT server for use.
 * 
 *  \param host [in] The host IP address or name of the remote server.
 *  \param port [in] The IP port number to use for the communications.
 *  \param useSslVersion [in] Specifies the version and options for use of SSL to protect communications.
 *  \param useMqttVersion [in] Specifies the MQTT protocol revision to use for communications.
 * 
 *  \returns A resultCode_t value indicating the success or type of failure.
*/
resultCode_t mqtt_open(const char *host, uint16_t port, sslVersion_t useSslVersion, mqttVersion_t useMqttVersion)
{
    // AT+QSSLCFG="sslversion",5,3
    // AT+QMTCFG="ssl",5,1,0
    // AT+QMTCFG="version",5,4
    // AT+QMTOPEN=5,"iothub-dev-pelogical.azure-devices.net",8883

    #define ACTION_CMD_SZ 81
    #define ACTION_RSP_SZ 81
    char actionCmd[ACTION_CMD_SZ] = {0};
    char actionResponse[ACTION_RSP_SZ] = {0};
    actionResult_t atResult;

    g_ltem1->mqtt->state = mqtt_status(host, false);    // refresh state, adjust mqtt start process as needed 
    if (g_ltem1->mqtt->state >= mqttStatus_connected)   // already open+connected with server "host"
        return RESULT_CODE_SUCCESS;

    if (useSslVersion != sslVersion_none)
    {
        snprintf(actionCmd, ACTION_CMD_SZ, "AT+QSSLCFG=\"sslversion\",%d,%d", MQTT_SOCKET_ID, (uint8_t)useSslVersion);
        if (action_tryInvoke(actionCmd))
        {
            if (action_awaitResult(true).statusCode != RESULT_CODE_SUCCESS)
                return RESULT_CODE_ERROR;
        }

        snprintf(actionCmd, ACTION_CMD_SZ, "AT+QMTCFG=\"ssl\",%d,1,%d", MQTT_SOCKET_ID, MQTT_SOCKET_ID);
        if (action_tryInvoke(actionCmd))
        {
            if (action_awaitResult(true).statusCode != RESULT_CODE_SUCCESS)
                return RESULT_CODE_ERROR;
        }
    }

    if (useMqttVersion == mqttVersion_311)
    {
        snprintf(actionCmd, ACTION_CMD_SZ, "AT+QMTCFG=\"version\",%d,4", MQTT_SOCKET_ID);
        if (action_tryInvoke(actionCmd))
        {
            if (action_awaitResult(true).statusCode != RESULT_CODE_SUCCESS)
                return RESULT_CODE_ERROR;
        }
    }

    // TYPICAL: AT+QMTOPEN=0,"iothub-dev-pelogical.azure-devices.net",8883
    snprintf(actionCmd, ACTION_CMD_SZ, "AT+QMTOPEN=%d,\"%s\",%d", MQTT_SOCKET_ID, host, port);
    if (action_tryInvokeAdv(actionCmd, ACTION_RETRIES_DEFAULT, WAIT_SECONDS(45), mqttOpenCompleteParser))
    {
        actionResult_t atResult = action_awaitResult(true);

        // if (atResult.statusCode == RESULT_CODE_SUCCESS)
        //     g_ltem1->iop->peerTypeMap.mqttConnection = 1;
        // else
        //     return RESULT_CODE_ERROR;

        switch (atResult.statusCode)
        {
            case RESULT_CODE_SUCCESS:
                g_ltem1->iop->peerTypeMap.mqttConnection = 1;
                return RESULT_CODE_SUCCESS;
            case 899:
            case 903:
            case 905:
                return RESULT_CODE_GONE;
            case 901:
                return RESULT_CODE_BADREQUEST;
            case 902:
                return RESULT_CODE_CONFLICT;
            case 904:
                return RESULT_CODE_NOTFOUND;
            default:
                return RESULT_CODE_ERROR;
        }
    }
    //g_ltem1->sockets->socketCtrls[MQTT_SOCKET_ID].protocol = (useSslVersion == sslVersion_none) ? protocol_mqtt : protocol_mqtts;
    //return RESULT_CODE_SUCCESS;
}


/*
"iothub-dev-pelogical.azure-devices.net"
"e8fdd7df-2ca2-4b64-95de-031c6b199299"
"iothub-dev-pelogical.azure-devices.net/e8fdd7df-2ca2-4b64-95de-031c6b199299/?api-version=2018-06-30"
*/

/**
 *  \brief Disconnect and close a connection to a MQTT server
 * 
 *  \param [in] socketId - The socket based identifier for the connection to close
*/
void mqtt_close()
{
    char actionCmd[81] = {0};
    char actionResponse[81] = {0};

    g_ltem1->mqtt->state = mqttStatus_closed;                 

    g_ltem1->iop->peerTypeMap.mqttConnection = 0;           // release mqtt socket in IOP
    for (size_t i = 0; i < IOP_RX_DATABUFFERS_MAX; i++)     // release any iop buffers assigned
    {
        if (g_ltem1->iop->rxDataBufs[i]->dataPeer == iopDataPeer_MQTT)
            g_ltem1->iop->rxDataBufs[i]->dataPeer = iopDataPeer__NONE;
    }
    for (size_t i = 0; i < MQTT_TOPIC_MAXCNT; i++)          // clear subscriptions table
    {
        g_ltem1->mqtt->subscriptions[i].topicName[0] = '0';
        g_ltem1->mqtt->subscriptions[i].receiver_func = NULL;
    }

    // if (g_ltem1->mqtt->state == mqttStatus_connected)
    // {
    //     snprintf(actionCmd, 80, "AT+QMTDISC=%d", MQTT_SOCKET_ID);
    //     if (action_tryInvoke(actionCmd))
    //         action_awaitResult(true);
    // }
    if (g_ltem1->mqtt->state >= mqttStatus_open)
    {
        snprintf(actionCmd, 80, "AT+QMTCLOSE=%d", MQTT_SOCKET_ID);
        if (action_tryInvoke(actionCmd))
            action_awaitResult(true);
    }

    g_ltem1->mqtt->state == mqttStatus_closed;
    return RESULT_CODE_SUCCESS;
}



/**
 *  \brief Connect (authenticate) to a MQTT server.
 * 
 *  \param [in] clientId - The client or device identifier for the connection.
 *  \param [in] username - The user identifier or name for the connection to authenticate.
 *  \param [in] password - The secret string or phrase to authenticate the connection.
 *  \param [in] sessionClean - Directs MQTT to preserve or flush messages received prior to the session start.
 * 
 *  \returns A resultCode_t value indicating the success or type of failure.
*/
resultCode_t mqtt_connect(const char *clientId, const char *username, const char *password, mqttSession_t sessionClean)
{
    #define MQTT_CONNECT_CMDSZ 400
    #define MQTT_CONNECT_RSPSZ 81

    char actionCmd[MQTT_CONNECT_CMDSZ] = {0};
    actionResult_t atResult;

    if (g_ltem1->mqtt->state == mqttStatus_connected)      // already connected
        return RESULT_CODE_SUCCESS;

    snprintf(actionCmd, ACTION_CMD_SZ, "AT+QMTCFG=\"session\",%d,%d", MQTT_SOCKET_ID, (uint8_t)sessionClean);
    if (action_tryInvoke(actionCmd))
    {
        if (action_awaitResult(true).statusCode != RESULT_CODE_SUCCESS)
            return RESULT_CODE_ERROR;
    }

    snprintf(actionCmd, MQTT_CONNECT_CMDSZ, "AT+QMTCONN=%d,\"%s\",\"%s\",\"%s\"", MQTT_SOCKET_ID, clientId, username, password);
    if (action_tryInvokeAdv(actionCmd, ACTION_RETRIES_DEFAULT, WAIT_SECONDS(60), mqttConnectCompleteParser))
    {
        atResult = action_awaitResult(true);

        // if (atResult.statusCode == RESULT_CODE_SUCCESS)
        //     g_ltem1->iop->peerTypeMap.mqttConnection = 2;
        // return atResult.statusCode;

        switch (atResult.statusCode)
        {
            case RESULT_CODE_SUCCESS:
                g_ltem1->iop->peerTypeMap.mqttConnection = 2;
                return RESULT_CODE_SUCCESS;
            case 901:
            case 902:
            case 904:
                return RESULT_CODE_BADREQUEST;
            case 903:
                return RESULT_CODE_UNAVAILABLE;
            case 905:
                return RESULT_CODE_FORBIDDEN;
            default:
                return RESULT_CODE_ERROR;
        }
    }
    return RESULT_CODE_BADREQUEST;          // bad parameters assumed
}



/**
 *  \brief Subscribe to a topic on the MQTT server.
 * 
 *  \param [in] topic - The messaging topic to subscribe to.
 *  \param [in] qos - The QOS level for messages subscribed to.
 *  \param [in] recv_func - The receiver function in the application to receive subscribed messages on arrival.
 * 
 *  \returns A resultCode_t value indicating the success or type of failure.
*/
resultCode_t mqtt_subscribe(const char *topic, mqttQos_t qos, mqtt_recvFunc_t recv_func)
{
    #define MQTT_PUBSUB_CMDSZ (MQTT_TOPIC_NAME_SZ + MQTT_TOPIC_PROPS_SZ + MQTT_TOPIC_PUBOVRHD_SZ)
    #define MQTT_PUBSUB_RSPSZ 81

    char actionCmd[MQTT_PUBSUB_CMDSZ] = {0};
    char actionResponse[MQTT_PUBSUB_RSPSZ] = {0};
    uint8_t subscriptionSlot = 255;

    if (recv_func == NULL || strlen(topic) > (MQTT_TOPIC_NAME_SZ + MQTT_TOPIC_PROPS_SZ- 1))
        return RESULT_CODE_BADREQUEST;

    uint16_t topicSz = strlen(topic);               // test for MQTT multilevel wildcard, store separately for future topic parsing on recv
    bool wildcard = *(topic + topicSz - 1) == '#';

    for (size_t i = 0; i < MQTT_TOPIC_MAXCNT; i++)
    {
        if (g_ltem1->mqtt->subscriptions[i].topicName[0] == ASCII_cNULL)
        {
            if (wildcard)                    
            {
                g_ltem1->mqtt->subscriptions[i].wildcard = '#';
                topicSz--;
            }
            strncpy(g_ltem1->mqtt->subscriptions[i].topicName, topic, MIN(topicSz, MQTT_TOPIC_NAME_SZ));
            g_ltem1->mqtt->subscriptions[i].topicName[topicSz] = ASCII_cNULL;                               // null out the wildcard char
            g_ltem1->mqtt->subscriptions[i].receiver_func = recv_func;
            recv_func = NULL;
            subscriptionSlot = i;
            break;
        }
    }
    if (subscriptionSlot == 255)
        return RESULT_CODE_CONFLICT;

    snprintf(actionCmd, MQTT_CONNECT_CMDSZ, "AT+QMTSUB=%d,%d,\"%s\",%d", MQTT_SOCKET_ID, ++g_ltem1->mqtt->msgId, topic, qos);
    if (action_tryInvokeAdv(actionCmd, ACTION_RETRIES_DEFAULT, WAIT_SECONDS(15), mqttSubscribeCompleteParser))
    {
        actionResult_t atResult = action_awaitResult(true);
        if (atResult.statusCode == RESULT_CODE_SUCCESS)
            g_ltem1->iop->peerTypeMap.mqttSubscribe = g_ltem1->iop->peerTypeMap.mqttSubscribe | (1 << subscriptionSlot);
        return atResult.statusCode;
    }
    return RESULT_CODE_BADREQUEST;
}



/**
 *  \brief Unsubscribe to a topic on the MQTT server.
 * 
 *  \param [in] topic - The messaging topic to unsubscribe from.
 * 
 *  \returns A resultCode_t (http status type) value indicating the success or type of failure.
*/
resultCode_t mqtt_unsubscribe(const char *topic)
{
    // AT+QMTUNS=<tcpconnectID>,<msgID>,"<topic1>"

    char actionCmd[MQTT_PUBSUB_CMDSZ] = {0};
    char actionResponse[81] = {0};
    uint8_t subscriptionSlot = 255;

    uint16_t topicSz = strlen(topic);                   // adjust topic if multilevel wildcard, remove prior to subscriptions scan
    if (*(topic + topicSz) == "#")
        topicSz--;

    for (size_t i = 0; i < MQTT_TOPIC_MAXCNT; i++)
    {
        if (strncmp(g_ltem1->mqtt->subscriptions[i].topicName, topic, topicSz) == 0)
        {
            g_ltem1->mqtt->subscriptions[i].topicName[0] = ASCII_cNULL;
            g_ltem1->mqtt->subscriptions[i].receiver_func = NULL;
            subscriptionSlot = i;
            break;
        }
    }
    
    snprintf(actionCmd, MQTT_CONNECT_CMDSZ, "AT+QMTUNS=%d,%d,\"<topic1>\"", MQTT_SOCKET_ID, ++g_ltem1->mqtt->msgId, topic);
    if (action_tryInvoke(actionCmd))
    {
        actionResult_t atResult = action_awaitResult(true);
        if (atResult.statusCode == RESULT_CODE_SUCCESS)
            g_ltem1->iop->peerTypeMap.mqttSubscribe = g_ltem1->iop->peerTypeMap.mqttSubscribe & (~(1 << subscriptionSlot));
        return atResult.statusCode;
    }
    return RESULT_CODE_BADREQUEST;
}


#define MQTT_PUBLISH_TIMEOUT 15000

/**
 *  \brief Publish a message to server.
 * 
 *  \param [in] *topic - The topic to receive the message on the server.
 *  \param [in] qos - The MQTT QOS to be assigned to sent message.
 *  \param [in] *message - Pointer to message to be sent.
 * 
 *  \returns A resultCode_t value indicating the success or type of failure (http status type code).
*/
resultCode_t mqtt_publish(const char *topic, mqttQos_t qos, const char *message)
{
    // AT+QMTPUB=<tcpconnectID>,<msgID>,<qos>,<retain>,"<topic>"

    #define PUBBUF_SZ (MQTT_TOPIC_NAME_SZ + MQTT_TOPIC_PROPS_SZ + MQTT_TOPIC_PUBOVRHD_SZ)

    char publishCmd[PUBBUF_SZ] = {0};
    char msgText[MQTT_MESSAGE_SZ];
    char actionResponse[81] = {0};
    actionResult_t atResult;

    // register the pending publish action
    uint16_t msgId = ((uint8_t)qos == 0) ? 0 : ++g_ltem1->mqtt->msgId;
    snprintf(publishCmd, PUBBUF_SZ, "AT+QMTPUB=%d,%d,%d,0,\"%s\"", MQTT_SOCKET_ID, msgId, qos, topic);
    
    if (action_tryInvokeAdv(publishCmd, ACTION_RETRIES_DEFAULT, ACTION_RETRY_INTERVALmillis, iop_txDataPromptParser))
    {
        atResult = action_awaitResult(false);

        // wait for data prompt for data, now complete sub-command to actually transfer data
        if (atResult.statusCode == RESULT_CODE_SUCCESS)
        {
            action_sendRawWithEOTs(message, strlen(message), ASCII_sCTRLZ, MQTT_PUBLISH_TIMEOUT, mqttPublishCompleteParser);
            atResult = action_awaitResult(true);
        }
    }
    else 
        return RESULT_CODE_BADREQUEST;

    return atResult.statusCode;
}


/**
 *  \brief Performs URL escape removal for special char (%20-%2F) without malloc.
 * 
 *  \param [in] src - Input text string to URL decode.
 *  \param [in] len - Length of input text string.
*/
static void urlDecode(char *src, int len)
{
    char subTable[] = " !\"#$%&'()*+,-./";
    uint8_t srcChar;
    uint8_t subKey;
    uint16_t dest = 0;

    for (size_t i = 0; i < len; i++)
    {
        if (src[i] == ASCII_cNULL)
            break;
        if (src[i] == '%')
        {
            srcChar = src[i + 2];
            subKey = (srcChar >= 0x30 && srcChar <= 0x39) ? srcChar - 0x30 : \
                     ((srcChar >= 0x41 && srcChar <= 0x46) ? srcChar - 0x37 : \
                     ((srcChar >= 0x61 && srcChar <= 0x66) ? srcChar - 0x57: 255));
            if (subKey != 255)
            {
                src[dest] = subTable[subKey];
                i += 2;
            }
        }
        else
            src[dest] = src[i];
        
        dest++;
    }
    src[dest + 1] = '\0';
}


/**
 *  \brief Performs background tasks to advance MQTT dataflows.
*/
void mqtt_doWork()
{
    // rdyMap not empty and rdyMapTail points to iopDataBuffer
    if (g_ltem1->mqtt->dataBufferIndx != IOP_NO_BUFFER)
    {
        uint8_t iopBufIndx = g_ltem1->mqtt->dataBufferIndx;

        // parse received MQTT message into topic and message
        // Example: +QMTRECV: 0,0, "topic/example", "This is the payload related to topic"
        // char topic[MQTT_TOPIC_NAME_SZ];
        // char message[MQTT_MESSAGE_SZ];
        char *topic;
        char *message;
        uint16_t start, end;
        char *eot;

        urlDecode(g_ltem1->iop->rxDataBufs[iopBufIndx]->buffer, IOP_RX_DATABUF_SZ);

        topic = memchr(g_ltem1->iop->rxDataBufs[iopBufIndx]->buffer, ASCII_cDBLQUOTE, MQTT_TOPIC_OFFSET_MAX);
        if (topic == NULL)                                                      // malformed
            goto discardBuffer;
        topic++;

        eot = memchr(topic, ASCII_cDBLQUOTE, g_ltem1->iop->rxDataBufs[iopBufIndx]->head - eot);
        if (eot == NULL)                                                        // malformed, overflowed somehow
            goto discardBuffer;
        *eot  = ASCII_cNULL;                                                           // null term the topic

        message = eot + 3;                                                      // set the message start
        eot = memchr(message, ASCII_cCR, g_ltem1->iop->rxDataBufs[iopBufIndx]->head - eot);
        if (eot == NULL)                                                        // malformed, overflowed somehow
            goto discardBuffer;
        *(eot-1)  = ASCII_cNULL;                                                // null term the message (remove BGx trailing "\r\n)

        // find topic in subscriptions array & invoke application receiver
        for (size_t i = 0; i < MQTT_TOPIC_MAXCNT; i++)
        {
            uint16_t topicSz = strlen(g_ltem1->mqtt->subscriptions[i].topicName);
            if (strncmp(g_ltem1->mqtt->subscriptions[i].topicName, topic, topicSz) == 0)
            {
                //                                           (topic name,                                props,           message body)
                g_ltem1->mqtt->subscriptions[i].receiver_func(g_ltem1->mqtt->subscriptions[i].topicName, topic + topicSz, message);
                break;
            }
        }

        discardBuffer:
        g_ltem1->mqtt->dataBufferIndx = IOP_NO_BUFFER;
        iop_resetDataBuffer(iopBufIndx);           // delivered, clear IOP data buffer
    }
}


#pragma endregion

/* private mqtt functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions

/**
 *	\brief [private] MQTT open status response parser.
 *
 *  \param response [in] Character data recv'd from BGx to parse for task complete
 *  \param endptr [out] Char pointer to the char following parsed text
 * 
 *  \return HTTP style result code, 0 = not complete
 */
static resultCode_t mqttOpenStatusParser(const char *response, char **endptr) 
{
    return action_serviceResponseParser(response, "+QMTOPEN: ", 0, endptr);
}


/**
 *	\brief [private] MQTT open connection response parser.
 *
 *  \param response [in] Character data recv'd from BGx to parse for task complete
 *  \param endptr [out] Char pointer to the char following parsed text
 * 
 *  \return HTTP style result code, 0 = not complete
 */
static resultCode_t mqttOpenCompleteParser(const char *response, char **endptr) 
{
    return action_serviceResponseParser(response, "+QMTOPEN: ", 1, endptr);
}


/**
 *	\brief [private] MQTT connect to server response parser.
 *
 *  \param response [in] Character data recv'd from BGx to parse for task complete
 *  \param endptr [out] Char pointer to the char following parsed text
 * 
 *  \return HTTP style result code, 0 = not complete
 */
static resultCode_t mqttConnectStatusParser(const char *response, char **endptr) 
{
    return action_serviceResponseParser(response, "+QMTCONN: ", 1, endptr);
}


/**
 *	\brief [private] MQTT connect to server response parser.
 *
 *  \param response [in] Character data recv'd from BGx to parse for task complete
 *  \param endptr [out] Char pointer to the char following parsed text
 * 
 *  \return HTTP style result code, 0 = not complete
 */
static resultCode_t mqttConnectCompleteParser(const char *response, char **endptr) 
{
    return action_serviceResponseParser(response, "+QMTCONN: ", 2, endptr);
}


/**
 *	\brief [private] MQTT subscribe to topic response parser.
 *
 *  \param response [in] Character data recv'd from BGx to parse for task complete
 *  \param endptr [out] Char pointer to the char following parsed text
 * 
 *  \return HTTP style result code, 0 = not complete
 */
static resultCode_t mqttSubscribeCompleteParser(const char *response, char **endptr) 
{
    return action_serviceResponseParser(response, "+QMTSUB: ", 2, endptr);
}


/**
 *	\brief [private] MQTT publish message to topic response parser.
 *
 *  \param response [in] Character data recv'd from BGx to parse for task complete
 *  \param endptr [out] Char pointer to the char following parsed text
 * 
 *  \return HTTP style result code, 0 = not complete
 */
static resultCode_t mqttPublishCompleteParser(const char *response, char **endptr) 
{
    return action_serviceResponseParser(response, "+QMTPUB: ", 2, endptr);
}


#pragma endregion
