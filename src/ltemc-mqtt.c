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

#define _DEBUG 2                        // set to non-zero value for PRINTF debugging output, 
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

#define SRCFILE "MQT"                           // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT
#include "ltemc-internal.h"
#include "ltemc-mqtt.h"

extern ltemDevice_t g_lqLTEM;


#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define ASCII_CtrlZ_STR "\x1A"
#define ASCII_ESC_STR "\x1B"
#define ASCII_DblQuote_CHAR '"'


enum
{
    resultCode__parserPending = 0xFFFF
};

// #define MQTT_ACTION_CMD_SZ 81
// #define MQTT_CONNECT_CMD_SZ 300


/* Local Function Declarations
 ----------------------------------------------------------------------------------------------- */

static void S__mqttUrcHandler();

//static cmdParseRslt_t S__mqttOpenStatusParser();
static cmdParseRslt_t S__mqttOpenCompleteParser();
static cmdParseRslt_t S__mqttConnectCompleteParser();
static cmdParseRslt_t S__mqttConnectStatusParser();
static cmdParseRslt_t S__mqttSubscribeCompleteParser();
static cmdParseRslt_t S__mqttPublishCompleteParser();


/* public mqtt functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions


/**
 *  \brief Initialize a MQTT protocol control structure.
*/
void mqtt_init(mqttCtrl_t *mqttCtrl, dataCntxt_t dataCntxt, uint8_t *recvBuf, uint16_t recvBufSz, mqttRecv_func recvCallback)
{
    ASSERT(dataCntxt < dataCntxt__cnt);                                 // valid streams index
    ASSERT(strlen(g_lqLTEM.streams[dataCntxt].streamType) == 0);        // context not already in use

    memset(mqttCtrl, 0, sizeof(mqttCtrl_t));
    memcpy(g_lqLTEM.streams[dataCntxt].streamType, STREAM_MQTT, streams__typeCodeSz);

    g_lqLTEM.streams[dataCntxt].pCtrl = mqttCtrl;
    LTEM_registerUrcHandler(S__mqttUrcHandler);
    g_lqLTEM.streams[dataCntxt].recvDataCB = recvCallback;
}


/**
 *  \brief Set the remote server connection values.
*/
void mqtt_setConnection(mqttCtrl_t *mqttCtrl, const char *hostUrl, uint16_t hostPort, bool useTls, mqttVersion_t mqttVersion, const char *clientId, const char *username, const char *password)
{
    ASSERT(mqttCtrl != NULL);

    strcpy(mqttCtrl->hostUrl, hostUrl);
    mqttCtrl->hostPort = hostPort;
    mqttCtrl->useTls = useTls;
    mqttCtrl->mqttVersion = mqttVersion;

    strncpy(mqttCtrl->clientId, clientId, mqtt__clientIdSz);
    strncpy(mqttCtrl->username, username, mqtt__userNameSz);
    strncpy(mqttCtrl->password, password, mqtt__userPasswordSz);
}


/**
 *  \brief Open a remote MQTT server for use.
*/
resultCode_t mqtt_open(mqttCtrl_t *mqttCtrl)
{
    // AT+QSSLCFG="sslversion",5,3
    // AT+QMTCFG="ssl",5,1,0
    // AT+QMTCFG="version",5,4
    // AT+QMTOPEN=5,"iothub-dev-pelogical.azure-devices.net",8883

    atcmdResult_t atResult;

    if (mqttCtrl->state >= mqttState_open)                      // already open or connected
        return resultCode__success;
    // if (mqttCtrl->state != mqttState_closed)                    // not in a closed state, (most) mqtt setting changes require closed connection
    //     return resultCode__preConditionFailed;

    // set options prior to open
    if (mqttCtrl->useTls)
    {
        if (atcmd_tryInvoke("AT+QMTCFG=\"ssl\",%d,1,%d", mqttCtrl->dataContext, mqttCtrl->dataContext))
        {
            if (atcmd_awaitResult() != resultCode__success)
                return resultCode__internalError;
        }
    }
    // AT+QMTCFG="version",0,4
    if (atcmd_tryInvoke("AT+QMTCFG=\"version\",%d,4", mqttCtrl->dataContext, mqttCtrl->mqttVersion))
    {
        if (atcmd_awaitResult() != resultCode__success)
            return resultCode__internalError;
    }

    // TYPICAL: AT+QMTOPEN=0,"iothub-dev-pelogical.azure-devices.net",8883
    if (atcmd_tryInvoke("AT+QMTOPEN=%d,\"%s\",%d", mqttCtrl->dataContext, mqttCtrl->hostUrl, mqttCtrl->hostPort))
    {
        resultCode_t atResult = atcmd_awaitResultWithOptions(PERIOD_FROM_SECONDS(45), S__mqttOpenCompleteParser);
        if (atResult == resultCode__success && atcmd_getValue() == 0)
        {
            mqttCtrl->state = mqttState_open;
            // g_lqLTEM.atcmd->mqttMap |= 0x01 << mqttCtrl->dataContext;
            // g_lqLTEM.atcmd->streamPeers[mqttCtrl->dataContext] = mqttCtrl;
            // LTEM_registerDoWorker(S__mqttDoWork);                                // register background recv worker
            return resultCode__success;
        }
        else
        {
            switch (atcmd_getValue())
            {
                case -1:
                case 1:
                    return resultCode__badRequest;
                case 2:
                    return resultCode__conflict;
                case 4:
                    return resultCode__notFound;
                default:
                    return resultCode__gtwyTimeout;
            }
        }
    }
}


/**
 *  \brief Connect (authenticate) to a MQTT server.
 *  \param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  \param clientId [in] - The client or device identifier for the connection.
 *  \param username [in] - The user identifier or name for the connection to authenticate.
 *  \param password [in] - The secret string or phrase to authenticate the connection.
 *  \param cleanSession [in] - Directs MQTT to preserve or flush messages received prior to the session start.
 *  \return A resultCode_t value indicating the success or type of failure, OK = 200.
*/
resultCode_t mqtt_connect(mqttCtrl_t *mqttCtrl, bool cleanSession)
{
    resultCode_t atResult;
    if (mqttCtrl->state == mqttState_connected)
        return resultCode__success;

    if (atcmd_tryInvoke("AT+QMTCFG=\"session\",%d,%d", mqttCtrl->dataContext, (uint8_t)cleanSession))     // set option to clear session history on connect
    {
        if (atcmd_awaitResult() != resultCode__success)
            return resultCode__internalError;
    }

    /* MQTT connect command can be quite large, using local buffer here rather than bloat global cmd\core buffer */
    char connectCmd[384] = {0};
    snprintf(connectCmd, sizeof(connectCmd), "AT+QMTCONN=%d,\"%s\",\"%s\",\"%s\"", mqttCtrl->dataContext, mqttCtrl->clientId, mqttCtrl->username, mqttCtrl->password);

    if (ATCMD_awaitLock(atcmd__defaultTimeout))                             // to use oversized MQTT buffer, we need to use sendCmdData()
    {      
        atcmd_reset(false);
        atcmd_sendCmdData(connectCmd, strlen(connectCmd), "\r");
        atResult = atcmd_awaitResultWithOptions(PERIOD_FROM_SECONDS(60), S__mqttConnectCompleteParser);     // in autolock mode, so this will release lock
        if (atResult == resultCode__success)
        {
            switch (atcmd_getValue())
            {
                case 0:
                    return resultCode__success;
                case 1:
                    return resultCode__methodNotAllowed;        // invalid protocol version 
                case 2:               
                case 4:
                case 5:
                    return resultCode__unauthorized;            // bad user ID or password
                case 3:
                    return resultCode__unavailable;             // server unavailable
                default:
                    return resultCode__internalError;
            }
        }
        // else if (atResult == resultCode__timeout)                           // assume got a +QMTSTAT back not +QMTCONN
        // {
        //     char *continuePtr = strstr(atcmd_getLastResponse(), "+QMTSTAT: ");
        //     if (continuePtr != NULL)
        //     {
        //         continuePtr += 12;
        //         atResult = atol(continuePtr) + 200;
        //     }
        // }
    }
    return resultCode__badRequest;          // bad parameters assumed
}


/**
 *  \brief Subscribe to a topic on the MQTT server.
 *  \param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  \param topic [in] - The messaging topic to subscribe to.
 *  \param qos [in] - The MQTT QOS level for messages subscribed to.
 *  \return The topic index handle. 0xFF indicates the topic subscription was unsuccessful
 */
resultCode_t mqtt_subscribe(mqttCtrl_t *mqttCtrl, const char *topic, mqttQos_t qos)
{
    ASSERT(strlen(topic) < mqtt__topic_nameSz);

    resultCode_t atResult = 400;

    // BGx doesn't provide a test for an existing subscription, but is tolerant of a duplicate create subscription 
    // if sucessful, the topic's subscription will overwrite the IOP peer map without issue as well (same bitmap value)

    // snprintf(actionCmd, MQTT_ACTION_CMD_SZ, "AT+QMTSUB=%d,%d,\"%s\",%d", mqtt->sckt, ++mqtt->msgId, topic, qos);
    if (atcmd_tryInvoke("AT+QMTSUB=%d,%d,\"%s\",%d", mqttCtrl->dataContext, ++mqttCtrl->lastMsgId, topic, qos))
    {
        atResult = atcmd_awaitResultWithOptions(PERIOD_FROM_SECONDS(30), S__mqttSubscribeCompleteParser);
        if (atResult == resultCode__success)
        {
            S__updateSubscriptionsTable(mqttCtrl, true, topic);
        }
    }
    return atResult;
}


/**
 *  \brief Unsubscribe to a topic on the MQTT server.
 *  \param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  \param topic [in] - The messaging topic to unsubscribe from.
 *  \return A resultCode_t (http status type) value indicating the success or type of failure, OK = 200.
*/
resultCode_t mqtt_unsubscribe(mqttCtrl_t *mqttCtrl, const char *topic)
{
    // snprintf(actionCmd, MQTT_CONNECT_CMD_SZ, "AT+QMTUNS=%d,%d,\"<topic1>\"", mqtt->sckt, ++mqtt->msgId, topic);
    if (atcmd_tryInvoke("AT+QMTUNS=%d,%d,\"%s\"", mqttCtrl->dataContext, ++mqttCtrl->lastMsgId, topic))
    {
        if (atcmd_awaitResult() == resultCode__success)
        {
            S__updateSubscriptionsTable(mqttCtrl, false, topic);
            return resultCode__success;
        }
    }
    return resultCode__badRequest;
}


/**
 *  \brief Publish an encoded message to server.
 *  \param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  \param topic [in] - Pointer to the message topic (see your server for topic formatting details).
 *  \param qos [in] - The MQTT QOS to be assigned to sent message.
 *  \param encodedMsg [in] - Pointer to message to be sent.
 *  \param timeoutSeconds [in] - Seconds to wait for publish to complete, highly dependent on network and remote server.
 *  \return A resultCode_t value indicating the success or type of failure (http status type code).
*/
resultCode_t mqtt_publishEncoded(mqttCtrl_t *mqttCtrl, const char *topic, mqttQos_t qos, const char *encodedMsg, uint8_t timeoutSeconds)
{
    ASSERT(strlen(encodedMsg) <= 560);                                               // max msg length PUBEX=560, PUB=4096
    ASSERT(strchr(encodedMsg, '"') == NULL);

    uint32_t timeoutMS = (timeoutSeconds == 0) ? mqtt__publishTimeout : timeoutSeconds * 1000;
    char msgText[mqtt__messageSz];
    resultCode_t atResult = resultCode__conflict;               // assume lock not obtainable, conflict

    mqttCtrl->lastMsgId++;                                                                                      // keep sequence going regardless of MQTT QOS
    uint16_t msgId = ((uint8_t)qos == 0) ? 0 : mqttCtrl->lastMsgId;                                             // msgId not sent with QOS == 0, otherwise sent

    // AT+QMTPUBEX=<tcpconnectID>,<msgID>,<qos>,<retain>,"<topic>","<msg,len<=(560)>"
    if (atcmd_tryInvoke("AT+QMTPUBEX=%d,%d,%d,0,\"%s\",\"%s\"", mqttCtrl->dataContext, msgId, qos, topic, encodedMsg))
    {
        atResult = atcmd_awaitResultWithOptions(PERIOD_FROM_SECONDS(timeoutSeconds), NULL);
        if (atResult != resultCode__success)                                        
        {
            PRINTF(dbgColor__dYellow, "MQTT-PUB ERROR: rslt=%d(%d)\r", atResult, atcmd_getValue());
        }
    }
    atcmd_close();
    return atResult;
}


/** mqtt_publish()
 *  \brief Publish a message to server.
 *  \param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  \param topic [in] - Pointer to the message topic (see your server for topic formatting details).
 *  \param qos [in] - The MQTT QOS to be assigned to sent message.
 *  \param message [in] - Pointer to message to be sent.
 *  \param timeoutSeconds [in] - Seconds to wait for publish to complete, highly dependent on network and remote server.
 *  \return A resultCode_t value indicating the success or type of failure (http status type code).
*/
resultCode_t mqtt_publish(mqttCtrl_t *mqttCtrl, const char *topic, mqttQos_t qos, const char *message, uint8_t timeoutSeconds)
{
    ASSERT(strlen(message) <= 4096);                                               // max msg length PUBEX=560, PUB=4096
    uint8_t pubstate = 0;
    uint32_t timeoutMS = (timeoutSeconds == 0) ? mqtt__publishTimeout : timeoutSeconds * 1000;
    char msgText[mqtt__messageSz];
    resultCode_t atResult = resultCode__conflict;               // assume lock not obtainable, conflict
    if (ATCMD_awaitLock(timeoutMS))
    {
        mqttCtrl->lastMsgId++;                                                                                      // keep sequence going regardless of MQTT QOS
        uint16_t msgId = ((uint8_t)qos == 0) ? 0 : mqttCtrl->lastMsgId;                                             // msgId not sent with QOS == 0, otherwise sent
        // AT+QMTPUB=<tcpconnectID>,<msgID>,<qos>,<retain>,"<topic>"
        atcmd_invokeReuseLock("AT+QMTPUB=%d,%d,%d,0,\"%s\"", mqttCtrl->dataContext, msgId, qos, topic);
        pubstate++;
        atResult = atcmd_awaitResultWithOptions(PERIOD_FROM_SECONDS(timeoutSeconds), ATCMD_txDataPromptParser);     // wait for data prompt 
        if (atResult == resultCode__success)                                        
        {
            pubstate++;
            atcmd_sendCmdData(message, strlen(message), ASCII_CtrlZ_STR);                                           // now send data
            atResult = atcmd_awaitResultWithOptions(timeoutMS, S__mqttPublishCompleteParser);
            if (atResult == resultCode__success)
            {
                atcmd_close();
                return resultCode__success;
            }
        }
    }
    atcmd_close();
    atcmd_exitTextMode();                                                                                           // if any problem, make sure BGx is out of "text" mode
    PRINTF(dbgColor__dYellow, "MQTT-PUB ERROR: state=%d, rslt=%d(%d)\r", pubstate, atResult, atcmd_getValue());
    return atResult;
}


/**
 *  \brief Disconnect and close a connection to a MQTT server
*/
void mqtt_close(mqttCtrl_t *mqttCtrl)
{
    S__mqttResetContext(mqttCtrl->dataContext);
    // /*
    //         mqttCtrl->state = mqttState_open;
    //         g_lqLTEM.iop->mqttMap |= 0x01 << mqttCtrl->dataContext;
    //         g_lqLTEM.iop->streamPeers[mqttCtrl->dataContext] = mqttCtrl;
    //         LTEM_registerDoWorker(S__mqttDoWork);                                // register background recv worker
    // */

    // for (size_t i = 0; i < sizeof(mqttCtrl->topicSubs); i++)                                    // clear topic subscriptions table
    // {
    //     mqttCtrl->topicSubs[i].topicName[0] = 0;
    // }

    // /* not fully documented how Quectel intended to use close/disconnect, LTEmC uses AT+QMTCLOSE which appears to work for both open (NC) and connected states
    // */
    // if (mqttCtrl->state >= mqttState_open)                                                      // LTEmC uses CLOSE
    // {
    //     if (atcmd_tryInvoke("AT+QMTCLOSE=%d", mqttCtrl->dataContext))
    //         atcmd_awaitResultWithOptions(5000, NULL);
    // }
    // mqttCtrl->state == mqttState_closed;
    // return resultCode__success;
}


/**
 *  \brief Disconnect and close a connection to a MQTT server
 *  \param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  \param resetModem [in] True if modem should be reset prior to reestablishing MQTT connection.
*/
void mqtt_reset(mqttCtrl_t *mqttCtrl, bool resetModem)
{}


/**
 *  \brief Query the status of the MQTT server state.
 *  \param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  \param host [in] A char string to match with the currently connected server. Host is not checked if empty string passed.
 *  \return A mqttState_t value indicating the state of the MQTT connection.
*/
mqttState_t mqtt_fetchStatus(mqttCtrl_t *mqttCtrl)
{
    // See BG96_MQTT_Application_Note: AT+QMTOPEN? and AT+QMTCONN?


    /* BGx modules will not respond over serial to AT+QMTOPEN? (command works fine over USB, Quectel denies this)
     * AT+QMTCONN? returns a state == 1 (MQTT is initializing) when MQTT in an open, not connected condition
    */

    resultCode_t atResult;
    if (atcmd_tryInvoke("AT+QMTCONN?"))
    {
        atResult = atcmd_awaitResultWithOptions(PERIOD_FROM_SECONDS(5), S__mqttConnectStatusParser);
        if (atResult == resultCode__success)
        {
            if (atcmd_getPreambleFound())
            {
                switch (atcmd_getValue())
                {
                    case 1:
                        mqttCtrl->state = mqttState_open;
                        break;
                    case 3:
                        mqttCtrl->state = mqttState_connected;
                        break;
                    default:
                        break;
                }
            }
            else
                mqttCtrl->state = mqttState_closed;
        }
    }
    return mqttCtrl->state;
}


/**
 *  @brief Disconnect and close a connection to a MQTT server
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  @param resetModem [in] True if modem should be reset prior to reestablishing MQTT connection.
*/
uint16_t mqtt_getLastSentMsgId(mqttCtrl_t *mqttCtrl)
{}


/**
 *  @brief Disconnect and close a connection to a MQTT server
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  @param topicBffr [in] pointer to application location where topic of the received message should be copied.
 *  @param bffrSz [in] The size of the buffer.
 *  @return False if message transfer is complete, true if there is additional message content to transfer.
*/
resultCode_t mqtt_readTopic(mqttCtrl_t *mqttCtrl, char *topicBffr, uint16_t bffrSz)
{
    cBuffer_t *rxBffr = g_lqLTEM.iop->rxBffr;

    /* find complete topic; look for <topic>","<message> sequence
     */
    int16_t topicBeg = cbffr_find(rxBffr, ",\"", 0, 0, false) + 2;           // start of topic (cbffr index)
    int16_t topicEnd;

    do
    {
        topicEnd = cbffr_find(rxBffr, "\",\"", 0, 0, false);                 // wait for end of topic    
    } while (topicEnd == CBFFR_NOFIND);

    if (bffrSz == 0)                                                        // flush/discard requested
    {
        cbffr_skip(rxBffr, topicEnd - topicBeg);                            // cbffr->tail pointing at message now
        mqttCtrl->recvState = mqttRecvState_topicDelivered;
        return resultCode__success;
    }
    else if (topicEnd - topicBeg > bffrSz)                                  // can't service: insufficient buffer
    {
        return resultCode__badRequest;
    }
    else                                                                    // return topic to caller
    {
        uint16_t popSz = MIN(topicEnd - topicBeg, bffrSz);
        cbffr_pop(rxBffr, topicBffr, popSz);
        mqttCtrl->recvState = mqttRecvState_topicDelivered;
    }
    return resultCode__success;
}


/**
 *  @brief Disconnect and close a connection to a MQTT server
*/
bool mqtt_readMessage(mqttCtrl_t *mqttCtrl, char *messageBffr, uint16_t bffrSz)
{
    cBuffer_t *rxBffr = g_lqLTEM.iop->rxBffr;

    if (mqttCtrl->recvState < mqttRecvState_topicDelivered)                 // host application skipped topic read
        mqtt_readTopic(mqttCtrl, NULL, 0);

    // msgBeg is 0, cbffr.tail points at start (from readTopic)
    int16_t msgEnd;

    msgEnd = cbffr_find(rxBffr, "\"\r\n", 0, 0, false);                     // check for end of message

    if (msgEnd == CBFFR_NOFIND)
    {
        uint16_t popCnt = cbffr_pop(rxBffr, messageBffr, bffrSz);
        mqttCtrl->recvState < mqttRecvState_msgUnderway;
        return false;
    }


    uint16_t popSz = MIN(msgEnd, bffrSz);                                   
    cbffr_pop(rxBffr, messageBffr, popSz);

    if (popSz <= bffrSz)
    {
        g_lqLTEM.urcPending = NO_URC;
        mqttCtrl->recvState < mqttRecvState_none;
        return true;
    }
}


/**
 *  @brief Close out and discard a message receive underway. This clears message state. 
*/
void mqtt_recvServiced(mqttCtrl_t *mqttCtrl)
{
    S__mqttFlushPendingRecv(mqttCtrl);
}


#pragma endregion   // public API


/* private mqtt functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions


// static void S__updateSubscriptionsTable(mqttCtrl_t *mqttCtrl, bool addSubscription, const char *topic)
// {
//     uint8_t topicSz = strlen(topic);
//     bool wildcard = *(topic + topicSz - 1) == '#';      // test for MQTT multilevel wildcard, store separately for future topic parsing on recv

//     char topicEntryName[mqtt__topic_nameSz] = {0};
//     memcpy(topicEntryName, topic, wildcard ? topicSz - 1 : topicSz);

//     for (size_t i = 0; i < mqtt__topic_subscriptionCnt; i++)
//     {
//         if (strcmp(mqttCtrl->topicSubs[i].topicName, topicEntryName) == 0)
//         {
//             if (!addSubscription)
//                 mqttCtrl->topicSubs[i].topicName[0] = '\0';
//             return;
//         }
//     }

//     if (addSubscription)
//     {
//         for (size_t i = 0; i < sizeof(mqttCtrl->topicSubs); i++)
//         {
//             if (mqttCtrl->topicSubs[i].topicName[0] == 0)
//             {
//                 strncpy(mqttCtrl->topicSubs[i].topicName, topicEntryName, strlen(topicEntryName)+1);
//                 if (wildcard)
//                     mqttCtrl->topicSubs[i].wildcard = '#';
//                 return;
//             }
//         }
//     }
//     ASSERT(false);                                              // if got here subscription failed and appl is likely unstable
// }

/*
   +QMTRECV: <tcpconnectID>,<msgID>,"<topic>","<payload>"
   +QMTRECV: 5,65535,"<topic>","<payload>"
   +QMTSTAT: <tcpconnectID>,<err_code>
*/

static void S__mqttUrcHandler()
{
    cBuffer_t *rxBffr = g_lqLTEM.iop->rxBffr;                       // for convenience
    // need non-wrapped buffers for handoff to app
    // FUTURE: implement block transfer to app allowing for direct transfer from cBffr or app requesting a smaller transfer buffer
    char topicBffr[mqtt__topicSz] = {0};                            // local buffer to parse header and topic of rx message (topics can be quite large if props encoded)
    char msgBffr[mqtt__messageSz] = {0};

    if (cbffr_find(rxBffr, "+QMT", 0, 0, false) == CBFFR_NOFIND ||  // not a MQTT URC
        cbffr_peek(rxBffr, topicBffr, 20) < 20)                     //  or not sufficient chars to parse URC header
    {
        return;                                                     
    }

    bool commit = false;                                            // cbffr transaction (rollback or commit tail changes)
    char *nextPtr;
    uint8_t dataCntxt;

    cbffr_startTransaction(rxBffr);                             // start bffr transaction in case only part of the URC has arrived

    dataCntxt = strtol(topicBffr + 9, nextPtr, 10);
    ASSERT(memcmp(g_lqLTEM.streams[dataCntxt].streamType, STREAM_MQTT, 4) == 0);

    streamCtrl_t stream = g_lqLTEM.streams[dataCntxt];          // convenience
    mqttCtrl_t *mqtt = (mqttCtrl_t*)g_lqLTEM.streams[dataCntxt].pCtrl;


    /* MQTT Receive Message
        * ------------------------------------------------------------------------------------- */
    if (cbffr_find(rxBffr, "+QMTRECV:", 0, 0, true) >= 0)       // if recv, move tail to start of header
    {
        g_lqLTEM.urcPending = 'M';                              // let global URC process know: working through a MQTT URC
        nextPtr++;                                              // past , delim
        uint16_t msgId = strtol(nextPtr, NULL, 10);

        ((mqttRecv_func)stream.recvDataCB)(dataCntxt, msgId);   // signal new receive data available to host application
    }

    /* MQTT Status Change
        * ------------------------------------------------------------------------------------- */
    else if (cbffr_find(rxBffr, "+QMTSTAT", 0, 0, true) >= 0)        // MQTT connection closed
    {
        cbffr_commitTransaction(rxBffr);                        // commit buffer changes (consumed QMTSTAT)
        mqtt->state = mqttState_closed;
    }
}


void S__mqttResetContext(dataCntxt_t dataCntxt)
{
    mqttCtrl_t *mqtt = (mqttCtrl_t*)g_lqLTEM.streams[dataCntxt].pCtrl;
    S__mqttFlushPendingRecv(dataCntxt);
    mqtt->state = mqttState_closed;
}


void S__mqttFlushPendingRecv(dataCntxt_t dataCntxt)
{
    mqttCtrl_t *mqtt = (mqttCtrl_t*)g_lqLTEM.streams[dataCntxt].pCtrl;

    if (mqtt->recvState > mqttRecvState_none)
    {
        //flush cbffr of pending receive parts
        if (mqtt->recvState < mqttRecvState_topicDelivered)     // flush topic
        {
            mqtt_readTopic(mqtt, NULL, 0);                      // signal discard
        }
        if (mqtt->recvState < mqttRecvState_msgUnderway)        // flush message
        {
            mqtt_readMessage(mqtt, NULL, 0);                    // signal discard
        }
    }
    mqtt->recvState = mqttRecvState_none;
}


#pragma endregion


/* MQTT ATCMD Parsers
 * --------------------------------------------------------------------------------------------- */
#pragma region MQTT ATCMD Parsers
// /**
//  *	\brief [private] MQTT open status response parser.
//  *
//  *  \param response [in] Character data recv'd from BGx to parse for task complete
//  *  \param endptr [out] Char pointer to the char following parsed text
//  * 
//  *  \return HTTP style result code, 0 = not complete
//  */
// static cmdParseRslt_t S__mqttOpenStatusParser() 
// {
//     cmdParseRslt_t parserRslt = atcmd_stdResponseParser("+QMTOPEN: ", false, ",", 0, 3, "OK\r\n", 0);

//     // cmdParseRslt_t parserRslt = atcmd_stdResponseParser("+QMTOPEN: ", true, NULL, 1, 1, NULL, 0);
//     // resultCode_t parserResult = atcmd_serviceResponseParserTerm(response, "+QMTOPEN: ", 0, "\r\n", endptr);

//     // if (parserResult > resultCode__successMax && strstr(response, "OK\r\n"))                        // if no QMTOPEN and OK: not connected
//     //     return resultCode__notFound;

//     return parserRslt;
// }


/**
 *	\brief [private] MQTT open connection response parser.
 *  \return LTEmC parse result
 */
static cmdParseRslt_t S__mqttOpenCompleteParser() 
{
    cmdParseRslt_t parserRslt = atcmd_stdResponseParser("+QMTOPEN: ", true, ",", 0, 2, "", 0);
    return parserRslt;
}


/**
 *	\brief [private] MQTT connect to server response parser.
 *  \return LTEmC parse result
 */
static cmdParseRslt_t S__mqttConnectCompleteParser() 
{
    // return ATCMD_testResponseTrace();
    return atcmd_stdResponseParser("+QMTCONN: ", true, ",", 0, 3, "\r\n", 0);
}


/**
 *	\brief [private] MQTT connect to server response parser.
 *  \return LTEmC parse result
 */
static cmdParseRslt_t S__mqttConnectStatusParser() 
{
    // BGx +QMTCONN Returns status: 1 = connecting, 3 = connected. Service parser returns 200 + status.
    // A simple "OK" response indicates no connection
    return atcmd_stdResponseParser("+QMTCONN: ", false, ",", 0, 2, "OK\r\n", 0);
}


/**
 *	\brief [private] MQTT subscribe to topic response parser.
 *  \return LTEmC parse result
 */
static cmdParseRslt_t S__mqttSubscribeCompleteParser() 
{
    return atcmd_stdResponseParser("+QMTSUB: ", true, ",", 0, 4, "\r\n", 0);
}


/**
 *	\brief [private] MQTT publish message to topic response parser.
 *  \return LTEmC parse result
 */
static cmdParseRslt_t S__mqttPublishCompleteParser() 
{
    return atcmd_stdResponseParser("+QMTPUB: ", true, ",", 0, 2, "", 0);
}

/**
 *	\brief [private] MQTT publish message to topic response parser.
 *  \return LTEmC parse result
 */
static cmdParseRslt_t S__mqttCloseCompleteParser() 
{
    return atcmd_stdResponseParser("OK\r\n\r\n+QMTCLOSE: ", true, ",", 0, 2, "", 0);
}


#pragma endregion

