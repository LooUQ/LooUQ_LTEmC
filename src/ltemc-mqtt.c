/** ****************************************************************************
  \file 
  \brief Public API providing MQTT/MQTTS support
  \author Greg Terrell, LooUQ Incorporated

  \loouq

--------------------------------------------------------------------------------

    This project is released under the GPL-3.0 License.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 
***************************************************************************** */


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

static uint8_t S__findtopicIndx(mqttCtrl_t* mqttCntl, mqttTopicCtrl_t* topicCtrl);
static resultCode_t S__notifyServerTopicChange(mqttCtrl_t* mqttCtrl, uint8_t topicIndx, bool subscribe);
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
 *  @brief Initialize a MQTT protocol control structure.
*/
void mqtt_initControl(mqttCtrl_t *mqttCtrl, dataCntxt_t dataCntxt)
{
    ASSERT(dataCntxt < dataCntxt__cnt);                                 // valid streams index
    ASSERT(g_lqLTEM.streams[dataCntxt] == 0);                           // context not already in use

    memset(mqttCtrl, 0, sizeof(mqttCtrl_t));

    mqttCtrl->streamType = streamType_MQTT;
    mqttCtrl->streamUrcHndlr = S__mqttUrcHandler;                       // for MQTT, URC handler performs all necessary functions
    mqttCtrl->dataRxHndlr = NULL;                                       // marshalls data from buffer to app done by URC handler
}


/**
 *  @brief Initialize a MQTT topic subscription control structure.
*/
void mqtt_initTopicControl(mqttTopicCtrl_t* topicCtrl, const char* topic, bool wildcard, uint8_t qos, mqttAppRecv_func appTopicRecvCB)
{
    memset(topicCtrl, 0, sizeof(mqttTopicCtrl_t));
    memcpy(topicCtrl->topicName, topic, mqtt__topic_nameSz);
    topicCtrl->wildcard = wildcard ? '#' : ' ';
    topicCtrl->Qos = qos;
    topicCtrl->appRecvDataCB = appTopicRecvCB;
}


/**
 *  @brief Set the remote server connection values.
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
 *  @brief Open a remote MQTT server IP connection for use.
 *  @details Preferred way for user app to connect to server is via mqtt_start() or mqtt_reset()
*/
resultCode_t MQTT__open(mqttCtrl_t *mqttCtrl)
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
 *  @brief Connect (authenticate) to a MQTT server.
 *  @details Preferred way for user app to connect to server is via mqtt_start() or mqtt_reset()
*/
resultCode_t MQTT__connect(mqttCtrl_t *mqttCtrl, bool cleanSession)
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
        strcat(connectCmd, "\r");
        atcmd_sendCmdData(connectCmd, strlen(connectCmd));
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
 *  @brief Open and connect to a remote MQTT server.
 */
resultCode_t mqtt_start(mqttCtrl_t *mqttCtrl, bool cleanSession)
{
    resultCode_t rslt;
    do
    {
        rslt = MQTT__open(&mqttCtrl);
        if (rslt != resultCode__success)
        {
            PRINTF(dbgColor__warn, "Open fail status=%d\r", rslt);
            break;
        }

        rslt = MQTT__connect(&mqttCtrl, true);
        if (rslt != resultCode__success)
        {
            PRINTF(dbgColor__warn, "Connect fail status=%d\r", rslt);
            break;
        }

        for (size_t i = 0; i < mqtt__topicsCnt; i++)
        {
            S__notifyServerTopicChange(mqttCtrl, i, true);
        }

    } while (false);

    return rslt;
}


/**
 *  @brief Open and connect to a remote MQTT server.
 */
resultCode_t mqtt_subscribeTopic(mqttCtrl_t *mqttCtrl, mqttTopicCtrl_t* topicCtrl)
{
    uint8_t topicIndx = S__findtopicIndx(mqttCtrl, topicCtrl);
    if (topicIndx != 255)
    {
        mqttCtrl->topics[topicIndx] = topicCtrl;
    }
    else
    {
        return resultCode__preConditionFailed;
    }
    
    if (mqttCtrl->state == mqttState_connected)
    {
        S__notifyServerTopicChange(mqttCtrl, mqttCtrl->topics[topicIndx], true);
    }
    return resultCode__conflict;
}


/**
 *  @brief Unsubscribe from a topic on the MQTT server.
 */
resultCode_t mqtt_cancelTopic(mqttCtrl_t *mqttCtrl, mqttTopicCtrl_t* topicCtrl)
{
    uint8_t topicIndx = S__findtopicIndx(mqttCtrl, topicCtrl);
    if (topicIndx != 255)
    {
        mqttCtrl->topics[topicIndx] = NULL;
    }
    else
    {
        return resultCode__preConditionFailed;
    }

    if (mqttCtrl->state == mqttState_connected)
    {
        S__notifyServerTopicChange(mqttCtrl, topicIndx, false);
    }
    return resultCode__conflict;
}


// /**
//  *  @brief Publish an extended message to server.
//  *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
//  *  @param topic [in] - Pointer to the message topic (see your server for topic formatting details).
//  *  @param qos [in] - The MQTT QOS to be assigned to sent message.
//  *  @param encodedMsg [in] - Pointer to message to be sent.
//  *  @param timeoutSeconds [in] - Seconds to wait for publish to complete, highly dependent on network and remote server.
//  *  @return A resultCode_t value indicating the success or type of failure (http status type code).
// */
// resultCode_t mqtt_publishExtended(mqttCtrl_t *mqttCtrl, const char *topic, mqttQos_t qos, const char *encodedMsg, uint8_t timeoutSeconds)
// {
//     ASSERT(strlen(encodedMsg) <= 560);                                               // max msg length PUBEX=560, PUB=4096
//     ASSERT(strchr(encodedMsg, '"') == NULL);

//     uint32_t timeoutMS = (timeoutSeconds == 0) ? mqtt__publishTimeout : timeoutSeconds * 1000;
//     char msgText[mqtt__messageSz];
//     resultCode_t atResult = resultCode__conflict;               // assume lock not obtainable, conflict

//     mqttCtrl->lastMsgId++;                                                                                      // keep sequence going regardless of MQTT QOS
//     uint16_t msgId = ((uint8_t)qos == 0) ? 0 : mqttCtrl->lastMsgId;                                             // msgId not sent with QOS == 0, otherwise sent

//     // AT+QMTPUBEX=<tcpconnectID>,<msgID>,<qos>,<retain>,"<topic>","<msg,len<=(560)>"
//     if (atcmd_tryInvoke("AT+QMTPUBEX=%d,%d,%d,0,\"%s\",\"%s\"", mqttCtrl->dataContext, msgId, qos, topic, encodedMsg))
//     {
//         atResult = atcmd_awaitResultWithOptions(PERIOD_FROM_SECONDS(timeoutSeconds), NULL);
//         if (atResult != resultCode__success)                                        
//         {
//             PRINTF(dbgColor__dYellow, "MQTT-PUB ERROR: rslt=%d(%d)\r", atResult, atcmd_getValue());
//         }
//     }
//     atcmd_close();
//     return atResult;
// }


/** 
 *  @brief Publish a message to server.
*/
resultCode_t mqtt_publish(mqttCtrl_t *mqttCtrl, const char *topic, mqttQos_t qos, const char *message, uint8_t timeoutSeconds)
{
    ASSERT(strlen(message) <= 4096);                                                                                // max msg length PUBEX=560, PUB=4096
    
    uint8_t pubstate = 0;
    uint32_t timeoutMS = (timeoutSeconds == 0) ? mqtt__publishTimeout : timeoutSeconds * 1000;
    char msgText[mqtt__messageSz];
    resultCode_t atResult = resultCode__conflict;                                                                   // assume lock not obtainable, conflict
    if (ATCMD_awaitLock(timeoutMS))
    {
        mqttCtrl->sentMsgId++;                                                                                      // keep sequence going regardless of MQTT QOS
        uint16_t msgId = ((uint8_t)qos == 0) ? 0 : mqttCtrl->sentMsgId;                                             // msgId not sent with QOS == 0, otherwise sent
        // AT+QMTPUB=<tcpconnectID>,<msgID>,<qos>,<retain>,"<topic>"
        atcmd_invokeReuseLock("AT+QMTPUB=%d,%d,%d,0,\"%s\"", mqttCtrl->dataContext, msgId, qos, topic);
        pubstate++;
        atResult = atcmd_awaitResultWithOptions(PERIOD_FROM_SECONDS(timeoutSeconds), ATCMD_txDataPromptParser);     // wait for data prompt 
        if (atResult == resultCode__success)                                        
        {
            pubstate++;
            strcat(message, ASCII_CtrlZ_STR);
            atcmd_sendCmdData(message, strlen(message));                                                            // now send data
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
 *  @brief Disconnect and close a connection to a MQTT server
*/
void mqtt_close(mqttCtrl_t *mqttCtrl)
{
    /* not fully documented how Quectel intended to use close/disconnect, LTEmC uses AT+QMTCLOSE which appears to work for both open (NC) and connected states
    */
    if (mqttCtrl->state >= mqttState_open)                                                      // LTEmC uses CLOSE
    {
        if (atcmd_tryInvoke("AT+QMTCLOSE=%d", mqttCtrl->dataContext))
            atcmd_awaitResultWithOptions(5000, NULL);
    }
    mqttCtrl->state == mqttState_closed;
    return resultCode__success;
}


/**
 *  @brief Reset and attempt to reestablish a server connection.
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  @param resetModem [in] True if modem should be reset prior to reestablishing MQTT connection.
*/
resultCode_t mqtt_reset(mqttCtrl_t *mqttCtrl, bool resetModem)
{
    mqtt_close(mqttCtrl);

    // more intrusive MQTT reset, BGx high-level protocols when faulted can fail to recover with less intrusive reset efforts
    if (resetModem)                                     
    {
        ltem_start(resetAction_swReset);
    }
    mqtt_start(mqttCtrl, true);
}


/**
 *  @brief Return current MQTT connection state.
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  @return A mqttState_t value indicating the state of the MQTT connection.
*/
mqttState_t mqtt_getStatus(mqttCtrl_t *mqttCtrl)
{
    return mqttCtrl->state;
}


/**
 *  @brief Query the status of the MQTT connection state.
 *  @note  This function works around a known issue with most (if not all) BGx firmware versions.
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
 *  @brief Get the last outgoing message ID.
*/
uint16_t mqtt_getSentMsgId(mqttCtrl_t *mqttCtrl)
{
    mqttCtrl->sentMsgId;
}


/**
 *  @brief Get the last incoming message ID.
*/
uint16_t mqtt_getRecvMsgId(mqttCtrl_t *mqttCtrl)
{
    mqttCtrl->recvMsgId;
}


/**
 *  @brief Get the last incoming message ID.
*/
uint16_t mqtt_getErrCode(mqttCtrl_t *mqttCtrl)
{
    mqttCtrl->errCode;
}


#pragma endregion   // public API

/* private mqtt functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions


static uint8_t S__findtopicIndx(mqttCtrl_t* mqttCntl, mqttTopicCtrl_t* topicCtrl)
{
    uint8_t emptySlot = UINT8_MAX;

    for (size_t i = 0; i < mqtt__topicsCnt; i++)
    {
        if (mqttCntl->topics[i] == NULL)
        {
            emptySlot = MIN(i, emptySlot);
        }
        else
        {
            if (memcmp(mqttCntl->topics[i]->topicName, topicCtrl->topicName, mqtt__topic_nameSz) == 0)
            {
                return i;
            }
        }
    }
    return emptySlot;
}


static resultCode_t S__notifyServerTopicChange(mqttCtrl_t* mqttCtrl, uint8_t topicIndx, bool subscribe)
{
    if (subscribe)
    {
        if (atcmd_tryInvoke("AT+QMTSUB=%d,%d,\"%s\",%d", mqttCtrl->dataContext, ++mqttCtrl->sentMsgId, mqttCtrl->topics[topicIndx]->topicName, mqttCtrl->topics[topicIndx]->Qos))
        {
            return atcmd_awaitResultWithOptions(PERIOD_FROM_SECONDS(30), S__mqttSubscribeCompleteParser);
        }
    }
    else
    {
        if (atcmd_tryInvoke("AT+QMTUNS=%d,%d,\"%s\"", mqttCtrl->dataContext, ++mqttCtrl->sentMsgId, mqttCtrl->topics[topicIndx]->topicName))
        {
            return atcmd_awaitResult();
        }
    }
}


static void S__mqttUrcHandler()
{
    cBuffer_t* rxBffr = g_lqLTEM.iop->rxBffr;                               // for convenience

    /*
    +QMTRECV: <tcpconnectID>,<msgID>,"<topic>","<payload>"
    +QMTRECV: 5,65535,"<topic>","<payload>"
    +QMTSTAT: <tcpconnectID>,<err_code>
    */

    if (cbffr_find(rxBffr, "+QMT", 0, 0, false) == CBFFR_NOFIND ||          // not a MQTT URC
        cbffr_getOccupied(rxBffr) < 20)                                     //  or not sufficient chars to parse URC header
    {
        return;                                                     
    }

    char workBffr[512] = {0};
    char* workPtr = workBffr;
    uint8_t dataCntxt;

    /* MQTT Receive Message
     * -------------------------------------------------------------------------------------
     */
    if (cbffr_find(rxBffr, "+QMTRECV:", 0, 0, true) >= 0)                                       // if recv, move tail to start of header
    {
        uint16_t findIndx = cbffr_find(rxBffr, "\",\"", sizeof("+QMTRECV: "), 2, false);        // "," separator: "topic","message"
        if (CBFFR_NOTFOUND(findIndx))
        {
            return;
        }
        ASSERT(findIndx < sizeof(workBffr));
        cbffr_pop(rxBffr, workBffr, findIndx + 3);                                              // rxBffr->tail now points to message, operate on header in workBffr

        workPtr = memchr(workPtr, ',', sizeof(workBffr));
        dataCntxt = strtol(workPtr, &workPtr, 10);
        workPtr++;
        uint16_t msgId = strtol(workPtr, &workPtr, 10);

        // find topic, to get callback
        ASSERT(g_lqLTEM.streams[dataCntxt]->streamType == streamType_MQTT);
        mqttCtrl_t* mqttCtrl = (mqttCtrl_t*)g_lqLTEM.streams[dataCntxt];

        mqttTopicCtrl_t* topicCtrl;
        uint16_t topicLen;
        for (size_t i = 0; i < mqtt__topicsCnt; i++)
        {
            if (mqttCtrl->topics[i]->wildcard)                                          // wildcard: compare length of registered topic name (prefix)
            {
                topicLen = strlen(mqttCtrl->topics[i]->topicName);
            }
            else                                                                        // no wildcard: compare length of received topic name (full name)
            {
                uint16_t bffrLen = workBffr + sizeof(workBffr) - workPtr;
                topicLen = memchr(workPtr, '\"', bffrLen) - (void*)workPtr;
            }
            if (memcmp(mqttCtrl->topics[i]->topicName, workPtr, topicLen) == 0)     // found topic control
            {
                topicCtrl = mqttCtrl->topics[i];
            }
        }
        
        // forward topic
        PRINTF(dbgColor__cyan, "httpPageRcvr() topic ptr=%p blkSz=%d \r", workBffr, topicLen);
        ((mqttAppRecv_func)topicCtrl->appRecvDataCB)(dataCntxt, mqttMsgSegment_topic, msgId, workBffr, topicLen, false);

        // forward topic extension
        uint16_t extensionLen = strlen(workPtr + 1);
        if (extensionLen > 0)
        {
            PRINTF(dbgColor__cyan, "httpPageRcvr() topic ptr=%p blkSz=%d \r", workPtr, topicLen);
            ((mqttAppRecv_func)topicCtrl->appRecvDataCB)(dataCntxt, mqttMsgSegment_topicExt, msgId, workPtr, extensionLen, false);
        }

        bool eomFound = false;
        char* streamPtr;
        uint16_t reqstBlockSz = cbffr_getCapacity(rxBffr) / 4;
        do
        {
            uint16_t blockSz = cbffr_popBlock(rxBffr, &streamPtr, reqstBlockSz);
            eomFound = lq_strnstr(streamPtr, "\"\r\n", blockSz) != NULL;

            PRINTF(dbgColor__cyan, "httpPageRcvr() part=2 ptr=%p blkSz=%d isFinal=%d\r", streamPtr, blockSz, eomFound);

            //typedef void (*mqttTopicRecv_func)(dataCntxt_t dataCntxt, uint8_t topicHandle, uint16_t msgId, char* streamData, uint16_t blockSz, bool isFinal);

            // signal new receive data available to host application
            ((mqttAppRecv_func)topicCtrl->appRecvDataCB)(dataCntxt, mqttMsgSegment_msgBody, msgId, streamPtr, blockSz, eomFound);

            cbffr_popBlockFinalize(g_lqLTEM.iop->rxBffr, true);                                 // commit POP
        } while (!eomFound);
    }

    /* MQTT Status Change
     * ------------------------------------------------------------------------------------- */
    else if (cbffr_find(rxBffr, "+QMTSTAT", 0, 20, true) >= 0)        // MQTT connection closed
    {
        uint16_t eopUrl = cbffr_find(rxBffr, "\r\n", 0, 0, false);
        if (CBFFR_FOUND(eopUrl))
        {
            cbffr_pop(rxBffr, workBffr, eopUrl);
            workPtr = lq_strnstr(workBffr, "+QMSTAT", sizeof(workBffr)) + 9;

            uint8_t cntxt = strtol(workPtr, &workPtr, 10);
            workPtr++;
            uint8_t indx = LTEM__getStreamIndx(cntxt);
            ((mqttCtrl_t*)g_lqLTEM.streams[indx])->errCode = strtol(workPtr, NULL, 10);
            ((mqttCtrl_t*)g_lqLTEM.streams[indx])->state = mqttState_closed;
        }
    }
}


#pragma endregion

/* MQTT ATCMD Parsers
 * --------------------------------------------------------------------------------------------- */
#pragma region MQTT ATCMD Parsers


/**
 *	@brief [private] MQTT open connection response parser.
 *  @return LTEmC parse result
 */
static cmdParseRslt_t S__mqttOpenCompleteParser() 
{
    cmdParseRslt_t parserRslt = atcmd_stdResponseParser("+QMTOPEN: ", true, ",", 0, 2, "", 0);
    return parserRslt;
}


/**
 *	@brief [private] MQTT connect to server response parser.
 *  @return LTEmC parse result
 */
static cmdParseRslt_t S__mqttConnectCompleteParser() 
{
    // return ATCMD_testResponseTrace();
    return atcmd_stdResponseParser("+QMTCONN: ", true, ",", 0, 3, "\r\n", 0);
}


/**
 *	@brief [private] MQTT connect to server response parser.
 *  @return LTEmC parse result
 */
static cmdParseRslt_t S__mqttConnectStatusParser() 
{
    // BGx +QMTCONN Returns status: 1 = connecting, 3 = connected. Service parser returns 200 + status.
    // A simple "OK" response indicates no connection
    return atcmd_stdResponseParser("+QMTCONN: ", false, ",", 0, 2, "OK\r\n", 0);
}


/**
 *	@brief [private] MQTT subscribe to topic response parser.
 *  @return LTEmC parse result
 */
static cmdParseRslt_t S__mqttSubscribeCompleteParser() 
{
    return atcmd_stdResponseParser("+QMTSUB: ", true, ",", 0, 4, "\r\n", 0);
}


/**
 *	@brief [private] MQTT publish message to topic response parser.
 *  @return LTEmC parse result
 */
static cmdParseRslt_t S__mqttPublishCompleteParser() 
{
    return atcmd_stdResponseParser("+QMTPUB: ", true, ",", 0, 2, "", 0);
}


/**
 *	@brief [private] MQTT publish message to topic response parser.
 *  @return LTEmC parse result
 */
static cmdParseRslt_t S__mqttCloseCompleteParser() 
{
    return atcmd_stdResponseParser("OK\r\n\r\n+QMTCLOSE: ", true, ",", 0, 2, "", 0);
}


#pragma endregion
