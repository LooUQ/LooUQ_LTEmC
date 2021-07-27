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
#if _DEBUG > 0
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG == 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #define PRINTF(c_,f_,__VA_ARGS__...) do { rtt_printf(c_, (f_), ## __VA_ARGS__); } while(0)
    #endif
#else
#define PRINTF(c_, f_, ...) 
#endif

#include "ltemc-mqtt.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define ASCII_CtrlZ_STR "\032"
#define ASCII_DblQuote_CHAR '"'

// #define MQTT_ACTION_CMD_SZ 81
// #define MQTT_CONNECT_CMD_SZ 300

extern ltemDevice_t g_ltem;

enum
{
    resultCode__parserPending = 0xFFFF
};

/* Local Function Declarations
 ----------------------------------------------------------------------------------------------- */
static resultCode_t S_mqttOpenStatusParser(const char *response, char **endptr);
static resultCode_t S_mqttOpenCompleteParser(const char *response, char **endptr);
static resultCode_t S_mqttConnectStatusParser(const char *response, char **endptr);
static resultCode_t S_mqttConnectCompleteParser(const char *response, char **endptr);
static resultCode_t S_mqttSubscribeCompleteParser(const char *response, char **endptr);
static resultCode_t S_mqttPublishCompleteParser(const char *response, char **endptr);
static void S_urlDecode(char *src, int len);
static void S_mqttDoWork();


/* public mqtt functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions


/**
 *  \brief Query the status of the MQTT server state.
 * 
 *  \param dataCntxt [in] Data context to host this protocol stream.
 *  \param useTls [in] Specifies if a SSL/TLS configuration has been applied to data context to protect communications.
 *  \param useMqttVersion [in] Specifies the MQTT protocol revision to use for communications.
 * 
 *  \returns A mqtt object to govern operations for this protocol stream value indicating the state of the MQTT connection.
*/
void mqtt_initControl(mqttCtrl_t *mqttCtrl, dataContext_t dataCntxt, bool useTls, mqttVersion_t useMqttVersion, uint8_t *recvBuf, uint16_t recvBufSz, mqttRecvFunc_t recvCallback)
{
    ASSERT(mqttCtrl != NULL && recvBuf != NULL && recvCallback != NULL, srcfile_sckt_c);
    ASSERT(dataCntxt < dataContext_cnt, srcfile_sckt_c);
    //ASSERT(protocol < protocol_socket, srcfile_sckt_c);

    memset(mqttCtrl, 0, sizeof(mqttCtrl_t));

    mqttCtrl->ctrlMagic = streams__ctrlMagic;
    mqttCtrl->dataCntxt = dataCntxt;
    mqttCtrl->protocol = protocol_mqtt;
    mqttCtrl->useTls = useTls;
    uint16_t bufferSz = IOP_initRxBufferCtrl(&(mqttCtrl->recvBufCtrl), recvBuf, recvBufSz);
    ASSERT_W(recvBufSz == bufferSz, srcfile_mqtt_c, "RxBufSz != multiple of 128B");
    ASSERT(bufferSz > 64, srcfile_mqtt_c);

    mqttCtrl->dataRecvCB = recvCallback;
    mqttCtrl->useMqttVersion = useMqttVersion;
    mqttCtrl->msgId = 1;
}


/**
 *  \brief Query the status of the MQTT server state.
 * 
 *  \param mqtt [in] Pointer to MQTT type stream control to operate on.
 *  \param host [in] A char string to match with the currently connected server. Host is not checked if empty string passed.
 *  \param force [in] If true query BGx for current state, otherwise return internal property value
 * 
 *  \returns A mqttStatus_t value indicating the state of the MQTT connection.
*/
mqttStatus_t mqtt_status(mqttCtrl_t *mqtt, const char *host, bool force)
{
    // See BG96_MQTT_Application_Note: AT+QMTOPEN? and AT+QMTCONN?

    ASSERT(mqtt->ctrlMagic == streams__ctrlMagic, srcfile_mqtt_c);   // check for bad pointer in

    if (!force && *host == 0)                                       // if not forcing host verification, return current memory value
        return mqtt->state;

    mqtt->state = mqttStatus_closed;

    // connect check first to short-circuit efforts
    atcmd_setOptions(atcmd__setLockModeAuto, PERIOD_FROM_SECONDS(5), S_mqttConnectStatusParser);
    if (atcmd_tryInvokeOptions("AT+QMTCONN?"))
    {
        resultCode_t atResult = atcmd_awaitResult();
        if (atResult == resultCode__success)
            mqtt->state = mqttStatus_connected;
    }

    if (mqtt->state != mqttStatus_connected || *host != 0)             // if not connected or need host verification: check open
    {
        atcmd_setOptions(atcmd__setLockModeAuto, PERIOD_FROM_SECONDS(5), S_mqttOpenStatusParser);
        if (atcmd_tryInvokeOptions("AT+QMTOPEN?"))
        {
            resultCode_t atResult = atcmd_awaitResult();
            if (atResult == resultCode__success)
            {
                mqtt->state = mqttStatus_open;
                if (*host != 0)                                // host matching requested, check modem response host == requested host
                {
                    char *hostNamePtr = strchr(atcmd_getLastResponse(), '"');
                    mqtt->state = (hostNamePtr != NULL && strncmp(hostNamePtr + 1, host, strlen(host)) == 0) ? mqttStatus_open : mqttStatus_closed;
                }
            }
        }
    }
    return mqtt->state;
}


/**
 *  \brief Open a remote MQTT server for use.
 * 
 *  \param mqtt [in] MQTT type stream control to operate on.
 *  \param host [in] The host IP address or name of the remote server.
 *  \param port [in] The IP port number to use for the communications.
 * 
 *  \returns A resultCode_t value indicating the success or type of failure.
*/
resultCode_t mqtt_open(mqttCtrl_t *mqtt, const char *host, uint16_t port)
{
    // AT+QSSLCFG="sslversion",5,3
    // AT+QMTCFG="ssl",5,1,0
    // AT+QMTCFG="version",5,4
    // AT+QMTOPEN=5,"iothub-dev-pelogical.azure-devices.net",8883

    atcmdResult_t atResult;

    mqtt->state = mqtt_status(mqtt, host, true);        // refresh state, state must be not open for config changes
    if (mqtt->state >= mqttStatus_open)                     // already open+connected with server "host"
        return resultCode__success;

    if (mqtt->useTls)
    {
        if (atcmd_tryInvokeDefaults("AT+QMTCFG=\"ssl\",%d,1,%d", mqtt->dataCntxt, mqtt->dataCntxt))
        {
            if (atcmd_awaitResult() != resultCode__success)
                return resultCode__internalError;
        }
    }

    if (mqtt-> useMqttVersion == mqttVersion_311)
    {
        if (atcmd_tryInvokeDefaults("AT+QMTCFG=\"version\",%d,4", mqtt->dataCntxt))
        {
            if (atcmd_awaitResult() != resultCode__success)
                return resultCode__internalError;
        }
    }

    // TYPICAL: AT+QMTOPEN=0,"iothub-dev-pelogical.azure-devices.net",8883
    atcmd_setOptions(atcmd__setLockModeAuto, PERIOD_FROM_SECONDS(45), S_mqttOpenCompleteParser);
    if (atcmd_tryInvokeOptions("AT+QMTOPEN=%d,\"%s\",%d", mqtt->dataCntxt, host, port))
    {
        resultCode_t atResult = atcmd_awaitResult();
        if (atResult >= 200 && atResult < 300)                          // opened mqtt server
        {
            ((iop_t*)g_ltem.iop)->mqttMap &= 0x01 & mqtt->dataCntxt;
            mqtt->state = mqttStatus_open;
            return resultCode__success;
        }
        else
        {
            switch (atResult)
            {
                case 899:
                case 903:
                case 905:
                    return resultCode__gone;
                case 901:
                    return resultCode__badRequest;
                case 902:
                    return resultCode__conflict;
                case 904:
                    return resultCode__notFound;
                default:
                    return resultCode__internalError;
            }
        }
    }
}


/**
 *  \brief Disconnect and close a connection to a MQTT server
 * 
 *  \param mqtt [in] Pointer to MQTT type stream control to operate on.
*/
void mqtt_close(mqttCtrl_t *mqtt)
{
    //char actionCmd[MQTT_ACTION_CMD_SZ] = {0};

    mqtt->state = mqttStatus_closed;
    ((iop_t*)g_ltem.iop)->mqttMap &= ~(0x01 & mqtt->dataCntxt);
    ((iop_t*)g_ltem.iop)->streamPeers[mqtt->dataCntxt] = NULL;

    for (size_t i = 0; i < sizeof(mqtt->topicSubs); i++)          // clear topicSubs table
    {
        mqtt->topicSubs[i].topicName[0] = 0;
    }

    // if (mqtt->state == mqttStatus_connected)
    // {
    //     if (atcmd_tryInvokeDefaults("AT+QMTDISC=%d", mqtt->dataCntxt))
    //         atcmd_awaitResult(true);
    // }
    if (mqtt->state >= mqttStatus_open)
    {
        if (atcmd_tryInvokeDefaults("AT+QMTCLOSE=%d", mqtt->dataCntxt))
            atcmd_awaitResult();
    }
    mqtt->state == mqttStatus_closed;
    return resultCode__success;
}


/**
 *  \brief Connect (authenticate) to a MQTT server.
 * 
 *  \param mqtt [in] Pointer to MQTT type stream control to operate on.
 *  \param clientId [in] - The client or device identifier for the connection.
 *  \param username [in] - The user identifier or name for the connection to authenticate.
 *  \param password [in] - The secret string or phrase to authenticate the connection.
 *  \param cleanSession [in] - Directs MQTT to preserve or flush messages received prior to the session start.
 * 
 *  \returns A resultCode_t value indicating the success or type of failure, OK = 200.
*/
resultCode_t mqtt_connect(mqttCtrl_t *mqtt, const char *clientId, const char *username, const char *password, mqttSession_t cleanSession)
{
    // char actionCmd[MQTT_CONNECT_CMD_SZ] = {0};
    resultCode_t atResult;

    if (mqtt->state == mqttStatus_connected)       // already connected, trusting internal state as this is likely immediately after open
        return resultCode__success;                // mqtt_open forces mqtt state sync with BGx

    if (atcmd_tryInvokeDefaults("AT+QMTCFG=\"session\",%d,%d", mqtt->dataCntxt, (uint8_t)cleanSession))
    {
        if (atcmd_awaitResult() != resultCode__success)
            return resultCode__internalError;
    }

    // snprintf(actionCmd, MQTT_CONNECT_CMD_SZ, "AT+QMTCONN=%d,\"%s\",\"%s\",\"%s\"", mqtt->dataCntxt, clientId, username, password);
    atcmd_setOptions(atcmd__setLockModeAuto, PERIOD_FROM_SECONDS(60), S_mqttConnectCompleteParser);
    if (atcmd_tryInvokeOptions("AT+QMTCONN=%d,\"%s\",\"%s\",\"%s\"", mqtt->dataCntxt, clientId, username, password))
    {
        atResult = atcmd_awaitResult();
        switch (atResult)
        {
            case resultCode__success:
                ((iop_t*)g_ltem.iop)->mqttMap &= 0x01 << mqtt->dataCntxt;
                mqtt->state = mqttStatus_connected;
                return resultCode__success;
            case 201:
            case 202:
            case 204:
                return resultCode__badRequest;
            case 203:
                return resultCode__unavailable;
            case 205:
                return resultCode__forbidden;
            default:
                return resultCode__internalError;
        }
    }
    return resultCode__badRequest;          // bad parameters assumed
}



/**
 *  \brief Subscribe to a topic on the MQTT server.
 * 
 *  \param mqtt [in] Pointer to MQTT type stream control to operate on.
 *  \param topic [in] - The messaging topic to subscribe to.
 *  \param qos [in] - The MQTT QOS level for messages subscribed to.
 * 
 *  \returns The topic index handle. 0xFF indicates the topic subscription was unsuccessful
 */
uint8_t mqtt_subscribe(mqttCtrl_t *mqttCtrl, const char *topic, mqttQos_t qos)
{
    // char actionCmd[MQTT_ACTION_CMD_SZ] = {0};
    uint8_t topicIndx = 0xFF;
    uint16_t topicSz = strlen(topic);

    ASSERT(topicSz >= mqtt__topic_nameSz, srcfile_mqtt_c);

    bool wildcard = *(topic + topicSz - 1) == '#';      // test for MQTT multilevel wildcard, store separately for future topic parsing on recv
    char topicEntryName[mqtt__topic_nameSz] = {0};

    strncpy(topicEntryName, topic, wildcard ? topicSz - 1 : topicSz);

    bool alreadySubscribed = false;
    for (size_t i = 0; i < sizeof(mqttCtrl->topicSubs); i++)
    {
        if (strcmp(mqttCtrl->topicSubs[i].topicName, topicEntryName) == 0)
        {
            alreadySubscribed = true;
            topicIndx = i;
            break;
        }
    }
    if (!alreadySubscribed)
    {
        for (size_t i = 0; i < sizeof(mqttCtrl->topicSubs); i++)
        {
            if (mqttCtrl->topicSubs[i].topicName[0] == 0)
            {
                strncpy(mqttCtrl->topicSubs[i].topicName, topicEntryName, strlen(topicEntryName)+1);
                if (wildcard)
                    mqttCtrl->topicSubs[i].wildcard = '#';
                topicIndx = i;
                break;
            }
        }
    }
    if (topicIndx == 0xFF)
        return resultCode__conflict;

    // BGx doesn't provide a test for an existing subscription, but is tolerant of a duplicate create subscription 
    // if sucessful, the topic's subscription will overwrite the IOP peer map without issue as well (same bitmap value)

    // snprintf(actionCmd, MQTT_ACTION_CMD_SZ, "AT+QMTSUB=%d,%d,\"%s\",%d", mqtt->dataCntxt, ++mqtt->msgId, topic, qos);
    atcmd_setOptions(atcmd__setLockModeAuto, PERIOD_FROM_SECONDS(15), S_mqttSubscribeCompleteParser);
    if (atcmd_tryInvokeOptions("AT+QMTSUB=%d,%d,\"%s\",%d", mqttCtrl->dataCntxt, ++mqttCtrl->msgId, topic, qos))
    {
        resultCode_t atResult = atcmd_awaitResult();
        if (atResult == resultCode__success)
        {
            LTEM_registerDoWorker(S_mqttDoWork);                    // need to register worker if 1 or more subscriptions
            return atResult;
        }
        else
            mqttCtrl->topicSubs[topicIndx].topicName[0] = 0;        // if error on BGx subscribe, give table entry back
    }
    return resultCode__badRequest;
}



/**
 *  \brief Unsubscribe to a topic on the MQTT server.
 * 
 *  \param mqtt [in] Pointer to MQTT type stream control to operate on.
 *  \param topic [in] - The messaging topic to unsubscribe from.
 * 
 *  \returns A resultCode_t (http status type) value indicating the success or type of failure, OK = 200.
*/
resultCode_t mqtt_unsubscribe(mqttCtrl_t *mqtt, const char *topic)
{
    // AT+QMTUNS=<tcpconnectID>,<msgID>,"<topic1>"

    // char actionCmd[mqtt__topic_publishCmdSz] = {0};
    uint8_t topictopicIndx = 255;

    uint16_t topicSz = strlen(topic);                   // adjust topic if multilevel wildcard, remove prior to topicSubs scan
    if (*(topic + topicSz) == "#")
        topicSz--;

    for (size_t i = 0; i < sizeof(mqtt->topicSubs); i++)
    {
        if (strncmp(mqtt->topicSubs[i].topicName, topic, topicSz) == 0)
        {
            mqtt->topicSubs[i].topicName[0] = 0;
            topictopicIndx = i;
            break;
        }
    }
    
    // snprintf(actionCmd, MQTT_CONNECT_CMD_SZ, "AT+QMTUNS=%d,%d,\"<topic1>\"", mqtt->dataCntxt, ++mqtt->msgId, topic);
    if (atcmd_tryInvoke("AT+QMTUNS=%d,%d,\"<topic1>\"", mqtt->dataCntxt, ++mqtt->msgId, topic))
    {
        if (atcmd_awaitResult() == resultCode__success)
        {
            // unregister worker if this is the last subscription being "unsubscribed"
            return resultCode__success;
        }
    }
    return resultCode__badRequest;
}


/**
 *  \brief Publish a message to server.
 * 
 *  \param mqtt [in] Pointer to MQTT type stream control to operate on.
 *  \param topic [in] - Pointer to the message topic (see your server for topic formatting details).
 *  \param qos [in] - The MQTT QOS to be assigned to sent message.
 *  \param message [in] - Pointer to message to be sent.
 * 
 *  \returns A resultCode_t value indicating the success or type of failure (http status type code).
*/
resultCode_t mqtt_publish(mqttCtrl_t *mqtt, const char *topic, mqttQos_t qos, const char *message)
{
    // AT+QMTPUB=<tcpconnectID>,<msgID>,<qos>,<retain>,"<topic>"
    //char publishCmd[MQTT_TOPIC_PUBBUF_SZ] = {0};
    char msgText[mqtt__messageSz];
    resultCode_t atResult;

    // register the pending publish action
    uint16_t msgId = ((uint8_t)qos == 0) ? 0 : ++mqtt->msgId;
    // snprintf(publishCmd, MQTT_TOPIC_PUBBUF_SZ, "AT+QMTPUB=%d,%d,%d,0,\"%s\"", mqtt->dataCntxt, msgId, qos, topic);
    
    atcmd_setOptions(atcmd__setLockModeManual, atcmd__useDefaultTimeout, atcmd_txDataPromptParser);
    if (atcmd_awaitLock(atcmd__useDefaultTimeout))
    {
        atcmd_invokeReuseLock("AT+QMTPUB=%d,%d,%d,0,\"%s\"", mqtt->dataCntxt, msgId, qos, topic);
        atResult = atcmd_awaitResult();

        if (atResult == resultCode__success)                    // wait for data prompt for data, now complete sub-command to actually transfer data
        {
            atcmd_setOptions(atcmd__setLockModeManual, mqtt__publishTimeout, S_mqttPublishCompleteParser);
            atcmd_sendCmdData(message, strlen(message), ASCII_CtrlZ_STR);
            atResult = atcmd_awaitResult();
        }
        else
            atcmd_exitTextMode();                               // if any problem, make sure BGx is out of text mode
    }
    return atResult;
}


/**
 *  \brief Performs URL escape removal for special char (%20-%2F) without malloc.
 * 
 *  \param src [in] - Input text string to URL decode.
 *  \param len [in] - Length of input text string.
*/
static void S_urlDecode(char *src, int len)
{
    char subTable[] = " !\"#$%&'()*+,-./";
    uint8_t srcChar;
    uint8_t subKey;
    uint16_t dest = 0;

    for (size_t i = 0; i < len; i++)
    {
        if (src[i] == '\0')
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

S_processReceivedMessage(dataContext_t dataCntxt);



/**
 *  \brief Performs background tasks to advance MQTT pipeline dataflows.
*/
void S_mqttDoWork()
{
    static uint32_t _lastExecTk;                                // tick count when doWork() was last EXECUTED. used by progress checking
    static uint64_t _bufferFillState[dataContext_cnt];

    if (!pElapsed(_lastExecTk, IOP__uartFIFO_fillMS + 1))       // watching IOP\ISR fill: if less than 1 UART buffer won't see anything
        return;

    iop_t *iopPtr = (iop_t*)g_ltem.iop;
    _lastExecTk = pMillis();

    for (size_t i = 0; i < dataContext_cnt; i++)
    {
        if (iopPtr->mqttMap & 0x01 < i)                         // mask off MQTT active bit, i == MQTT active context
        {
            mqttCtrl_t *mqtt = ((mqttCtrl_t*)iopPtr->streamPeers[i]);
            rxDataBufferCtrl_t *bufPtr = &mqtt->recvBufCtrl;
            uint16_t available = IOP_rxPageDataAvailable(bufPtr, bufPtr->iopPg);
            if (available == _bufferFillState[i])               // buffer has not received addt. chars, assume idle stream
            {
                S_processReceivedMessage(i);
            }
        }
    }
}


S_processReceivedMessage(dataContext_t dataCntxt)
{
    iop_t *iopPtr = (iop_t*)g_ltem.iop;

    // readability vars
    mqttCtrl_t *mqttPtr = ((mqttCtrl_t *)iopPtr->rxStreamCtrl);
    ASSERT(mqttPtr == iopPtr->streamPeers[dataCntxt], srcfile_mqtt_c);  // ASSERT dataCntxt cross-links are still valid
    rxDataBufferCtrl_t *bufPtr = &(mqttPtr->recvBufCtrl);               // smart-buffer for this operation

    // parse received MQTT message into topic and message
    // Example: +QMTRECV: 0,0, "topic/example", "This is the payload related to topic"

    char *topic;
    uint8_t topicIndx;
    char *message;
    uint16_t messageSz = 0;
    
    // uint16_t mqttAvailable = rxPageDataAvailable(bufPtr, !bufPtr->iopPg);
    // uint16_t iopAvailable = rxPageDataAvailable(bufPtr, bufPtr->iopPg);

    char *head = bufPtr->pages[0].head;                                 // create full buffer pointers for head and tail (combine pages)
    char *tail = bufPtr->pages[0].tail;
    if (bufPtr->iopPg == 1)
        tail = bufPtr->pages[1].tail;                                   // ignore page properties now and don't use rxPageDataAvailable()

// uint16_t bufferFillTimeout = IOP_rxPageFillTimeout(bufPtr);                 // wait for ISR to nearly fill buffer or timeout (EOT)
// uint32_t waitOnBuffer_Start = pMillis();
// while (dataAvailable < bufPtr->_pageSz - iop__uartFIFOBufferSz)
// {
//     if (pElapsed(waitOnBuffer_Start, bufferFillTimeout))
//     {
//         isFinal = true;
//         break;
//     }
//     dataAvailable = rxPageDataAvailable(bufPtr, bufPtr->iopPg);
// }
// IOP_swapRxBufferPage(bufPtr);                                               

    S_urlDecode(tail, head - tail);
    topic = memchr(bufPtr->pages[!bufPtr->iopPg].tail, '"', mqtt__topicOffset);
    if (topic == NULL)                                                          // malformed
        goto discardBuffer;

    tail = ++topic;                                                             // point past opening dblQuote
    tail = memchr(tail, ASCII_DblQuote_CHAR, head - tail);
    if (tail == NULL)                                                           // malformed, overflowed somehow
        goto discardBuffer;


    message = tail += 3;                                                        // set the message start
    tail = memchr(tail, '\"\r\n\r\n\r\n', head - tail);                         // BGx is sloppy on MQTT end-of-message, look for dblQuote + 3 line ends
    if (tail == NULL)                                                           // malformed, overflowed somehow
        goto discardBuffer;

    *(tail-1)  = '\0';                                                          // null term the message (remove BGx trailing "\r\n)
    messageSz = tail - message - 1;

    /* find topic in subscriptions array & invoke application receiver */
    for (size_t i = 0; i < mqtt__topicCnt; i++)
    {
        uint16_t topicSz = strlen(mqttPtr->topicSubs[i].topicName);
        if (strncmp(mqttPtr->topicSubs[i].topicName, topic, topicSz) == 0)
        {
            //                                props here (null-terminated)
            mqttPtr->dataRecvCB(dataCntxt, i, topic + topicSz, message, messageSz);
            break;
        }
    }
    discardBuffer:
    IOP_resetRxDataBufferPage(bufPtr, 0, true);               // delivered, clear buffer
    IOP_resetRxDataBufferPage(bufPtr, 1, true);               // delivered, clear buffer
    iopPtr->rxStreamCtrl = NULL;
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
static resultCode_t S_mqttOpenStatusParser(const char *response, char **endptr) 
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
static resultCode_t S_mqttOpenCompleteParser(const char *response, char **endptr) 
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
static resultCode_t S_mqttConnectStatusParser(const char *response, char **endptr) 
{
    // BGx +QMTCONN Read returns Status = 3 for connected, service parser returns success code == 203
    resultCode_t rslt = atcmd_serviceResponseParser(response, "+QMTCONN: ", 1, endptr);
    if (rslt == 0xFFFF)
        return 0XFFFF;

    return (rslt == 203) ? resultCode__success : resultCode__unavailable;
}


/**
 *	\brief [private] MQTT connect to server response parser.
 *
 *  \param response [in] Character data recv'd from BGx to parse for task complete
 *  \param endptr [out] Char pointer to the char following parsed text
 * 
 *  \return HTTP style result code, RESULT_CODE_PENDING = not complete
 */
static resultCode_t S_mqttConnectCompleteParser(const char *response, char **endptr) 
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
static resultCode_t S_mqttSubscribeCompleteParser(const char *response, char **endptr) 
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
static resultCode_t S_mqttPublishCompleteParser(const char *response, char **endptr) 
{
    return atcmd_serviceResponseParser(response, "+QMTPUB: ", 2, endptr);
}


#pragma endregion
