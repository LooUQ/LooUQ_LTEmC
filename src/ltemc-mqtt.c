/******************************************************************************
 *  \file ltemc-mqtt.c
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020,2021 LooUQ Incorporated.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED
 * "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
 * LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 ******************************************************************************
 * MQTT protocol support 
 *****************************************************************************/

#define _DEBUG 0                        // set to non-zero value for PRINTF debugging output, 
// debugging output options             // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if defined(_DEBUG) && _DEBUG > 0
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG == 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#endif


#include "ltemc.h"
//#include "mqtt.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define MQTT_ACTION_CMD_SZ 81
#define MQTT_CONNECT_CMD_SZ 300

#pragma region Static Local Function Declarations
static resultCode_t s_mqttOpenStatusParser(const char *response, char **endptr);
static resultCode_t s_mqttOpenCompleteParser(const char *response, char **endptr);
static resultCode_t s_mqttConnectStatusParser(const char *response, char **endptr);
static resultCode_t s_mqttConnectCompleteParser(const char *response, char **endptr);
static resultCode_t s_mqttSubscribeCompleteParser(const char *response, char **endptr);
static resultCode_t s_mqttPublishCompleteParser(const char *response, char **endptr);
static void s_urlDecode(char *src, int len);
#pragma endregion

// IOP peer
static iop_t *iopPtr;
// this 
static mqttPtr_t mqttPtr;


/* public mqtt functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions

/**
 *	\brief Initialize MQTT service structure and add it to LTEm1c services.
 */
void mqtt_create()
{
    mqttPtr = calloc(1, sizeof(mqtt_t));
	if (mqttPtr == NULL)
	{
        ltem_notifyApp(ltemNotifType_memoryAllocFault, "could not alloc mqtt service struct");
	}
    mqttPtr->msgId = 1;
    mqttPtr->dataBufferIndx = IOP_NO_BUFFER;

    // set global reference
    g_ltem->mqtt = mqttPtr;
    g_ltem->mqttWork_func = &mqtt_doWork;
    // set reference to IOP peer
    iopPtr = g_ltem->iop;
    iop_registerProtocol(ltemOptnModule_mqtt, mqttPtr);
}



/**
 *  \brief Query the status of the MQTT server state.
 * 
 *  \param host [in] A char string to match with the currently connected server. Host is not checked if empty string passed.
 *  \param force [in] If true query BGx for current state, otherwise return internal property value
 * 
 *  \returns A mqttStatus_t value indicating the state of the MQTT connection.
*/
mqttStatus_t mqtt_status(const char *host, bool force)
{
    // See BG96_MQTT_Application_Note: AT+QMTOPEN? and AT+QMTCONN?

    if (!force && *host == 0)                          // if not forcing query and no host verification, return current internal prop value
        return mqttPtr->state;

    mqttPtr->state = mqttStatus_closed;

    // connect check first to short-circuit efforts
    if (atcmd_tryInvokeAdv("AT+QMTCONN?", PERIOD_FROM_SECONDS(5), s_mqttConnectStatusParser))
    {
        atcmdResult_t atResult = atcmd_awaitResult(true);
        if (atResult.statusCode == RESULT_CODE_SUCCESS)
            mqttPtr->state = mqttStatus_connected;
    }

    if (mqttPtr->state != mqttStatus_connected ||           // if not connected
        *host != 0 &&                                       // or need host verification: check open
        atcmd_tryInvokeAdv("AT+QMTOPEN?", PERIOD_FROM_SECONDS(5), s_mqttOpenStatusParser))
    {
        atcmdResult_t atResult = atcmd_awaitResult(true);
        if (atResult.statusCode == RESULT_CODE_SUCCESS)
        {
            mqttPtr->state = mqttStatus_open;
            if (*host != 0)                                // host matching requested, check modem response host == requested host
            {
                char *hostNameAt = strchr(atResult.response, ASCII_cDBLQUOTE);
                mqttPtr->state = (hostNameAt != NULL && strncmp(hostNameAt + 1, host, strlen(host)) == 0) ? mqttStatus_open : mqttStatus_closed;
            }
        }
    }
    return mqttPtr->state;
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

    char actionCmd[MQTT_ACTION_CMD_SZ] = {0};
    atcmdResult_t atResult;

    mqttPtr->state = mqtt_status(host, true);     // refresh state, state must be not open for config changes
    if (mqttPtr->state >= mqttStatus_open)        // already open+connected with server "host"
        return RESULT_CODE_SUCCESS;

    if (useSslVersion != sslVersion_none)
    {
        snprintf(actionCmd, MQTT_ACTION_CMD_SZ, "AT+QSSLCFG=\"sslversion\",%d,%d", MQTT_SOCKET_ID, (uint8_t)useSslVersion);
        if (atcmd_tryInvoke(actionCmd))
        {
            if (atcmd_awaitResult(true).statusCode != RESULT_CODE_SUCCESS)
                return RESULT_CODE_ERROR;
        }

        snprintf(actionCmd, MQTT_ACTION_CMD_SZ, "AT+QMTCFG=\"ssl\",%d,1,%d", MQTT_SOCKET_ID, MQTT_SOCKET_ID);
        if (atcmd_tryInvoke(actionCmd))
        {
            if (atcmd_awaitResult(true).statusCode != RESULT_CODE_SUCCESS)
                return RESULT_CODE_ERROR;
        }
    }

    if (useMqttVersion == mqttVersion_311)
    {
        snprintf(actionCmd, MQTT_ACTION_CMD_SZ, "AT+QMTCFG=\"version\",%d,4", MQTT_SOCKET_ID);
        if (atcmd_tryInvoke(actionCmd))
        {
            if (atcmd_awaitResult(true).statusCode != RESULT_CODE_SUCCESS)
                return RESULT_CODE_ERROR;
        }
    }

    // TYPICAL: AT+QMTOPEN=0,"iothub-dev-pelogical.azure-devices.net",8883
    snprintf(actionCmd, MQTT_ACTION_CMD_SZ, "AT+QMTOPEN=%d,\"%s\",%d", MQTT_SOCKET_ID, host, port);
    if (atcmd_tryInvokeAdv(actionCmd, PERIOD_FROM_SECONDS(45), s_mqttOpenCompleteParser))
    {
        atcmdResult_t atResult = atcmd_awaitResult(true);

        // if (atResult.statusCode == RESULT_CODE_SUCCESS)
        //     iopPtr->peerTypeMap.mqttConnection = 1;
        // else
        //     return RESULT_CODE_ERROR;

        switch (atResult.statusCode)
        {
            case RESULT_CODE_SUCCESS:
                iopPtr->peerTypeMap.mqttConnection = 1;
                mqttPtr->state = mqttStatus_open;
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
}


/**
 *  \brief Disconnect and close a connection to a MQTT server
 * 
 *  \param [in] socketId - The socket based identifier for the connection to close
*/
void mqtt_close()
{
    char actionCmd[MQTT_ACTION_CMD_SZ] = {0};

    mqttPtr->state = mqttStatus_closed;                 

    iopPtr->peerTypeMap.mqttConnection = 0;           // release mqtt socket in IOP
    for (size_t i = 0; i < IOP_RX_DATABUFFERS_MAX; i++)     // release any iop buffers assigned
    {
        if (iopPtr->rxDataBufs[i]->dataPeer == iopDataPeer_MQTT)
            iopPtr->rxDataBufs[i]->dataPeer = iopDataPeer__NONE;
    }
    for (size_t i = 0; i < MQTT_TOPIC_MAXCNT; i++)          // clear subscriptions table
    {
        mqttPtr->subscriptions[i].topicName[0] = 0;
        mqttPtr->subscriptions[i].receiver_func = NULL;
    }

    // if (mqttPtr->state == mqttStatus_connected)
    // {
    //     snprintf(actionCmd, 80, "AT+QMTDISC=%d", MQTT_SOCKET_ID);
    //     if (atcmd_tryInvoke(actionCmd))
    //         atcmd_awaitResult(true);
    // }
    if (mqttPtr->state >= mqttStatus_open)
    {
        snprintf(actionCmd, 80, "AT+QMTCLOSE=%d", MQTT_SOCKET_ID);
        if (atcmd_tryInvoke(actionCmd))
            atcmd_awaitResult(true);
    }

    mqttPtr->state == mqttStatus_closed;
    return RESULT_CODE_SUCCESS;
}



/**
 *  \brief Connect (authenticate) to a MQTT server.
 * 
 *  \param clientId [in] - The client or device identifier for the connection.
 *  \param username [in] - The user identifier or name for the connection to authenticate.
 *  \param password [in] - The secret string or phrase to authenticate the connection.
 *  \param cleanSession [in] - Directs MQTT to preserve or flush messages received prior to the session start.
 * 
 *  \returns A resultCode_t value indicating the success or type of failure, OK = 200.
*/
resultCode_t mqtt_connect(const char *clientId, const char *username, const char *password, mqttSession_t cleanSession)
{

    char actionCmd[MQTT_CONNECT_CMD_SZ] = {0};
    atcmdResult_t atResult;

    if (mqttPtr->state == mqttStatus_connected)       // already connected, trusting internal state as this is likely immediately after open
        return RESULT_CODE_SUCCESS;                         // mqtt_open forces mqtt state sync with BGx

    snprintf(actionCmd, MQTT_CONNECT_CMD_SZ, "AT+QMTCFG=\"session\",%d,%d", MQTT_SOCKET_ID, (uint8_t)cleanSession);
    if (atcmd_tryInvoke(actionCmd))
    {
        if (atcmd_awaitResult(true).statusCode != RESULT_CODE_SUCCESS)
            return RESULT_CODE_ERROR;
    }

    snprintf(actionCmd, MQTT_CONNECT_CMD_SZ, "AT+QMTCONN=%d,\"%s\",\"%s\",\"%s\"", MQTT_SOCKET_ID, clientId, username, password);
    if (atcmd_tryInvokeAdv(actionCmd, PERIOD_FROM_SECONDS(60), s_mqttConnectCompleteParser))
    {
        atResult = atcmd_awaitResult(true);

        // if (atResult.statusCode == RESULT_CODE_SUCCESS)
        //     iopPtr->peerTypeMap.mqttConnection = 2;
        // return atResult.statusCode;

        switch (atResult.statusCode)
        {
            case RESULT_CODE_SUCCESS:
                iopPtr->peerTypeMap.mqttConnection = 2;
                mqttPtr->state = mqttStatus_connected;
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
 *  \param topic [in] - The messaging topic to subscribe to.
 *  \param qos [in] - The MQTT QOS level for messages subscribed to.
 *  \param recv_func [in] - The receiver function in the application to receive subscribed messages on arrival.
 * 
 *  \returns A resultCode_t value indicating the success or type of failure, OK = 200.
 */
resultCode_t mqtt_subscribe(const char *topic, mqttQos_t qos, mqtt_recvFunc_t recv_func)
{
    char actionCmd[MQTT_ACTION_CMD_SZ] = {0};
    uint8_t subSlot = 0xFF;

    uint16_t topicSz = strlen(topic);
    if (recv_func == NULL || topicSz >= MQTT_TOPIC_NAME_SZ)
        return RESULT_CODE_BADREQUEST;

    bool wildcard = *(topic + topicSz - 1) == '#';      // test for MQTT multilevel wildcard, store separately for future topic parsing on recv
    char topicEntryName[MQTT_TOPIC_NAME_SZ] = {0};

    strncpy(topicEntryName, topic, wildcard ? topicSz - 1 : topicSz);

    bool alreadySubscribed = false;
    for (size_t i = 0; i < MQTT_TOPIC_MAXCNT; i++)
    {
        if (strcmp(mqttPtr->subscriptions[i].topicName, topicEntryName) == 0)
        {
            alreadySubscribed = true;
            subSlot = i;
            break;
        }
    }
    if (!alreadySubscribed)
    {
        for (size_t i = 0; i < MQTT_TOPIC_MAXCNT; i++)
        {
            if (mqttPtr->subscriptions[i].topicName[0] == 0)
            {
                strncpy(mqttPtr->subscriptions[i].topicName, topicEntryName, strlen(topicEntryName)+1);
                if (wildcard)
                    mqttPtr->subscriptions[i].wildcard = '#';
                mqttPtr->subscriptions[i].receiver_func = recv_func;
                subSlot = i;
                break;
            }
        }
    }
    if (subSlot == 0xFF)
        return RESULT_CODE_CONFLICT;

    // regardless of new or existing subscription table entry, complete network subscribe
    // BGx implementation of MQTT doesn't provide subscription query, but is tolerant of duplicate subscription 
    // if sucessful, the topic's subscription will overwrite the IOP peer map without issue as well (same bitmap value)

    snprintf(actionCmd, MQTT_ACTION_CMD_SZ, "AT+QMTSUB=%d,%d,\"%s\",%d", MQTT_SOCKET_ID, ++mqttPtr->msgId, topic, qos);
    if (atcmd_tryInvokeAdv(actionCmd, PERIOD_FROM_SECONDS(15), s_mqttSubscribeCompleteParser))
    {
        atcmdResult_t atResult = atcmd_awaitResult(true);
        if (atResult.statusCode == RESULT_CODE_SUCCESS)
        {
            iopPtr->peerTypeMap.mqttSubscribe = iopPtr->peerTypeMap.mqttSubscribe | (1 << subSlot);
            return atResult.statusCode;
        }
        else
            mqttPtr->subscriptions[subSlot].topicName[0] = 0;        // if error on BGx subscribe, give table entry back
    }
    return RESULT_CODE_BADREQUEST;
}



/**
 *  \brief Unsubscribe to a topic on the MQTT server.
 * 
 *  \param topic [in] - The messaging topic to unsubscribe from.
 * 
 *  \returns A resultCode_t (http status type) value indicating the success or type of failure, OK = 200.
*/
resultCode_t mqtt_unsubscribe(const char *topic)
{
    // AT+QMTUNS=<tcpconnectID>,<msgID>,"<topic1>"

    char actionCmd[MQTT_TOPIC_PUBCMD_OVRHD_SZ] = {0};
    uint8_t subscriptionSlot = 255;

    uint16_t topicSz = strlen(topic);                   // adjust topic if multilevel wildcard, remove prior to subscriptions scan
    if (*(topic + topicSz) == "#")
        topicSz--;

    for (size_t i = 0; i < MQTT_TOPIC_MAXCNT; i++)
    {
        if (strncmp(mqttPtr->subscriptions[i].topicName, topic, topicSz) == 0)
        {
            mqttPtr->subscriptions[i].topicName[0] = 0;
            mqttPtr->subscriptions[i].receiver_func = NULL;
            subscriptionSlot = i;
            break;
        }
    }
    
    snprintf(actionCmd, MQTT_CONNECT_CMD_SZ, "AT+QMTUNS=%d,%d,\"<topic1>\"", MQTT_SOCKET_ID, ++mqttPtr->msgId, topic);
    if (atcmd_tryInvoke(actionCmd))
    {
        atcmdResult_t atResult = atcmd_awaitResult(true);
        if (atResult.statusCode == RESULT_CODE_SUCCESS)
            iopPtr->peerTypeMap.mqttSubscribe = iopPtr->peerTypeMap.mqttSubscribe & (~(1 << subscriptionSlot));
        return atResult.statusCode;
    }
    return RESULT_CODE_BADREQUEST;
}


/**
 *  \brief Publish a message to server.
 * 
 *  \param topic [in] - Pointer to the message topic (see your server for topic formatting details).
 *  \param qos [in] - The MQTT QOS to be assigned to sent message.
 *  \param message [in] - Pointer to message to be sent.
 * 
 *  \returns A resultCode_t value indicating the success or type of failure (http status type code).
*/
resultCode_t mqtt_publish(const char *topic, mqttQos_t qos, const char *message)
{
    // AT+QMTPUB=<tcpconnectID>,<msgID>,<qos>,<retain>,"<topic>"
    char publishCmd[MQTT_TOPIC_PUBBUF_SZ] = {0};
    char msgText[MQTT_MESSAGE_SZ];
    atcmdResult_t atResult;

    // register the pending publish action
    uint16_t msgId = ((uint8_t)qos == 0) ? 0 : ++mqttPtr->msgId;
    snprintf(publishCmd, MQTT_TOPIC_PUBBUF_SZ, "AT+QMTPUB=%d,%d,%d,0,\"%s\"", MQTT_SOCKET_ID, msgId, qos, topic);
    
    if (atcmd_tryInvokeAdv(publishCmd, ACTION_TIMEOUTml, iop_txDataPromptParser))
    {
        atResult = atcmd_awaitResult(false);

        if (atResult.statusCode == RESULT_CODE_SUCCESS)         // wait for data prompt for data, now complete sub-command to actually transfer data
        {
            atcmd_sendRawWithEOTs(message, strlen(message), ASCII_sCTRLZ, MQTT_PUBLISH_TIMEOUT, s_mqttPublishCompleteParser);
            atResult = atcmd_awaitResult(true);
        }

        if (atResult.statusCode != RESULT_CODE_SUCCESS)         // if any problem, make sure BGx is out of text mode
            atcmd_exitTextMode();
    }
    else 
        return RESULT_CODE_BADREQUEST;

    return atResult.statusCode;
}


/**
 *  \brief Performs URL escape removal for special char (%20-%2F) without malloc.
 * 
 *  \param src [in] - Input text string to URL decode.
 *  \param len [in] - Length of input text string.
*/
static void s_urlDecode(char *src, int len)
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
 *  \brief Performs background tasks to advance MQTT pipeline dataflows.
*/
void mqtt_doWork()
{
    // rdyMap not empty and rdyMapTail points to iopDataBuffer
    if (mqttPtr &&
        mqttPtr->dataBufferIndx != IOP_NO_BUFFER)
    {
        uint8_t iopBufIndx = mqttPtr->dataBufferIndx;

        // parse received MQTT message into topic and message
        // Example: +QMTRECV: 0,0, "topic/example", "This is the payload related to topic"
        // char topic[MQTT_TOPIC_NAME_SZ];
        // char message[MQTT_MESSAGE_SZ];
        char *topic;
        char *message;
        uint16_t start, end;
        char *eot;

        s_urlDecode(iopPtr->rxDataBufs[iopBufIndx]->buffer, IOP_RX_DATABUF_SZ);

        topic = memchr(iopPtr->rxDataBufs[iopBufIndx]->buffer, ASCII_cDBLQUOTE, MQTT_TOPIC_OFFSET_MAX);
        if (topic == NULL)                                                      // malformed
            goto discardBuffer;
        topic++;

        eot = memchr(topic, ASCII_cDBLQUOTE, iopPtr->rxDataBufs[iopBufIndx]->head - eot);
        if (eot == NULL)                                                        // malformed, overflowed somehow
            goto discardBuffer;
        *eot  = ASCII_cNULL;                                                           // null term the topic

        message = eot + 3;                                                      // set the message start
        eot = memchr(message, ASCII_cCR, iopPtr->rxDataBufs[iopBufIndx]->head - eot);
        if (eot == NULL)                                                        // malformed, overflowed somehow
            goto discardBuffer;
        *(eot-1)  = ASCII_cNULL;                                                // null term the message (remove BGx trailing "\r\n)

        // find topic in subscriptions array & invoke application receiver
        for (size_t i = 0; i < MQTT_TOPIC_MAXCNT; i++)
        {
            uint16_t topicSz = strlen(mqttPtr->subscriptions[i].topicName);
            if (strncmp(mqttPtr->subscriptions[i].topicName, topic, topicSz) == 0)
            {
                //                                           (topic name,                                props,           message body)
                mqttPtr->subscriptions[i].receiver_func(mqttPtr->subscriptions[i].topicName, topic + topicSz, message);
                break;
            }
        }

        discardBuffer:
        mqttPtr->dataBufferIndx = IOP_NO_BUFFER;
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
static resultCode_t s_mqttOpenStatusParser(const char *response, char **endptr) 
{
    return atcmd_serviceResponseParser(response, "+QMTOPEN: ", 0, endptr);
}


/**
 *	\brief [private] MQTT open connection response parser.
 *
 *  \param response [in] Character data recv'd from BGx to parse for task complete
 *  \param endptr [out] Char pointer to the char following parsed text
 * 
 *  \return HTTP style result code, 0 = not complete
 */
static resultCode_t s_mqttOpenCompleteParser(const char *response, char **endptr) 
{
    return atcmd_serviceResponseParser(response, "+QMTOPEN: ", 1, endptr);
}


/**
 *	\brief [private] MQTT connect to server response parser.
 *
 *  \param response [in] Character data recv'd from BGx to parse for task complete
 *  \param endptr [out] Char pointer to the char following parsed text
 * 
 *  \return HTTP style result code, 0 = not complete
 */
static resultCode_t s_mqttConnectStatusParser(const char *response, char **endptr) 
{
    // BGx +QMTCONN Read returns Status = 3 for connected, service parser returns success code == 203
    resultCode_t rslt = atcmd_serviceResponseParser(response, "+QMTCONN: ", 1, endptr);
    return (rslt == RESULT_CODE_PENDING) ? RESULT_CODE_PENDING : (rslt == 203) ? RESULT_CODE_SUCCESS : RESULT_CODE_UNAVAILABLE;
}


/**
 *	\brief [private] MQTT connect to server response parser.
 *
 *  \param response [in] Character data recv'd from BGx to parse for task complete
 *  \param endptr [out] Char pointer to the char following parsed text
 * 
 *  \return HTTP style result code, RESULT_CODE_PENDING = not complete
 */
static resultCode_t s_mqttConnectCompleteParser(const char *response, char **endptr) 
{
    return atcmd_serviceResponseParser(response, "+QMTCONN: ", 2, endptr);
}


/**
 *	\brief [private] MQTT subscribe to topic response parser.
 *
 *  \param response [in] Character data recv'd from BGx to parse for task complete
 *  \param endptr [out] Char pointer to the char following parsed text
 * 
 *  \return HTTP style result code, RESULT_CODE_PENDING = not complete
 */
static resultCode_t s_mqttSubscribeCompleteParser(const char *response, char **endptr) 
{
    return atcmd_serviceResponseParser(response, "+QMTSUB: ", 2, endptr);
}


/**
 *	\brief [private] MQTT publish message to topic response parser.
 *
 *  \param response [in] Character data recv'd from BGx to parse for task complete
 *  \param endptr [out] Char pointer to the char following parsed text
 * 
 *  \return HTTP style result code, RESULT_CODE_PENDING = not complete
 */
static resultCode_t s_mqttPublishCompleteParser(const char *response, char **endptr) 
{
    return atcmd_serviceResponseParser(response, "+QMTPUB: ", 2, endptr);
}


#pragma endregion
