/******************************************************************************
 *  \file mqtt.c
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
 *
 *****************************************************************************/

#include "ltem1c.h"

//#define _DEBUG
#include "dbgprint.h"

#define WAIT_SECONDS(timeout) (timeout * 1000)


/* private function declarations */
static actionResult_t mqttOpenCompleteParser(const char *response);
static actionResult_t mqttConnectCompleteParser(const char *response);
static actionResult_t mqttSubscribeCompleteParser(const char *response);
static actionResult_t mqttPublishCompleteParser(const char *response);



/* public mqtt functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions


/**
 *	\brief Initialize the MQTT service structure.
 */
void mqtt_create()
{
    g_ltem1->mqtt = calloc(LTEM1_SOCKET_COUNT, sizeof(mqtt_t));
	if (g_ltem1->mqtt == NULL)
	{
        ltem1_faultHandler(0, "mqtt-could not alloc mqtt service struct");
	}

    for (size_t i = 0; i < LTEM1_SOCKET_COUNT; i++)
    {   
        g_ltem1->mqtt->msgId[i] = 1;
    }
    for (size_t i = 0; i < MQTT_TOPIC_MAXCNT; i++)
    {
        g_ltem1->mqtt->subscriptions[i].topicName[0] = NULL;
        g_ltem1->mqtt->subscriptions[i].recv_func = NULL;
    }
    
}


/**
 *	\brief Tear down MQTT service.
 */
void mqtt_destroy(void *mqtt)
{
	free(mqtt);
}



/**
 *  \brief Open a remote MQTT server for use.
 * 
 *  \param [in] socketId The socket based identifier to use for the server connection.
 *  \param [in] host The host IP address or name of the remote server.
 *  \param [in] port The IP port number to use for the communications.
 *  \param [in] useSslVersion Specifies the version and options for use of SSL to protect communications.
 *  \param [in] useMqttVersion Specifies the MQTT protocol revision to use for communications.
 * 
 *  \returns A socketResult_t value indicating the success or type of failure.
*/
socketResult_t mqtt_open(socketId_t socketId, const char *host, uint16_t port, sslVersion_t useSslVersion, mqttVersion_t useMqttVersion)
{
    // AT+QSSLCFG="sslversion",0,3
    // AT+QMTCFG="ssl",0,1,0

    // AT+QMTCFG="version",0,4
    // AT+QMTOPEN=0,"iothub-dev-pelogical.azure-devices.net",8883

    #define ACTION_CMD_SZ 81
    #define ACTION_RSP_SZ 81

    char actionCmd[ACTION_CMD_SZ] = {0};
    char actionResponse[ACTION_RSP_SZ] = {0};
    actionResult_t result;

    if (socketId >= IOP_SOCKET_COUNT)
        return ACTION_RESULT_BADREQUEST;
    if (g_ltem1->protocols->sockets[socketId].protocol != protocol_void)
        return ACTION_RESULT_BADREQUEST;

    if (useSslVersion != sslVersion_none)
    {
        snprintf(actionCmd, ACTION_CMD_SZ, "AT+QSSLCFG=\"sslversion\",%d,%d", socketId, (uint8_t)useSslVersion);
        if ( !action_tryInvoke(actionCmd, true) || action_awaitResult(actionResponse, ACTION_RSP_SZ, 0, NULL) != ACTION_RESULT_SUCCESS)
            result = 600;

        snprintf(actionCmd, ACTION_CMD_SZ, "AT+QMTCFG=\"ssl\",%d,1,%d", socketId, socketId);
        if ( !action_tryInvoke(actionCmd, true) || action_awaitResult(actionResponse, ACTION_RSP_SZ, 0, NULL) != ACTION_RESULT_SUCCESS)
            return ACTION_RESULT_ERROR;
    }

    if (useMqttVersion == mqttVersion_311)
    {
        snprintf(actionCmd, ACTION_CMD_SZ, "AT+QMTCFG=\"version\",%d,4", socketId);
        if ( !action_tryInvoke(actionCmd, true) || action_awaitResult(actionResponse, ACTION_RSP_SZ, 0, NULL) != ACTION_RESULT_SUCCESS)
            return ACTION_RESULT_ERROR;
    }

    // AT+QMTOPEN=0,"iothub-dev-pelogical.azure-devices.net",8883
    snprintf(actionCmd, ACTION_CMD_SZ, "AT+QMTOPEN=%d,\"%s\",%d", socketId, host, port);
    //if ( !action_tryInvoke(actionCmd, true) || (result = action_awaitResult(actionResponse, ACTION_RSP_SZ, WAIT_SECONDS(45), mqttOpenCompleteParser)) != ACTION_RESULT_SUCCESS)
    //    return result;

    result = !action_tryInvoke(actionCmd, true);
    result = action_awaitResult(actionResponse, ACTION_RSP_SZ, WAIT_SECONDS(45), mqttOpenCompleteParser);

    g_ltem1->protocols->sockets[socketId].protocol = (useSslVersion == sslVersion_none) ? protocol_mqtt : protocol_mqtts;
    return ACTION_RESULT_SUCCESS;
}


/*
"iothub-dev-pelogical.azure-devices.net"
"e8fdd7df-2ca2-4b64-95de-031c6b199299"
"iothub-dev-pelogical.azure-devices.net/e8fdd7df-2ca2-4b64-95de-031c6b199299/?api-version=2018-06-30"
*/


/**
 *  \brief Disconnect and close a connection to a MQTT server
 * 
 *  \param [in] socketId The socket based identifier for the connection to close
*/
void mqtt_close(socketId_t socketId)
{
    char actionCmd[81] = {0};
    char actionResponse[81] = {0};

    snprintf(actionCmd, 80, "AT+QMTCLOSE=%d", socketId);
    if (action_tryInvoke(actionCmd, true) || action_awaitResult(actionResponse, 80, 0, NULL) != ACTION_RESULT_SUCCESS)
        return;

    snprintf(actionCmd, 80, "AT+QMTDISC=%d", socketId);
    if ( !action_tryInvoke(actionCmd, true) || action_awaitResult(actionResponse, 80, 0, NULL) != ACTION_RESULT_SUCCESS)
        return;
}


/**
 *  \brief Connect (authenticate) to a MQTT server.
 * 
 *  \param [in] socketId The socket based identifier for the connection to close.
 *  \param [in] clientId The client or device identifier for the connection.
 *  \param [in] username The user identifier or name for the connection to authenticate.
 *  \param [in] password The secret string or phrase to authenticate the connection.
 * 
 *  \returns A socketResult_t value indicating the success or type of failure.
*/
socketResult_t mqtt_connect(socketId_t socketId, const char *clientId, const char *username, const char *password)
{
    #define MQTT_CONNECT_CMDSZ 540
    #define MQTT_CONNECT_RSPSZ 81

    char actionCmd[MQTT_CONNECT_CMDSZ] = {0};
    char actionResponse[MQTT_CONNECT_RSPSZ] = {0};
    socketResult_t result;

    //char actionCmd[] = "at+qmtconn=1,\"e8fdd7df-2ca2-4b64-95de-031c6b199299\",\"iothub-dev-pelogical.azure-devices.net/e8fdd7df-2ca2-4b64-95de-031c6b199299/?api-version=2018-06-30\",\"SharedAccessSignature sr=iothub-dev-pelogical.azure-devices.net%2Fdevices%2Fe8fdd7df-2ca2-4b64-95de-031c6b199299&sig=6hTmu6e11E9CCKo1Ppeg8qxTfSRIfFwaau0crXeF9kQ%3D&se=2058955139\"";

    snprintf(actionCmd, MQTT_CONNECT_CMDSZ, "AT+QMTCONN=%d,\"%s\",\"%s\",\"SharedAccessSignature %s\"", socketId, clientId, username, password);
    if (action_tryInvoke(actionCmd, true))
    {
        result = action_awaitResult(actionResponse, MQTT_CONNECT_RSPSZ, WAIT_SECONDS(30), mqttConnectCompleteParser);
        return result;
    }
    return ACTION_RESULT_BADREQUEST;
}



/**
 *  \brief Subscribe to a topic on the MQTT server.
 * 
 *  \param [in] socketId  The socket based identifier for the MQTT server connection to update.
 *  \param [in] topic  The messaging topic to subscribe to.
 *  \param [in] qos  The QOS level for messages subscribed to.
 *  \param [in] recv_func The receiver function in the application to receive subscribed messages on arrival.
 * 
 *  \returns A socketResult_t value indicating the success or type of failure.
*/
socketResult_t mqtt_subscribe(socketId_t socketId, const char *topic, mqttQos_t qos, mqttRecv_func_t recv_func)
{
    // AT+QMTSUB=1,99,"devices/e8fdd7df-2ca2-4b64-95de-031c6b199299/messages/devicebound/#",0
    #define MQTT_PUBSUB_CMDSZ 140
    #define MQTT_PUBSUB_RSPSZ 81


    char actionCmd[MQTT_PUBSUB_CMDSZ] = {0};
    char actionResponse[MQTT_PUBSUB_RSPSZ] = {0};

    if (recv_func == NULL)
        return ACTION_RESULT_BADREQUEST;

    for (size_t i = 0; i < MQTT_TOPIC_MAXCNT; i++)
    {
        if (g_ltem1->mqtt->subscriptions[i].topicName[0] == ASCII_cNULL)
        {
            strncpy(g_ltem1->mqtt->subscriptions[i].topicName, topic, MQTT_TOPICNAME_MAXSZ - 1);
            g_ltem1->mqtt->subscriptions[i].recv_func = recv_func;
            recv_func = NULL;
            break;
        }
    }
    if (recv_func != NULL)                          // recv_func ptr moved to g_ltem1 global if success
        return ACTION_RESULT_CONFLICT;

    snprintf(actionCmd, MQTT_CONNECT_CMDSZ, "AT+QMTSUB=%d,%d,\"%s\",%d", socketId, ++g_ltem1->mqtt->msgId[socketId], topic, qos);
    if (action_tryInvoke(actionCmd, true))
    {
        return action_awaitResult(actionResponse, 80, WAIT_SECONDS(10), mqttSubscribeCompleteParser);
    }
    return ACTION_RESULT_BADREQUEST;
}



/**
 *  \brief Unsubscribe to a topic on the MQTT server.
 * 
 *  \param [in] socketId  The socket based identifier for the MQTT server connection to update.
 *  \param [in] topic  The messaging topic to unsubscribe from.
 * 
 *  \returns A socketResult_t value indicating the success or type of failure.
*/
socketResult_t mqtt_unsubscribe(socketId_t socketId, const char *topic)
{
    // AT+QMTUNS=<tcpconnectID>,<msgID>,"<topic1>"

    char actionCmd[MQTT_PUBSUB_CMDSZ] = {0};
    char actionResponse[81] = {0};

    for (size_t i = 0; i < MQTT_TOPIC_MAXCNT; i++)
    {
        if (strcmp(g_ltem1->mqtt->subscriptions[i].topicName, topic) == 0)
        {
            g_ltem1->mqtt->subscriptions[i].topicName[0] = ASCII_cNULL;
            g_ltem1->mqtt->subscriptions[i].recv_func = NULL;
            break;
        }
    }
    
    snprintf(actionCmd, MQTT_CONNECT_CMDSZ, "AT+QMTUNS=%d,%d,\"<topic1>\"", socketId, ++g_ltem1->mqtt->msgId[socketId], topic);
    if (action_tryInvoke(actionCmd, true))
    {
        return action_awaitResult(actionResponse, 80, 0, NULL);
    }
    return ACTION_RESULT_BADREQUEST;
}



socketResult_t mqtt_publish(socketId_t socketId, const char *topic, mqttQos_t qos, const char *message)
{
    // AT+QMTPUB=<tcpconnectID>,<msgID>,<qos>,<retain>,"<topic>"

    char publishCmd[MQTT_PUBTOPIC_MAXSZ + MQTT_URC_OVRHDSZ] = {0};
    char msgText[MQTT_MESSAGE_MAXSZ];
    char actionResponse[81] = {0};
    socketResult_t result;

    // register the pending publish action
    action_setAutoClose(false);
    uint16_t msgId = ((uint8_t)qos == 0) ? 0 : ++g_ltem1->mqtt->msgId[socketId];
    snprintf(publishCmd, 160, "AT+QMTPUB=%d,%d,%d,0,\"%s\"", socketId, msgId, qos, topic);
    result = action_tryInvoke(publishCmd, true) ? ACTION_RESULT_SUCCESS : ACTION_RESULT_BADREQUEST;
    if (result >= ACTION_RESULT_ERRORS_BASE)
        return result;
    
    result = action_awaitResult(actionResponse, 80, 0, iop_txDataPromptParser);
    action_setAutoClose(true);

    // now send the data
    // after prompt for data, now complete sub-command to actually transfer data
    if (result == ACTION_RESULT_SUCCESS)
    {
        action_sendData(message, 0);
        do
        {
            result = action_getResult(actionResponse, ACTION_DEFAULT_RESPONSE_SZ, 2000, mqttPublishCompleteParser);
        } while (result == ACTION_RESULT_PENDING);
    }
    return result;
}


#pragma endregion

/* private mqtt functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions

/**
 *	\brief [private] MQTT open connection response parser.
 */
static actionResult_t mqttOpenCompleteParser(const char *response) 
{
    return action_serviceResponseParser(response, "+QMTOPEN: ", 1);
}


/**
 *	\brief [private] MQTT connect to server response parser.
 */
static actionResult_t mqttConnectCompleteParser(const char *response) 
{
    return action_serviceResponseParser(response, "+QMTCONN: ", 2);
}


/**
 *	\brief [private] MQTT subscribe to topic response parser.
 */
static actionResult_t mqttSubscribeCompleteParser(const char *response) 
{
    return action_serviceResponseParser(response, "+QMTSUB: ", 3);
}


/**
 *	\brief [private] MQTT publish message to topic response parser.
 */
static actionResult_t mqttPublishCompleteParser(const char *response) 
{
    return action_serviceResponseParser(response, "+QMTPUB: ", 2);
}


#pragma endregion
