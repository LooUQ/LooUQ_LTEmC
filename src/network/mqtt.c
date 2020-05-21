/******************************************************************************
 *  \file mqtt.c
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
 *
 *****************************************************************************/

#include "..\ltem1c.h"

/* private function declarations */
// static actionResult_t ipOpenCompleteParser(const char *response);
// static actionResult_t sslOpenCompleteParser(const char *response);
// static actionResult_t openCompleteParser(const char *response, const char *landmark);

// static actionResult_t sendDataPromptParser(const char *response);
// static actionResult_t recvIrdPromptParser(const char *response);


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
        g_ltem1->mqtt->msgId[i] = 0;
    }
}


/**
 *	\brief Tear down MQTT service.
 */
void mqtt_destroy(void *mqtt)
{
	free(mqtt);
}



socketId_t mqtt_open(const char *host, uint16_t port, sslVersion_t useSslVersion, mqttVersion_t useMqttVersion)
{
    // AT+QSSLCFG="sslversion",0,3
    // AT+QMTCFG="ssl",0,1,0

    // AT+QMTCFG="version",0,4
    // AT+QMTOPEN=0,"iothub-dev-pelogical.azure-devices.net",8883

    char actionCmd[81] = {0};
    char actionResponse[81] = {0};
    actionResult_t result;

    socketId_t socketId = SOCKET_CLOSED;

    for (size_t i = 0; i < LTEM1_SOCKET_COUNT; i++)
    {
        if (g_ltem1->protocols->sockets[i].protocol == protocol_none)
        {
            socketId = i;
            break;
        }
    }

    if (socketId == SOCKET_CLOSED)
        return SOCKET_CLOSED;

    if (useSslVersion != sslVersion_none)
    {
        snprintf(actionCmd, 80, "AT+QSSLCFG=\"sslversion\",%d,%d", socketId, (uint8_t)useSslVersion);
        result = action_awaitResult(actionResponse, 80, 0, NULL, true);
        if (result != ACTION_RESULT_SUCCESS)
            return SOCKET_CLOSED;

        snprintf(actionCmd, 80, "AT+QMTCFG=\"ssl\",%d,1,%d", socketId, socketId);
        result = action_awaitResult(actionResponse, 80, 0, NULL, true);
        if (result != ACTION_RESULT_SUCCESS)
            return SOCKET_CLOSED;
    }

    if (useMqttVersion == mqttVersion_311)
    {
        snprintf(actionCmd, 80, "AT+QMTCFG=\"version\",%d,4", socketId);
        result = action_awaitResult(actionResponse, 80, 0, NULL, true);
        if (result != ACTION_RESULT_SUCCESS)
            return SOCKET_CLOSED;
    }

    // AT+QMTOPEN=0,"iothub-dev-pelogical.azure-devices.net",8883
    snprintf(actionCmd, 80, "AT+QMTOPEN=%d,\"%s\",%d", socketId, host, port);
    result = action_awaitResult(actionResponse, 80, 0, NULL, true);
    if (result != ACTION_RESULT_SUCCESS)
        return SOCKET_CLOSED;

    g_ltem1->protocols->sockets[socketId].protocol = (useSslVersion == sslVersion_none) ? protocol_mqtt : protocol_mqtts;
    return socketId;
}



void mqtt_close(socketId_t socketId)
{
    char actionCmd[81] = {0};
    char actionResponse[81] = {0};

    snprintf(actionCmd, 80, "AT+QMTDISC=%d", socketId);
    if (action_awaitResult(actionResponse, 80, 0, NULL, true) != ACTION_RESULT_SUCCESS)
        return;

    snprintf(actionCmd, 80, "AT+QMTCLOSE=%d", socketId);
    (void)action_awaitResult(actionResponse, 80, 0, NULL, true);
}



socketResult_t mqtt_connect(socketId_t socketId, const char *clientId, const char *username, const char *password)
{
    // AT+QMTCONN=0,"e8fdd7df-2ca2-4b64-95de-031c6b199299","iothub-dev-pelogical.azure-devices.net/e8fdd7df-2ca2-4b64-95de-031c6b199299/?api-version=2018-06-30","SharedAccessSignature sr=iothub-dev-pelogical.azure-devices.net%2Fdevices%2Fe8fdd7df-2ca2-4b64-95de-031c6b199299&sig=%2FEpVF6bbYubWqoDaWyfoH0S1ZADNGxqdbhB24vBS9dA%3D&se=1590173851"
    char actionCmd[400] = {0};
    char actionResponse[81] = {0};
    socketResult_t result;

    snprintf(actionCmd, 400, "AT+QMTCONN=%d,\"%s\",\"%s\",\"%s\"", socketId, clientId, username, password);
    result = action_awaitResult(actionResponse, 80, 0, NULL, true);
    return result;
}



socketResult_t mqtt_subscribe(socketId_t socketId, const char *topic, uint8_t qos)
{
    // AT+QMTSUB=<tcpconnectID>,<msgID>,"<topic1>",<qos1>
}

socketResult_t mqtt_unsubscribe(socketId_t socketId)
{
    // AT+QMTUNS=<tcpconnectID>,<msgID>,"<topic1>"
}



socketResult_t mqtt_publish(socketId_t socketId, const char *topic, const char *message)
{
    // AT+QMTPUB=<tcpconnectID>,<msgID>,<qos>,<retain>,"<topic>"

    char actionCmd[160] = {0};
    char msgText[MQTT_PUBMSG_MAXSZ];
    char actionResponse[81] = {0};
    socketResult_t result;

    // register the pending publish action
    snprintf(actionCmd, 160, "AT+QMTPUB=%d,0,0,0,\"%s\"", socketId, topic);
    result = action_awaitResult(actionResponse, 80, 0, sendDataPromptParser, false);

    // now send the data
    // after prompt for data, now complete sub-command to actually transfer data
    if (result == ACTION_RESULT_SUCCESS)
    {
        action_sendData(message, 0);
        do
        {
            result = action_getResult(actionResponse, ACTION_DEFAULT_RESPONSE_SZ, 0, NULL, true);
        } while (result == ACTION_RESULT_PENDING);
    }
    return result;
}


#pragma endregion

/* private mqtt functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions



#pragma endregion
