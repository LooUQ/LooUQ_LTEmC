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

#include "ltemc-mqtt.h"
#include <lq-str.h>

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define ASCII_CtrlZ_STR "\x1A"
#define ASCII_ESC_STR "\x1B"
#define ASCII_DblQuote_CHAR '"'

// #define MQTT_ACTION_CMD_SZ 81
// #define MQTT_CONNECT_CMD_SZ 300

extern ltemDevice_t g_lqLTEM;

enum
{
    resultCode__parserPending = 0xFFFF
};

/* Local Function Declarations
 ----------------------------------------------------------------------------------------------- */

static void S_updateSubscriptionsTable(mqttCtrl_t *mqttCtrl, bool addSubscription, const char *topic);
static void S_mqttDoWork();
static void S_urlDecode(char *src, int len);

static cmdParseRslt_t S_mqttOpenStatusParser(const char *response, char **endptr);
static cmdParseRslt_t S_mqttOpenCompleteParser(const char *response, char **endptr);
static cmdParseRslt_t S_mqttConnectStatusParser(const char *response, char **endptr);
static cmdParseRslt_t S_mqttConnectCompleteParser(const char *response, char **endptr);
static cmdParseRslt_t S_mqttSubscribeCompleteParser(const char *response, char **endptr);
static cmdParseRslt_t S_mqttPublishCompleteParser(const char *response, char **endptr);


/* public mqtt functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions


/**
 *  @brief Initialize a MQTT protocol control structure.
*/
void mqtt_initControl(mqttCtrl_t *mqttCtrl, socket_t scktNm, uint8_t *recvBuf, uint16_t recvBufSz, mqttRecvFunc_t recvCallback)
{
    ASSERT(mqttCtrl != NULL && recvBuf != NULL, srcfile_ltemc_mqtt_c);
    ASSERT(scktNm < socket__cnt, srcfile_ltemc_mqtt_c);

    memset(mqttCtrl, 0, sizeof(mqttCtrl_t));

    mqttCtrl->ctrlMagic = streams__ctrlMagic;
    mqttCtrl->scktNm = scktNm;
    mqttCtrl->protocol = protocol_mqtt;

    uint16_t bufferSz = IOP_initRxBufferCtrl(&(mqttCtrl->recvBufCtrl), recvBuf, recvBufSz);

    ASSERT_W(recvBufSz == bufferSz, srcfile_ltemc_mqtt_c, "RxBufSz != multiple of 128B");
    ASSERT(bufferSz > 64, srcfile_ltemc_mqtt_c);

    mqttCtrl->dataRecvCB = recvCallback;
    mqttCtrl->msgId = 1;
}

/**
 *  @brief Set the remote server connection values.
*/
void mqtt_setConnection(mqttCtrl_t *mqttCtrl, const char *hostUrl, uint16_t port, bool useTls, mqttVersion_t useMqttVersion, const char *deviceId, const char *userId, const char *secret)
{
    ASSERT(mqttCtrl != NULL, srcfile_ltemc_mqtt_c);

    mqttCtrl->useTls = useTls;
    mqttCtrl->useMqttVersion = useMqttVersion;

}


/**
 *  @brief Open a remote MQTT server for use.
*/
resultCode_t mqtt_open(mqttCtrl_t *mqttCtrl)
{
    // AT+QSSLCFG="sslversion",5,3
    // AT+QMTCFG="ssl",5,1,0
    // AT+QMTCFG="version",5,4
    // AT+QMTOPEN=5,"iothub-dev-pelogical.azure-devices.net",8883

    atcmdResult_t atResult;
    mqttCtrl->state = mqtt_getStatus(mqttCtrl);                 // get MQTT state, state must be not open for config changes

    if (mqttCtrl->state >= mqttState_open)                      // already open or connected
    {
        if (!mqttCtrl->canReuseConn)
            return resultCode__preConditionFailed;

        mqttCtrl->connReused = true;
        return resultCode__success;
    }

    // not recycling existing connection, doing the work to open new connection here 
    if (mqttCtrl->useTls)
    {
        if (atcmd_tryInvoke("AT+QMTCFG=\"ssl\",%d,1,%d", mqttCtrl->scktNm, mqttCtrl->scktNm))
        {
            if (atcmd_awaitResult() != resultCode__success)
                return resultCode__internalError;
        }
    }
    if (mqttCtrl->useMqttVersion == mqttVersion_311)
    {
        if (atcmd_tryInvoke("AT+QMTCFG=\"version\",%d,4", mqttCtrl->scktNm))
        {
            if (atcmd_awaitResult() != resultCode__success)
                return resultCode__internalError;
        }
    }

    // TYPICAL: AT+QMTOPEN=0,"iothub-dev-pelogical.azure-devices.net",8883
    atcmd_setOptions(PERIOD_FROM_SECONDS(45), S_mqttOpenCompleteParser);
    if (atcmd_tryInvokeWithOptions("AT+QMTOPEN=%d,\"%s\",%d", mqttCtrl->scktNm, mqttCtrl->hostUrl, mqttCtrl->hostPort))
    {
        resultCode_t atResult = atcmd_awaitResult();
        if (atResult >= resultCode__success && atResult <= resultCode__successMax)      // opened mqtt server
        {
            ((iop_t*)g_lqLTEM.iop)->mqttMap &= 0x01 & mqttCtrl->scktNm;
            mqttCtrl->state = mqttState_open;
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
 *  @brief Disconnect and close a connection to a MQTT server
*/
void mqtt_close(mqttCtrl_t *mqttCtrl)
{
    mqttCtrl->state = mqttState_closed;
    ((iop_t*)g_lqLTEM.iop)->mqttMap &= ~(0x01 & mqttCtrl->scktNm);
    ((iop_t*)g_lqLTEM.iop)->streamPeers[mqttCtrl->scktNm] = NULL;

    for (size_t i = 0; i < sizeof(mqttCtrl->topicSubs); i++)          // clear topicSubs table
    {
        mqttCtrl->topicSubs[i].topicName[0] = 0;
    }

    // if (mqtt->state == mqttState_connected)
    // {
    //     if (atcmd_tryInvoke("AT+QMTDISC=%d", mqtt->sckt))
    //         atcmd_awaitResult(true);
    // }
    if (mqttCtrl->state >= mqttState_open)
    {
        if (atcmd_tryInvoke("AT+QMTCLOSE=%d", mqttCtrl->scktNm))
            atcmd_awaitResult();
    }
    mqttCtrl->state == mqttState_closed;
    mqttCtrl->connReused = false;
    return resultCode__success;
}


/**
 *  @brief Connect (authenticate) to a MQTT server.
 * 
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  @param clientId [in] - The client or device identifier for the connection.
 *  @param username [in] - The user identifier or name for the connection to authenticate.
 *  @param password [in] - The secret string or phrase to authenticate the connection.
 *  @param cleanSession [in] - Directs MQTT to preserve or flush messages received prior to the session start.
 * 
 *  @return A resultCode_t value indicating the success or type of failure, OK = 200.
*/
resultCode_t mqtt_connect(mqttCtrl_t *mqttCtrl, mqttSession_t cleanSession)
{
    resultCode_t atResult;
    mqttCtrl->state = mqtt_getStatus(mqttCtrl);                                 // get MQTT state, state must be not open for config changes

    if (mqttCtrl->state == mqttState_connected)
    {
        if (!mqttCtrl->canReuseConn)
            return resultCode__preConditionFailed;

        mqttCtrl->connReused = true;
        return resultCode__success;
    }

    if (atcmd_tryInvoke("AT+QMTCFG=\"session\",%d,%d", mqttCtrl->scktNm, (uint8_t)cleanSession))
    {
        if (atcmd_awaitResult() != resultCode__success)
            return resultCode__internalError;
    }

    atcmd_setOptions(PERIOD_FROM_SECONDS(60), S_mqttConnectCompleteParser);

    /* MQTT connect command can be quite large, using local buffer here rather than bloat global cmd\core buffer */
    char connectCmd[384] = {0};
    snprintf(connectCmd, sizeof(connectCmd), "AT+QMTCONN=%d,\"%s\",\"%s\",\"%s\"", mqttCtrl->scktNm, mqttCtrl->clientId, mqttCtrl->username, mqttCtrl->secret);

    if (ATCMD_awaitLock(atcmd__defaultTimeoutMS))                  // to use oversized buffer need sendCmdData(), which doesn't acquire lock
    {
        atcmd_sendCmdData(connectCmd, strlen(connectCmd), "\r");
        atResult = atcmd_awaitResult();                             // in autolock mode, so this will release lock

        if (atResult == resultCode__success)
        {
            mqttCtrl->state = mqttState_connected;
            return resultCode__success;
        }
        else if (atResult == resultCode__timeout)                        // assume got a +QMTSTAT back not +QMTCONN
        {
            char *continuePtr = strstr(atcmd_getLastResponse(), "+QMTSTAT: ");
            if (continuePtr != NULL)
            {
                continuePtr += 12;
                atResult = atol(continuePtr) + 200;
            }
        }
        else
        {
            switch (atResult)
            {
                case 201:
                    return resultCode__methodNotAllowed;        // invalid protocol version 
                case 202:               
                case 204:
                    return resultCode__unauthorized;            // bad user ID or password
                case 203:
                    return resultCode__unavailable;             // server unavailable
                case 205:
                    return resultCode__forbidden;               // refused, not authorized
                default:
                    return resultCode__internalError;
            }
        }
    }
    return resultCode__badRequest;          // bad parameters assumed
}


/**
 *  @brief Subscribe to a topic on the MQTT server.
 * 
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  @param topic [in] - The messaging topic to subscribe to.
 *  @param qos [in] - The MQTT QOS level for messages subscribed to.
 * 
 *  @return The topic index handle. 0xFF indicates the topic subscription was unsuccessful
 */
resultCode_t mqtt_subscribe(mqttCtrl_t *mqttCtrl, const char *topic, mqttQos_t qos)
{
    ASSERT(strlen(topic) < mqtt__topic_nameSz, srcfile_ltemc_mqtt_c);

    resultCode_t atResult = 400;

    // BGx doesn't provide a test for an existing subscription, but is tolerant of a duplicate create subscription 
    // if sucessful, the topic's subscription will overwrite the IOP peer map without issue as well (same bitmap value)

    // snprintf(actionCmd, MQTT_ACTION_CMD_SZ, "AT+QMTSUB=%d,%d,\"%s\",%d", mqtt->sckt, ++mqtt->msgId, topic, qos);
    atcmd_setOptions(PERIOD_FROM_SECONDS(30), S_mqttSubscribeCompleteParser);
    if (atcmd_tryInvokeWithOptions("AT+QMTSUB=%d,%d,\"%s\",%d", mqttCtrl->scktNm, ++mqttCtrl->msgId, topic, qos))
    {
        atResult = atcmd_awaitResult();
        if (atResult == resultCode__success)
        {
            S_updateSubscriptionsTable(mqttCtrl, true, topic);

            ((iop_t*)g_lqLTEM.iop)->mqttMap |= 0x01 << mqttCtrl->scktNm;
            ((iop_t*)g_lqLTEM.iop)->streamPeers[mqttCtrl->scktNm] = mqttCtrl;

            LTEM_registerDoWorker(S_mqttDoWork);                                // need to register worker if 1 or more subscriptions
            return atResult;
        }
    }
    return atResult;
}


/**
 *  @brief Unsubscribe to a topic on the MQTT server.
 * 
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  @param topic [in] - The messaging topic to unsubscribe from.
 * 
 *  @return A resultCode_t (http status type) value indicating the success or type of failure, OK = 200.
*/
resultCode_t mqtt_unsubscribe(mqttCtrl_t *mqttCtrl, const char *topic)
{
    // snprintf(actionCmd, MQTT_CONNECT_CMD_SZ, "AT+QMTUNS=%d,%d,\"<topic1>\"", mqtt->sckt, ++mqtt->msgId, topic);
    if (atcmd_tryInvoke("AT+QMTUNS=%d,%d,\"%s\"", mqttCtrl->scktNm, ++mqttCtrl->msgId, topic))
    {
        if (atcmd_awaitResult() == resultCode__success)
        {
            S_updateSubscriptionsTable(mqttCtrl, false, topic);

            ((iop_t*)g_lqLTEM.iop)->mqttMap &= ~(0x01 << mqttCtrl->scktNm);
            ((iop_t*)g_lqLTEM.iop)->streamPeers[mqttCtrl->scktNm] = NULL;


            // // unregister worker if this is the last subscription being "unsubscribed"
            // if (((iop_t*)g_ltem.iop)->mqttMap == 0)
            //     LTEM_unregisterDoWorker(); 
            return resultCode__success;
        }
    }
    return resultCode__badRequest;
}


/**
 *  @brief Publish a message to server.
 * 
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  @param topic [in] - Pointer to the message topic (see your server for topic formatting details).
 *  @param qos [in] - The MQTT QOS to be assigned to sent message.
 *  @param message [in] - Pointer to message to be sent.
 *  @param timeoutSeconds [in] - Seconds to wait for publish to complete, highly dependent on network and remote server.
 * 
 *  @return A resultCode_t value indicating the success or type of failure (http status type code).
*/
resultCode_t mqtt_publish(mqttCtrl_t *mqttCtrl, const char *topic, mqttQos_t qos, const char *message, uint8_t timeoutSeconds)
{
    uint8_t pubstate = 0;
    uint32_t timeoutMS = (timeoutSeconds == 0) ? mqtt__publishTimeout : timeoutSeconds * 1000;

    // AT+QMTPUB=<tcpconnectID>,<msgID>,<qos>,<retain>,"<topic>"
    char msgText[mqtt__messageSz];
    resultCode_t atResult = resultCode__conflict;               // assume lock not obtainable, conflict

    atcmd_setOptions(PERIOD_FROM_SECONDS(timeoutSeconds), ATCMD_txDataPromptParser);
    if (ATCMD_awaitLock(timeoutMS))
    {
        uint16_t msgId = ((uint8_t)qos == 0) ? 0 : mqttCtrl->msgId++;
        atcmd_invokeReuseLock("AT+QMTPUB=%d,%d,%d,0,\"%s\"", mqttCtrl->scktNm, msgId, qos, topic);
        pubstate++;

        atResult = atcmd_awaitResult();
        if (atResult == resultCode__success)                        // wait for data prompt for data, now complete sub-command to actually transfer data
        {
            pubstate++;
            atcmd_setOptions(timeoutMS, S_mqttPublishCompleteParser);
            atcmd_sendCmdData(message, strlen(message), ASCII_CtrlZ_STR);
            atResult = atcmd_awaitResult();
            if (atResult == resultCode__success)
            {
                atcmd_close();
                return resultCode__success;
            }
        }
    }
    atcmd_exitTextMode();                                           // if any problem, make sure BGx is out of text mode
    atcmd_close();
    PRINTF(dbgColor__dYellow, "MQTT-PUB ERROR: state=%d, rslt=%d\r", pubstate, atResult);
    return atResult;
}


/**
 *  @brief Query the status of the MQTT server state.
 * 
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  @param host [in] A char string to match with the currently connected server. Host is not checked if empty string passed.
 * 
 *  @return A mqttState_t value indicating the state of the MQTT connection.
*/
mqttState_t mqtt_getStatus(mqttCtrl_t *mqttCtrl)
{
    // See BG96_MQTT_Application_Note: AT+QMTOPEN? and AT+QMTCONN?

    ASSERT(mqttCtrl->ctrlMagic == streams__ctrlMagic, srcfile_ltemc_mqtt_c);      // check for bad pointer into MQTT control
    ASSERT(mqttCtrl->scktNm < socket__cnt, srcfile_ltemc_mqtt_c);

    uint8_t mqttState = mqtt_getStatusForSocket(mqttCtrl->scktNm);

    if (!((iop_t*)g_lqLTEM.iop)->mqttMap & 0x01 << mqttCtrl->scktNm && mqttState == mqttState_connected)
    {
        ((iop_t*)g_lqLTEM.iop)->mqttMap |= 0x01 << mqttCtrl->scktNm;       // fixup IOP mqtt active mask
    }

    if (mqttState == mqttState_open || mqttState == mqttState_connected)
    {
        mqttCtrl->state = mqttState;
    }
    else if (mqttState == mqttState_pending || mqttState == mqttState_invalidHost)
    {
        // close MQTT on BGx, protocol not functional as is
        mqtt_close(mqttCtrl);
    }
    else
        mqttCtrl->state = mqttState_closed;

    return mqttCtrl->state;
}


/**
 *  @brief Query the status of the MQTT protocol state for a data context and optionally validate host.
 * 
 *  @param cntxt [in] Data context identifier (0-5).
 *  @param host [in] A char string to match with the currently connected server. Host is not checked if empty string passed.
 * 
 *  @return A mqttState_t value indicating the state of the MQTT connection.
*/
mqttState_t mqtt_getStatusForSocket(socket_t sckt)
{
    if (ltem_readDeviceState() && sckt < socket__cnt)
    {
        atcmd_setOptions(PERIOD_FROM_SECONDS(5), S_mqttConnectStatusParser);
        if (atcmd_tryInvokeWithOptions("AT+QMTCONN?"))
        {
            resultCode_t atResult = atcmd_awaitResult();
            if (atResult == 203)
                return mqttState_connected;
            else if (atResult == 202)
                return mqttState_open;
        }
    }
    //     if (statusResult == 0 || *host != '\0')
    //     {
    //         atcmd_setOptions(PERIOD_FROM_SECONDS(5), S_mqttOpenStatusParser);
    //         if (atcmd_tryInvokeWithOptions("AT+QMTOPEN?"))
    //         {
    //             resultCode_t atResult = atcmd_awaitResult();
    //             if (atResult == resultCode__success)
    //             {
    //                 if (*host != '\0')                                  // host matching requested, check modem response host == requested host
    //                 {
    //                     char *hostNamePtr = strchr(ATCMD_getLastResponse(), '"');
    //                     if (hostNamePtr == NULL && strncmp(hostNamePtr + 1, host, strlen(host)) != 0)
    //                         return mqttState_invalidHost;
    //                 }
    //                 ((iop_t*)g_ltem.iop)->mqttMap |= 0x01 << cntxt;
    //                 statusResult = mqttState_open;
    //             }
    //             if (atResult == 204)
    //                 statusResult = mqttState_invalidHost;
    //         }
    //     }
    // }
    return mqttState_closed;
}

/**
 *  @brief Fetch the MQTT connections from the BGx.
 * 
 * TO BE IMPLEMENTED
 * 
 *  @return A bitmap indicating the data contexts with an MQTT connection.
*/
uint8_t mqtt_fetchContextMap()
{
    return 0;
}


uint16_t mqtt_getMsgId(mqttCtrl_t *mqttCtrl)
{
    return mqttCtrl->msgId;
}


uint16_t mqtt_getLastBufferReqd(mqttCtrl_t *mqttCtrl)
{
    return mqttCtrl->lastBufferReqd;
}


#pragma endregion

/* private mqtt functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions


static void S_updateSubscriptionsTable(mqttCtrl_t *mqttCtrl, bool addSubscription, const char *topic)
{
    uint8_t topicSz = strlen(topic);
    bool wildcard = *(topic + topicSz - 1) == '#';      // test for MQTT multilevel wildcard, store separately for future topic parsing on recv

    char topicEntryName[mqtt__topic_nameSz] = {0};
    memcpy(topicEntryName, topic, wildcard ? topicSz - 1 : topicSz);

    for (size_t i = 0; i < mqtt__topic_subscriptionCnt; i++)
    {
        if (strcmp(mqttCtrl->topicSubs[i].topicName, topicEntryName) == 0)
        {
            if (!addSubscription)
                mqttCtrl->topicSubs[i].topicName[0] = '\0';
            return;
        }
    }

    if (addSubscription)
    {
        for (size_t i = 0; i < sizeof(mqttCtrl->topicSubs); i++)
        {
            if (mqttCtrl->topicSubs[i].topicName[0] == 0)
            {
                strncpy(mqttCtrl->topicSubs[i].topicName, topicEntryName, strlen(topicEntryName)+1);
                if (wildcard)
                    mqttCtrl->topicSubs[i].wildcard = '#';
                return;
            }
        }
    }
    ASSERT(false, srcfile_ltemc_mqtt_c);                                              // if got here subscription failed and appl is likely unstable
}


/**
 *  @brief Performs background tasks to advance MQTT pipeline dataflows.
*/
static void S_mqttDoWork()
{
    iop_t *iopPtr = (iop_t*)g_lqLTEM.iop;

    if (iopPtr->rxStreamCtrl != NULL && ((baseCtrl_t*)iopPtr->rxStreamCtrl)->protocol == protocol_mqtt)
    {
        /* parse received MQTT message into topic, wildcard (topic vars) and message
         * EXAMPLE: +QMTRECV: 0,0, "topic/wildcard","This is the payload related to topic" */

        char *topicPtr;
        char *topicVarPtr;
        char *messagePtr;
        uint16_t messageSz = 0;

        // readability variables
        mqttCtrl_t *mqttPtr = ((mqttCtrl_t*)iopPtr->rxStreamCtrl);
        rxDataBufferCtrl_t *rxBufPtr = &mqttPtr->recvBufCtrl;

        /* Check for message complete in IOP page, if not record progress for timeout detection and exit 
         * BGx is sloppy on MQTT end-of-message, look for dblQuote + line end ("+CR+LF) */
        char *trailerPtr = lq_strnstr(rxBufPtr->pages[rxBufPtr->iopPg].head - 8, "\"\r\n", 8);
        if (trailerPtr == NULL)
        {
            uint32_t idleTime = IOP_getRxIdleDuration();
            if (idleTime > IOP__rxDefaultTimeout)
            {
                ltem_notifyApp(appEvent_prto_recvFault, "MQTT message recv timeout");
                IOP_resetRxDataBufferPage(rxBufPtr, rxBufPtr->iopPg);                            // clear partial page recv content
                iopPtr->rxStreamCtrl = NULL;                                                     // exit data mode
            }
            return;
        }

        // iopPg has trailer text, swap in new page and process (parse) the current iopPg
        mqttPtr->lastBufferReqd = rxPageDataAvailable(rxBufPtr, rxBufPtr->iopPg);
        IOP_swapRxBufferPage(rxBufPtr);
        uint8_t thisPage = !rxBufPtr->iopPg;
        
        // uint16_t newLen = lq_strUrlDecode(dBufPtr->pages[thisPage]._buffer, mqttPtr->lastBufferReqd);   // un-escape the topic and message recv'd
        // dBufPtr->pages[thisPage].head += newLen - mqttPtr->lastBufferReqd;                      // shorten to UrlDecode adjusted
        // trailerPtr = lq_strnstr(dBufPtr->pages[thisPage].head - 8, "\"\r\n\r\n", 8);            // re-establish trailer pointer

        // ASSERT test for buffer overflow, good buffer has recv header and trailer in same page
        char *continuePtr = lq_strnstr(rxBufPtr->pages[thisPage]._buffer, "+QMTRECV: ", 12);     // allow for leading /r/n
        ASSERT(continuePtr != NULL, srcfile_ltemc_mqtt_c);

        uint16_t msgId = atol(continuePtr + 12);
        topicPtr = memchr(rxBufPtr->pages[thisPage].tail, '"', mqtt__topic_offset);
        if (topicPtr == NULL)                                                                   // malformed
        {
            IOP_resetRxDataBufferPage(rxBufPtr, thisPage);                                      // done with this page, clear it
            iopPtr->rxStreamCtrl = NULL;                                                        // exit data mode
            return;
        }                                                             
        rxBufPtr->pages[thisPage].tail = ++topicPtr;                                            // point past opening dblQuote
        rxBufPtr->pages[thisPage].tail = memchr(rxBufPtr->pages[thisPage].tail, '"', rxPageDataAvailable(rxBufPtr,thisPage));
        if (rxBufPtr->pages[thisPage].tail == NULL)                                             // malformed, overflowed somehow
        {
            IOP_resetRxDataBufferPage(rxBufPtr, thisPage);                                      // done with this page, clear it
            iopPtr->rxStreamCtrl = NULL;                                                        // exit data mode
            return;
        }

        messagePtr = rxBufPtr->pages[thisPage].tail += 3;                                       // set the message start
        messageSz = trailerPtr - messagePtr;
        *trailerPtr = '\0';                                                                     // null term the message (remove BGx trailing "\r\n)

        /* find topic in subscriptions array & invoke application receiver */
        for (size_t i = 0; i < mqtt__topic_subscriptionCnt; i++)
        {
            uint16_t topicSz = strlen(mqttPtr->topicSubs[i].topicName);
            if (topicSz > 0 && strncmp(mqttPtr->topicSubs[i].topicName, topicPtr, topicSz) == 0)
            {
                if (topicPtr + topicSz + 3 < messagePtr)                                        // test for topic wildcard
                {
                    topicVarPtr = topicPtr + topicSz;                                           // props or other variable data at end of topic (null-terminated)                    
                    *(topicVarPtr - 1) = '\0';                                                  // null-term topic : overwrite \ separating topic and wildcard (topic vars)
                }
                else
                {
                    topicVarPtr = NULL;                                                         // no topic wildcard\variables
                }
                *(messagePtr - 3) = '\0';                                                       // null-term topicVars
                mqttPtr->dataRecvCB(mqttPtr->scktNm, msgId, topicPtr, topicVarPtr, messagePtr, messageSz);
                IOP_resetRxDataBufferPage(rxBufPtr, thisPage);                                      // done with this page, clear it
                iopPtr->rxStreamCtrl = NULL;                                                        // exit data mode
                return;
            }
        }
        
        // finally:
        //     IOP_resetRxDataBufferPage(rxBufPtr, thisPage);                                      // done with this page, clear it
        //     iopPtr->rxStreamCtrl = NULL;                                                        // exit data mode
    }
}




/**
 *	@brief [private] MQTT open status response parser.
 *
 *  @param response [in] Character data recv'd from BGx to parse for task complete
 *  @param endptr [out] Char pointer to the char following parsed text
 * 
 *  @return HTTP style result code, 0 = not complete
 */
static cmdParseRslt_t S_mqttOpenStatusParser(const char *response, char **endptr) 
{
    resultCode_t parserResult = atcmd_serviceResponseParserTerm(response, "+QMTOPEN: ", 0, "\r\n", endptr);

    if (parserResult > resultCode__successMax && strstr(response, "OK\r\n"))                        // if no QMTOPEN and OK: not connected
        return resultCode__notFound;

    return parserResult;
}


/**
 *	@brief [private] MQTT open connection response parser.
 *
 *  @param response [in] Character data recv'd from BGx to parse for task complete
 *  @param endptr [out] Char pointer to the char following parsed text
 * 
 *  @return HTTP style result code or resultCode_, 0 = not complete
 */
static cmdParseRslt_t S_mqttOpenCompleteParser(const char *response, char **endptr) 
{
    cmdParseRslt_t parserRslt = atcmd__stdResponseParser("+QMTOPEN: ", true, NULL, 1, 1, NULL, 0);
    return parserRslt;
}


/**
 *	@brief [private] MQTT connect to server response parser.
 *
 *  @param response [in] Character data recv'd from BGx to parse for task complete
 *  @param endptr [out] Char pointer to the char following parsed text
 * 
 *  @return HTTP style result code, 0 = not complete
 */
static cmdParseRslt_t S_mqttConnectStatusParser(const char *response, char **endptr) 
{
    // BGx +QMTCONN Returns status: 1 = connecting, 3 = connected. Service parser returns 200 + status.
    // A simple "OK" response indicates no connection
    cmdParseRslt_t parserRslt = atcmd__stdResponseParser("+QMTCONN: ", true, NULL, 1, 1, NULL, 0);

    if (parserRslt > resultCode__successMax && strstr(response, "OK\r\n"))                        // if no QMTCONN and OK: not connection
        return resultCode__notFound;

    return parserRslt;
}


/**
 *	@brief [private] MQTT connect to server response parser.
 *
 *  @param response [in] Character data recv'd from BGx to parse for task complete
 *  @param endptr [out] Char pointer to the char following parsed text
 * 
 *  @return HTTP style result code, RESULT_CODE_PENDING = not complete
 */
static cmdParseRslt_t S_mqttConnectCompleteParser(const char *response, char **endptr) 
{
    return atcmd__stdResponseParser("+QMTCONN: ", true, ",", 2, 2, NULL, 0);
}


/**
 *	@brief [private] MQTT subscribe to topic response parser.
 *
 *  @param response [in] Character data recv'd from BGx to parse for task complete
 *  @param endptr [out] Char pointer to the char following parsed text
 * 
 *  @return HTTP style result code, RESULT_CODE_PENDING = not complete
 */
static cmdParseRslt_t S_mqttSubscribeCompleteParser(const char *response, char **endptr) 
{
    return atcmd__stdResponseParser("+QMTSUB: ", true, ",", 2, 2, NULL, 0);
}


/**
 *	@brief [private] MQTT publish message to topic response parser.
 *
 *  @param response [in] Character data recv'd from BGx to parse for task complete
 *  @param endptr [out] Char pointer to the char following parsed text
 * 
 *  @return HTTP style result code, RESULT_CODE_PENDING = not complete
 */
static cmdParseRslt_t S_mqttPublishCompleteParser(const char *response, char **endptr) 
{
    return atcmd__stdResponseParser("+QMTPUB: ", true, ",", 2, 2, NULL, 0);
}


#pragma endregion
