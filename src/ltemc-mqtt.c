/** ***************************************************************************
  @file 
  @brief Modem MQTT(S) communication functions/services.

  @author Greg Terrell, LooUQ Incorporated

  \loouq
-------------------------------------------------------------------------------

LooUQ-LTEmC // Software driver for the LooUQ LTEm series cellular modems.
Copyright (C) 2017-2023 LooUQ Incorporated

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
Also add information on how to contact you by electronic and paper mail.

**************************************************************************** */


#include <lq-embed.h>
#define lqLOG_LEVEL lqLOGLEVEL_DBG
//#define DISABLE_ASSERTS                   // ASSERT/ASSERT_W enabled by default, can be disabled 
#define LQ_SRCFILE "MQT"                        // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT

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

static uint8_t S__findtopicIndx(mqttCtrl_t *mqttCntl, mqttTopicCtrl_t *topicCtrl);
static resultCode_t S__notifyServerTopicChange(mqttCtrl_t *mqttCtrl, mqttTopicCtrl_t *topicCtrl, bool subscribe);
static void S__mqttUrcHandler();

// static cmdParseRslt_t S__mqttOpenStatusParser();
// static cmdParseRslt_t S__mqttOpenCompleteParser();
// static cmdParseRslt_t S__mqttConnectCompleteParser();
// static cmdParseRslt_t S__mqttConnectStatusParser();
// static cmdParseRslt_t S__mqttSubscribeCompleteParser();
// static cmdParseRslt_t S__mqttPublishCompleteParser();

/* public mqtt functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions

/**
 *  @brief Initialize a MQTT protocol control structure.
 */
void mqtt_initControl(mqttCtrl_t *mqttCtrl, dataCntxt_t dataCntxt)
{
    ASSERT(dataCntxt < dataCntxt__cnt);       // valid streams index
    ASSERT(g_lqLTEM.streams[dataCntxt] == 0); // context not already in use

    memset(mqttCtrl, 0, sizeof(mqttCtrl_t));

    mqttCtrl->streamType = streamType_MQTT;
    mqttCtrl->urcEvntHndlr = S__mqttUrcHandler; // for MQTT, URC handler performs all necessary functions
    mqttCtrl->dataRxHndlr = NULL;               // marshalls data from buffer to app done by URC handler
}


/**
 *  @brief Initialize a MQTT topic subscription control structure.
 */
void mqtt_initTopicControl(mqttTopicCtrl_t *topicCtrl, const char *topic, uint8_t qos, mqttAppRcvr_func appTopicRcvrCB)
{
    memset(topicCtrl, 0, sizeof(mqttTopicCtrl_t));

    uint16_t topicLen = strlen(topic);
    if (topic[topicLen - 1] == '#')
    {
        topicCtrl->wildcard = '#';
        topicLen -= 2;
    }
    else
    {
        topicCtrl->wildcard = '\0';
    }
    ASSERT(topicLen < mqtt__topic_nameSz);

    memcpy(topicCtrl->topicName, topic, topicLen);
    topicCtrl->Qos = qos;
    topicCtrl->appRcvrCB = appTopicRcvrCB;
}


// /**
//  *  @brief Set the remote server connection values.
//  */
// void mqtt_setConnection_D(mqttCtrl_t *mqttCtrl, const char *hostUrl, uint16_t hostPort, bool useTls, mqttVersion_t mqttVersion, const char *deviceId, const char *userId, const char *password)
// {
//     ASSERT(mqttCtrl != NULL);

//     strcpy(mqttCtrl->hostUrl, hostUrl);
//     mqttCtrl->hostPort = hostPort;
//     mqttCtrl->useTls = useTls;
//     mqttCtrl->mqttVersion = mqttVersion;

//     strncpy(mqttCtrl->clientId, deviceId, mqtt__clientIdSz);
//     strncpy(mqttCtrl->username, userId, mqtt__userNameSz);
//     strncpy(mqttCtrl->password, password, mqtt__userPasswordSz);
// }


/**
 *  @brief Set the remote server connection values.
*/
void mqtt_setConnection(mqttCtrl_t *mqttCtrl, const char *hostUrl, uint16_t hostPort, tlsCtrl_t* tlsCtrl, mqttVersion_t mqttVersion, const char *deviceId, const char *userId, const char *password)
{
    ASSERT(mqttCtrl != NULL);

    strcpy(mqttCtrl->hostUrl, hostUrl);
    mqttCtrl->hostPort = hostPort;
    mqttCtrl->mqttVersion = mqttVersion;
    if (tlsCtrl != NULL)
    {
        mqttCtrl->useTls = true;
        mqttCtrl->tlsCtrl = tlsCtrl;
        tls_applySettings(mqttCtrl->dataCntxt, tlsCtrl);
    }
    strncpy(mqttCtrl->clientId, deviceId, mqtt__clientIdSz);
    strncpy(mqttCtrl->username, userId, mqtt__userNameSz);
    strncpy(mqttCtrl->password, password, mqtt__userPasswordSz);
}


/**
 *  @brief Open and connect to a remote MQTT server.
 */
resultCode_t mqtt_start(mqttCtrl_t *mqttCtrl, bool cleanSession)
{
    resultCode_t rslt;
    LOG_INFO("MQTT Starting\r\n");
    do
    {
        ltem_addStream(mqttCtrl); // register stream for background receive operations (URC)
        lqLOG_VRBS("MQTT stream registered\r\n");

        rslt = mqtt_open(mqttCtrl);
        if (rslt != resultCode__success)
        {
            LOG_WARN("Open fail status=%d\r\n", rslt);
            break;
        }
        lqLOG_VRBS("MQTT Opened\r\n");

        rslt = mqtt_connect(mqttCtrl, true);
        if (rslt != resultCode__success)
        {
            LOG_WARN("Connect fail status=%d\r\n", rslt);
            break;
        }
        lqLOG_VRBS("MQTT Connected\r\n");

        for (size_t i = 0; i < mqtt__topicsCnt; i++)
        {
            if (mqttCtrl->topics[i] != NULL)
            {
                rslt = S__notifyServerTopicChange(mqttCtrl, mqttCtrl->topics[i], true);
                if (rslt != resultCode__success)
                {
                    return rslt;
                }
                break;
            }
        }
        LOG_INFO("MQTT Started\r\n");               // if still in block MQTT started successfully
        mqttCtrl->state = mqttState_connected;

    } while (0);                                                // do/while once, enable breaks
    return rslt;
}


/**
 *  @brief Open a remote MQTT server IP connection for use.
 *  @details Preferred way for user app to connect to server is via mqtt_start() or mqtt_reset()
 */
resultCode_t mqtt_open(mqttCtrl_t *mqttCtrl)
{
    // AT+QSSLCFG="sslversion",5,3
    // AT+QMTCFG="ssl",5,1,0
    // AT+QMTCFG="version",5,4
    // AT+QMTOPEN=5,"iothub-dev-pelogical.azure-devices.net",8883

    atcmdResult_t rslt;

    if (mqttCtrl->state >= mqttState_open)                          // already open or connected
        return resultCode__success;

    /* set session options prior to server open
     * ----------------------------------------------------------------------------------------------*/
    if (mqttCtrl->useTls)
    {
        if (atcmd_tryInvoke("AT+QMTCFG=\"ssl\",%d,1,%d", mqttCtrl->dataCntxt, mqttCtrl->dataCntxt))                 // enable SSL/TLS
        {
            if (atcmd_awaitResult() != resultCode__success)
                return resultCode__internalError;
        }
    }

    if (atcmd_tryInvoke("AT+QMTCFG=\"version\",%d,4", mqttCtrl->dataCntxt, mqttCtrl->mqttVersion))                  // set MQTT version
    {
        if (atcmd_awaitResult() != resultCode__success)
            return resultCode__internalError;
    }

    /* now open server
     * ----------------------------------------------------------------------------------------------*/
    if (atcmd_tryInvoke("AT+QMTOPEN=%d,\"%s\",%d", mqttCtrl->dataCntxt, mqttCtrl->hostUrl, mqttCtrl->hostPort))
    {
        atcmd_ovrrdTimeout(SEC_TO_MS(30));
        //atcmd_ovrrdParser(S__mqttOpenCompleteParser);
        atcmd_configParser("+QMTOPEN: ", true, ",", 0, "", 0);

        resultCode_t rslt = atcmd_awaitResult();
        lqLOG_VRBS("MQTT Open Resp: %s", atcmd_getRawResponse());

        // if (rslt == resultCode__success && atcmd_getValue() == 0)
        if (rslt == resultCode__success)
        {
            const char* token = atcmd_getToken(1);
            g_lqLTEM.atcmd->resultValue = strtol(token, NULL, 10);

            switch (g_lqLTEM.atcmd->resultValue)
            {
                case 0:
                {
                    mqttCtrl->state = mqttState_open;
                    return resultCode__success;
                }
                case 1:
                    return resultCode__badRequest;                                          // wrong parameter
                case 2:
                    return resultCode__conflict;                                            // MQTT socket identifier is occupied
                case 4:
                    return resultCode__notFound;                                            // failed to parse domain name
                default:
                    return resultCode__extendedCodesBase + g_lqLTEM.atcmd->resultValue;     // everything else
            }
        }
    }
    return resultCode__internalError;
}


/**
 *  @brief Connect (authenticate) to a MQTT server.
 *  @details Preferred way for user app to connect to server is via mqtt_start() or mqtt_reset()
 */
resultCode_t mqtt_connect(mqttCtrl_t *mqttCtrl, bool cleanSession)
{
    resultCode_t rslt;
    if (mqttCtrl->state == mqttState_connected)
        return resultCode__success;

    atcmd_tryInvoke("AT+QMTCFG=\"session\",%d,%d", mqttCtrl->dataCntxt, (uint8_t)cleanSession); // set option to clear session history on connect
    if (atcmd_awaitResult() != resultCode__success)
        return resultCode__internalError;

    atcmd_tryInvoke("AT+QMTCONN=%d,\"%s\",\"%s\",\"%s\"", mqttCtrl->dataCntxt, mqttCtrl->clientId, mqttCtrl->username, mqttCtrl->password);

   	atcmd_ovrrdTimeout(SEC_TO_MS(60));
    // atcmd_ovrrdParser(S__mqttConnectCompleteParser);
    atcmd_configParser("+QMTCONN: ", true, ",", 0, "\r\n", 0);
    rslt = atcmd_awaitResult();
    lqLOG_VRBS("MQTT Open Resp: %s", atcmd_getRawResponse());

    if (rslt == resultCode__success)                                                    // COMMAND executed, outcome of CONNECTION may not be a success
    {
        const char* token = atcmd_getToken(2);
        g_lqLTEM.atcmd->resultValue = strtol(token, NULL, 10);
        switch (g_lqLTEM.atcmd->resultValue)
        {
            case 0:
                return resultCode__success;
            case 1:                                                                     // Connection Refused: Unacceptable Protocol Version
                return resultCode__methodNotAllowed;
            case 2:                                                                     // Connection Refused: Identifier Rejected
                return resultCode__forbidden;
            case 3:                                                                     // Connection Refused: Server Unavailable (iotHub: user not found)
                return resultCode__notFound;
            case 4:                                                                     // Connection Refused: Bad User Name or Password
                return resultCode__forbidden;
            case 5:                                                                     // Connection Refused: Not Authorized
                return resultCode__forbidden;
            default:
                return resultCode__extendedCodesBase + g_lqLTEM.atcmd->resultValue;     // everything else
        }
    }
    return resultCode__badRequest; // command rejected by BGx
}


/**
 *  @brief Open and connect to a remote MQTT server.
 */
resultCode_t mqtt_subscribeTopic(mqttCtrl_t *mqttCtrl, mqttTopicCtrl_t *topicCtrl)
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
resultCode_t mqtt_cancelTopic(mqttCtrl_t *mqttCtrl, mqttTopicCtrl_t *topicCtrl)
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
//     resultCode_t rslt = resultCode__conflict;               // assume lock not obtainable, conflict

//     mqttCtrl->lastMsgId++;                                                                                      // keep sequence going regardless of MQTT QOS
//     uint16_t msgId = ((uint8_t)qos == 0) ? 0 : mqttCtrl->lastMsgId;                                             // msgId not sent with QOS == 0, otherwise sent

//     // AT+QMTPUBEX=<tcpconnectID>,<msgID>,<qos>,<retain>,"<topic>","<msg,len<=(560)>"
//     if (atcmd_tryInvoke("AT+QMTPUBEX=%d,%d,%d,0,\"%s\",\"%s\"", mqttCtrl->dataCntxt, msgId, qos, topic, encodedMsg))
//     {
//         rslt = atcmd_awaitResultWithOptions(PERIOD_FROM_SECONDS(timeoutSeconds), NULL);
//         if (rslt != resultCode__success)
//         {
//             DPRINT(PRNT_YELLOW, "MQTT-PUB ERROR: rslt=%d(%d)\r\n", rslt, atcmd_getValue());
//         }
//     }
//     atcmd_close();
//     return rslt;
// }

/**
 *  @brief Publish a message to server.
 */
resultCode_t mqtt_publish(mqttCtrl_t *mqttCtrl, const char *topic, mqttQos_t qos, const char *message, uint16_t messageSz, uint8_t timeoutSec)
{
    ASSERT(messageSz <= 4096);                                                                          // max msg length PUB=4096 (PUBEX=560)

    RSLT = resultCode__preConditionFailed;                                                              // MQTT not in connected state
    uint32_t timeoutMS = (timeoutSec == 0) ? mqtt__publishTimeout : SEC_TO_MS(timeoutSec);

    mqttCtrl->sentMsgId++;                                                                              // keep sequence going regardless of MQTT QOS
    uint16_t msgId = ((uint8_t)qos == 0) ? 0 : mqttCtrl->sentMsgId;                                     // msgId not sent with QOS == 0, otherwise sent

    if (mqttCtrl->state == mqttState_connected)                                                         // soft-check MQTT state
    {
        atcmd_ovrrdTimeout(timeoutMS);
        atcmd_configDataMode(mqttCtrl, "> ", atcmd_txHndlrDefault, message, messageSz, NULL, true);
        atcmd_configParser("+QMTPUB: ", true, ",", 0, "\r\n", 0);

        if (IS_SUCCESS_RSLT(atcmd_dispatch("AT+QMTPUB=%d,%d,%d,0,\"%s\",%d", mqttCtrl->dataCntxt, msgId, qos, topic, messageSz)))
        {
            char* pubRslt = atcmd_getToken(2);
            ASSERT(pubRslt);

            if (strtol(pubRslt, NULL, 10) == 2)
            {
                LOG_WARN("MQTT-PUB MsgId=%d failed.\r\n", msgId);
            }
            LOG_INFO("MQTT-PUB MsgId=%s successful\r\n", atcmd_getToken(1));
        }
    }
    return rslt;
}

/**
 *  @brief Disconnect and close a connection to a MQTT server
 */
void mqtt_close(mqttCtrl_t *mqttCtrl)
{
    /* not fully documented how Quectel intended to use close/disconnect, trying whats here
     */
    if (mqttCtrl->state == mqttState_connected)
    {
        atcmd_ovrrdTimeout(SEC_TO_MS(60));
        atcmd_dispatch("AT+QMTDISC=%d", mqttCtrl->dataCntxt);                  // get status is used to determine outcome
    }
    else if (mqttCtrl->state == mqttState_open)
    {

        atcmd_ovrrdTimeout(SEC_TO_MS(60));
        atcmd_dispatch("AT+QMTCLOSE=%d", mqttCtrl->dataCntxt);                  // get status is used to determine outcome
    }
    mqttCtrl->state == mqtt_readStatus(mqttCtrl);                               // fetch MQTT state
}


/**
 *  @brief Reset and attempt to reestablish a server connection.
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  @param resetModem [in] True if modem should be reset prior to reestablishing MQTT connection.
 */
void mqtt_reset(mqttCtrl_t *mqttCtrl, bool resetModem)
{
    SC16IS7xx_flushTx(' ', 1548);
    mqtt_close(mqttCtrl);

    // more intrusive MQTT reset, BGx high-level protocols when faulted can fail to recover with less intrusive reset efforts
    if (resetModem)
    {
        ltem_start(resetAction_swReset);
    }
    mqtt_start(mqttCtrl, true);
}

void mqtt_flush(mqttCtrl_t *mqttCtrl)
{
    for (size_t i = 0; i < 1548; i++)
    {
        /* code */
    }
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
mqttState_t mqtt_readStatus(mqttCtrl_t *mqttCtrl)
{
    // See BG96_MQTT_Application_Note: AT+QMTOPEN? and AT+QMTCONN?

    /* BGx modules will not respond over serial to AT+QMTOPEN? (command works fine over USB, Quectel denies this)
     * AT+QMTCONN? returns a state == 1 (MQTT is initializing) when MQTT in an open, not connected condition
     */

    resultCode_t rslt;
    if (atcmd_tryInvoke("AT+QMTCONN?"))
    {
        atcmd_ovrrdTimeout(SEC_TO_MS(5));
        // atcmd_ovrrdParser(S__mqttConnectStatusParser);
        atcmd_configParser("+QMTCONN: ", false, ",", 0, "OK\r\n", 0);
        rslt = atcmd_awaitResult();

        if (rslt == resultCode__success)
        {
            if (atcmd_getPreambleFound())
            {
                char* retCodePtr = atcmd_getToken(1);
                char retCode = (strlen(retCodePtr) > 0) ? retCodePtr[0] : '\0';

                switch (retCode)
                {
                case '1':
                    mqttCtrl->state = mqttState_open;
                    break;
                case '2':
                    mqttCtrl->state = mqttState_connecting;
                    break;
                case '3':
                    mqttCtrl->state = mqttState_connected;
                    break;
                case '4':
                    mqttCtrl->state = mqttState_disconnecting;
                    break;
                default:
                    mqttCtrl->state = mqttState_closed; // 0 or unknown
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
    return mqttCtrl->sentMsgId;
}

/**
 *  @brief Get the last incoming message ID.
 */
uint16_t mqtt_getRecvMsgId(mqttCtrl_t *mqttCtrl)
{
    return mqttCtrl->recvMsgId;
}

/**
 *  @brief Get the last incoming message ID.
 */
uint16_t mqtt_getErrCode(mqttCtrl_t *mqttCtrl)
{
    return mqttCtrl->errCode;
}

#pragma endregion // public API

/* private mqtt functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions

static uint8_t S__findtopicIndx(mqttCtrl_t *mqttCntl, mqttTopicCtrl_t *topicCtrl)
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

static resultCode_t S__notifyServerTopicChange(mqttCtrl_t *mqttCtrl, mqttTopicCtrl_t *topicCtrl, bool subscribe)
{
    char topicName[mqtt__topic_nameSz + 1];
    strcpy(topicName, topicCtrl->topicName);
    if (topicCtrl->wildcard)
    {
        strcat(topicName, "/#");
    }

    if (subscribe)
    {
        atcmd_ovrrdTimeout(SEC_TO_MS(30));
        // atcmd_ovrrdParser(S__mqttSubscribeCompleteParser);
        atcmd_configParser("+QMTCONN: ", false, ",", 0, "OK\r\n", 0);

        return atcmd_dispatch("AT+QMTSUB=%d,%d,\"%s\",%d", mqttCtrl->dataCntxt, ++mqttCtrl->sentMsgId, topicName, topicCtrl->Qos);
    }
    else
    {
        return atcmd_dispatch("AT+QMTUNS=%d,%d,\"%s\"", mqttCtrl->dataCntxt, ++mqttCtrl->sentMsgId, topicName);
    }
    return resultCode__internalError;
}


static void S__mqttUrcHandler()
{
    bBuffer_t *rxBffr = g_lqLTEM.iop->rxBffr; // for convenience

    /*
    +QMTRECV: <tcpconnectID>,<msgID>,"<topic>","<payload>"
    +QMTRECV: 5,65535,"<topic>","<payload>"
    +QMTSTAT: <tcpconnectID>,<err_code>
    */

    if (BBFFR_ISNOTFOUND(bbffr_find(rxBffr, "+QMT", 0, 0, false)) || // not a MQTT URC
        bbffr_getOccupied(rxBffr) < 20)                            // -or- not sufficient chars to parse URC header
    {
        return;
    }

    char workBffr[512] = {0};
    char *workPtr = workBffr;
    uint8_t dataCntxt;

    /* MQTT Receive Message
     * -------------------------------------------------------------------------------------
     */
    if (BBFFR_ISFOUND(bbffr_find(rxBffr, "+QMTRECV:", 0, 0, true))) // if recv, move tail to start of header
    {
        // separator: "topic","message"           ,"            search offset from URC prefix
        uint16_t findIndx = bbffr_find(rxBffr, "\",\"", sizeof("+QMTRECV: "), 2, false);
        if (BBFFR_ISNOTFOUND(findIndx))
        {
            return;
        }
        ASSERT(findIndx < sizeof(workBffr));
        bbffr_pop(rxBffr, workBffr, findIndx + 3); // rxBffr->tail now points to message, operate on header in workBffr

        workPtr += sizeof("+QMTRECV: ") - 1;
        dataCntxt = strtol(workPtr, &workPtr, 10);
        workPtr++;
        uint16_t msgId = strtol(workPtr, &workPtr, 10);

        // find topic in ctrl, to get callback func
        streamType_t *ctrlPtr = ltem_findStream(dataCntxt);
        ASSERT(ctrlPtr != NULL);
        mqttCtrl_t *mqttCtrl = (mqttCtrl_t *)ctrlPtr;

        mqttTopicCtrl_t *topicCtrl;
        uint16_t topicLen;
        workPtr += 2;
        bool topicFound = false;
        for (size_t i = 0; i < mqtt__topicsCnt; i++)
        {
            if (mqttCtrl->topics[i]->wildcard) // wildcard: compare length of registered topic name (suffix prev removed)
            {
                topicLen = strlen(mqttCtrl->topics[i]->topicName);
            }
            else // no wildcard: compare length of received topic name (full name)
            {
                uint16_t bffrLen = workBffr + sizeof(workBffr) - workPtr;
                topicLen = memchr(workPtr, '\"', bffrLen) - (void *)workPtr;
            }
            if (memcmp(mqttCtrl->topics[i]->topicName, workPtr, topicLen) == 0) // found topic control
            {
                topicFound = true;
                topicCtrl = mqttCtrl->topics[i];
                break;
            }
        }
        ASSERT(topicFound); // assert that we can find topic that we told server to send us

        // forward topic
        lqLOG_VRBS("mqttUrcHndlr() topic ptr=%p blkSz=%d \r\n", workPtr, topicLen);
        ((mqttAppRcvr_func)topicCtrl->appRcvrCB)(dataCntxt, msgId, mqttMsgSegment_topic, workPtr, topicLen, false);

        // forward topic extension
        workPtr += topicLen + 1;
        uint16_t extensionLen = strlen(workPtr);
        if (extensionLen > 0)
        {
            extensionLen -= 3; // remove topic(w/extension) and message body delimiter
            lqLOG_VRBS("mqttUrcHndlr() topicExt ptr=%p blkSz=%d \r\n", workPtr, extensionLen);
            ((mqttAppRcvr_func)topicCtrl->appRcvrCB)(dataCntxt, msgId, mqttMsgSegment_topicExt, workPtr, extensionLen, false);
        }

        bool eomFound = false;
        char *streamPtr;
        uint16_t reqstBlockSz = bbffr_getCapacity(rxBffr) / 4;
        do
        {
            uint16_t blockSz = bbffr_popBlock(rxBffr, &streamPtr, reqstBlockSz);
            eomFound = lq_strnstr(streamPtr, "\"\r\n", blockSz) != NULL;
            blockSz -= (eomFound) ? 3 : 0; // adjust blockSz to not include in app content

            lqLOG_VRBS("mqttUrcHndlr() msgBody ptr=%p blkSz=%d isFinal=%d\r\n", streamPtr, blockSz, eomFound);

            // signal new receive data available to host application
            ((mqttAppRcvr_func)topicCtrl->appRcvrCB)(dataCntxt, msgId, mqttMsgSegment_msgBody, streamPtr, blockSz, eomFound);

            bbffr_popBlockFinalize(g_lqLTEM.iop->rxBffr, true); // commit POP
        } while (!eomFound);
    }

    /* MQTT Status Change
     * ------------------------------------------------------------------------------------- */
    else if (BBFFR_ISFOUND(bbffr_find(rxBffr, "+QMTSTAT", 0, 20, true))) // MQTT connection closed
    {
        uint16_t eopUrl = bbffr_find(rxBffr, "\r\n", 0, 0, false);
        if (BBFFR_ISFOUND(eopUrl))
        {
            bbffr_pop(rxBffr, workBffr, eopUrl);
            workPtr = lq_strnstr(workBffr, "+QMSTAT", sizeof(workBffr)) + 9;

            uint8_t cntxt = strtol(workPtr, &workPtr, 10);
            workPtr++;

            streamCtrl_t *streamCtrl = ltem_findStream(cntxt);
            ASSERT(streamCtrl != NULL);
            ((mqttCtrl_t *)streamCtrl)->errCode = strtol(workPtr, NULL, 10);
            ((mqttCtrl_t *)streamCtrl)->state = mqttState_closed;
        }
    }
}


/**
 * @brief Translate a module specific MQTT error code into a standard web/HTTP response code.
 * 
 * @param [in] extendedResultCode BGx MQTT error code.
 * @return resultCode_t Translated standard web/HTTP response code.
 */
resultCode_t mqtt_translateExtended(uint16_t extendedResultCode)
{
    // switch (extendedResultCode)
    // {
    // case 1705:
    // case 1730:
    //     return resultCode__badRequest;          // 400

    // case 1711:
    // case 1712:
    // case 1713:
    // case 1714:
    //     return resultCode__notFound;            // 404

    // case 1702:
    // case 1726:
    // case 1727:
    // case 1728:
    //     return resultCode__timeout;             // 408

    // case 1703:
    // case 1704:
    //     return resultCode__conflict;            // 409

    // default:
    //     return resultCode__internalError;       // 500
    // }
    return extendedResultCode;
}


#pragma endregion

/* MQTT ATCMD Parsers
 * --------------------------------------------------------------------------------------------- */
#pragma region MQTT ATCMD Parsers

// /**
//  *	@brief [private] MQTT open connection response parser.
//  *  @return LTEmC parse result
//  */
// static cmdParseRslt_t S__mqttOpenCompleteParser()
// {
//     cmdParseRslt_t parserRslt = atcmd_stdResponseParser("+QMTOPEN: ", true, ",", 0, 2, "", 0);
//     return parserRslt;
// }

// /**
//  *	@brief [private] MQTT connect to server response parser.
//  *  @return LTEmC parse result
//  */
// static cmdParseRslt_t S__mqttConnectCompleteParser()
// {
//     // return atcmd_testResponseTrace();
//     return atcmd_stdResponseParser("+QMTCONN: ", true, ",", 0, 3, "\r\n", 0);
// }


// /**
//  *	@brief [private] MQTT connect to server response parser.
//  *  @return LTEmC parse result
//  */
// static cmdParseRslt_t S__mqttConnectStatusParser()
// {
//     // BGx +QMTCONN Returns status: 1 = connecting, 3 = connected. Service parser returns 200 + status.
//     // A simple "OK" response indicates no connection
//     return atcmd_stdResponseParser("+QMTCONN: ", false, ",", 0, 2, "OK\r\n", 0);
// }

// /**
//  *	@brief [private] MQTT subscribe to topic response parser.
//  *  @return LTEmC parse result
//  */
// static cmdParseRslt_t S__mqttSubscribeCompleteParser()
// {
//     return atcmd_stdResponseParser("+QMTSUB: ", true, ",", 0, 4, "\r\n", 0);
// }

// /**
//  *	@brief [private] MQTT publish message to topic response parser.
//  *  @return LTEmC parse result
//  */
// static cmdParseRslt_t S__mqttPublishCompleteParser()
// {
//     return atcmd_stdResponseParser("+QMTPUB: ", true, ",", 0, 2, "", 0);
// }

// /**
//  *	@brief [private] MQTT publish message to topic response parser.
//  *  @return LTEmC parse result
//  */
// static cmdParseRslt_t S__mqttCloseCompleteParser()
// {
//     return atcmd_stdResponseParser("OK\r\n\r\n+QMTCLOSE: ", true, ",", 0, 2, "", 0);
// }

#pragma endregion
