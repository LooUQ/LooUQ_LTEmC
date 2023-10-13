/** ****************************************************************************
  \file 
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


#ifndef __MQTT_H__
#define __MQTT_H__

#include "ltemc-types.h"
#include "ltemc-tls.h"

/** 
 *  @brief typed numeric constants used by MQTT subsystem.
*/
enum mqtt__constants
{
    mqtt__useTls = 1,
    mqtt__notUsingTls = 0,
    mqtt__publishTimeout = 15000,

    mqtt__messageSz = 1548,                                             // Maximum message size for BGx family (BG96, BG95, BG77)
    mqtt__topicsCnt = 4,
    mqtt__topic_offset = 24,
    mqtt__topic_nameSz = 90,                                            // Azure IoTHub typically 50-70 chars
    mqtt__topic_propsSz = 320,                                          // typically 250-300 bytes
    mqtt__topicSz = (mqtt__topic_nameSz + mqtt__topic_propsSz),         // Total topic size (name+props) for buffer sizing
    mqtt__topic_publishCmdOvrhdSz = 27,                                 // when publishing, number of extra chars in outgoing buffer added to AT cmd

    mqtt__topic_publishCmdSz = (mqtt__topic_nameSz + mqtt__topic_propsSz + mqtt__topic_publishCmdOvrhdSz),

    /* if Azure IoTHub Note: 3-system properties, 3-properties, plus application specified properties */
    mqtt__propertiesCnt = 12,

    mqtt__clientIdSz = 35,
    mqtt__userNameSz = 98,
    mqtt__userPasswordSz = 192
};


#define MQTT_URC_PREFIXES "QMTRECV,QMTSTAT"

/* Example connection strings key/SAS token
* ---------------------------------------------------------------------------------------------------------------------
  HostName=iothub-dev-pelogical.azure-devices.net;DeviceId=e8fdd7df-2ca2-4b64-95de-031c6b199299;SharedAccessKey=xx0p0kTA/PIUYCzOncQYWwTyzcrcNuXdQXjlKUBdkc0=
  HostName=iothub-dev-pelogical.azure-devices.net;DeviceId=e8fdd7df-2ca2-4b64-95de-031c6b199299;SharedAccessSignature=SharedAccessSignature sr=iothub-dev-pelogical.azure-devices.net%2Fdevices%2Fe8fdd7df-2ca2-4b64-95de-031c6b199299&sig=JTz%2BmVBf1BRmJr%2FgZiSpzSyHRfo4Uaxkj5POWe4h2YY%3D&se=1592909595

Azure IoTHub cloud to device 
    devices/{device_id}/messages/devicebound/#  
    Example: devices/e8fdd7df-2ca2-4b64-95de-031c6b199299/messages/devicebound/#
* ---------------------------------------------------------------------------------------------------------------------
*/

/** 
 *  @brief Emum describing the results of a MQTT publish.
*/
typedef enum mqttResult_tag
{
    mqttResult_success = 0,         // Sucessful publish.
    mqttResult_retransmission = 1,  // Publish Sucessful, with retransmission.
    mqttResult_failed = 2           // Publish failed.
} mqttResult_t;


/** 
 *  @brief Enum of available MQTT protocol version options.
*/
typedef enum mqttVersion_tag
{
    mqttVersion_3 = 3,              // Set behavior to MQTT version 3.0
    mqttVersion_311 = 4             // Set behavior to MQTT version 3.1.1, Note: this is not MQTT 4.0
} mqttVersion_t;


/** 
 *  @brief Enum specifying the MQTT QOS for a publish and subscribed topics.
*/
typedef enum mqttQos_tag
{
    mqttQos_0 = 0,                  // QOS 0: At most once (aka send and forget)
    mqttQos_1 = 1,                  // QOS 1: At least once
    mqttQos_2 = 2,                  // QOS 2: Exactly once
    mqttQos_maxAzure = 1            // MS Azure IoTHub max QOS (setting to QOS2 will force disconnect)
} mqttQos_t;


// /** 
//  *  @brief Enum specifying behavior for messages pre-session establishment (session starts with connect, not open).
// */
// typedef enum mqttSession_tag
// {
//     mqttSession_preserve = 0,       // Preserve message that arrive previous to session and deliver to application.
//     mqttSession_cleanStart = 1,     // Clear out any previously received messages and start session "clean".
// } mqttSession_t;


/** 
 *  @brief Emum describing current state of the MQTT protocol service.
*/
typedef enum mqttState_tag
{
    mqttState_closed = 0,           // MQTT is idle, not active
    mqttState_open = 1,             // MQTT is open, open but not connected
    mqttState_connected = 2,        // MQTT is connected, in session with server

    // BGx MQTT stack can hang in any of the next 3 states, only known recovery is soft-restart module
    mqttState_PENDING = 100,        ///
    mqttState_initializing = 101,   // After connect cmd, MQTT is attempting connect
    mqttState_connecting = 102,     // After connect cmd, MQTT is attempting connect
    mqttState_disconnecting = 104   // After disconnect/close cmd
} mqttState_t;


// /** 
//  *  @brief Struct describing a MQTT topic subscription.
// */
// typedef struct mqttTopicSub_tag
// {
//     char topicName[mqtt__topic_nameSz];     // Topic name. Note if the topic registered with '#' wildcard, this is removed from the topic name.
//     char wildcard;                          // Set to '#' if multilevel wildcard specified when subscribing to topic.
// } mqttTopicSub_t;
// typedef enum mqttRecvState_tag
// {
//     mqttRecvState_none = 0,
//     mqttRecvState_signalled,
//     mqttRecvState_topicDelivered,
//     mqttRecvState_msgUnderway
// } mqttRecvState_t;


typedef enum mqttMsgSegment_tag
{
    mqttMsgSegment_topic = 0,
    mqttMsgSegment_topicExt = 1,
    mqttMsgSegment_msgBody = 2
} mqttMsgSegment_t;

/** 
 *  @brief Struct describing a MQTT topic subscription.
*/
typedef struct mqttTopicCtrl_tag
{
    char topicName[PSZ(mqtt__topic_nameSz)];    // Topic name. Note if the topic registered with '#' wildcard, this is removed from the topic name.
    char wildcard;                                  // Set to '#' if multilevel wildcard specified when subscribing to topic.
    uint8_t Qos;
    appRcvProto_func appRecvDataCB;                 // callback into host application with data (cast from generic func* to stream specific function)
} mqttTopicCtrl_t;


/** 
 *  @brief Struct representing the state of a MQTT stream service.
*/
typedef struct mqttCtrl_tag
{
    char streamType;                            // stream type
    dataCntxt_t dataCntxt;                      // integer representing the source of the stream; fixed for protocols, file handle for FS
    dataHndlr_func dataRxHndlr;                 // function to handle data streaming, initiated by eventMgr() or atcmd module
    urcEvntHndlr_func urcEvntHndlr;             // function to determine if "potential" URC event is for an open stream and perform reqd actions

    /* Above section of <stream>Ctrl structure is the same for all LTEmC implemented streams/protocols TCP/HTTP/MQTT etc. 
    */
    mqttState_t state;                          // Current state of the MQTT protocol services on device.
    bool useTls;                                // flag indicating SSL/TLS applied to stream
    tlsCtrl_t* tlsCtrl;                         // NULL for no TLS/SSL, otherwise a TLS control with settings
    char hostUrl[host__urlSz];                  // URL or IP address of host
    uint16_t hostPort;                          // IP port number host is listening on (allows for 65535/0)
    mqttTopicCtrl_t* topics[mqtt__topicsCnt];   // array of topic controls, provides for independent app receive functions per topic
    char clientId[PSZ(mqtt__clientIdSz)];       // for auto-restart
    char username[PSZ(mqtt__userNameSz)];
    char password[PSZ(mqtt__userPasswordSz)];
    mqttVersion_t mqttVersion;
    uint16_t sentMsgId;                          // MQTT TX message ID for QOS, automatically incremented, rolls at max value.
    uint16_t recvMsgId;                          // last received message identifier
    uint8_t errCode;
} mqttCtrl_t;




/** 
 *  @brief Callback function to transfer incoming message data to app
 * 
 *  @details This func will be invoked multiple times (at least twice) to deliver received MQTT message data to the host
 *  application (app). The topic and msgBody will always be sent, the topicExtension (sometimes used for property pairs) may  
 *  be sent. IsFinal only applies to the msgBody part of the flow.
 *  =============================================================================================================================
 *  @param dataCntxt The data context receiving data.
 *  @param msgId MQTT ID of the message received.
 *  @param segment Enum specifying the part of the message being transfered to the app: topic, topicExtension, messageBody
 *  @param dataPtr Pointer to received data, now available to the application (see msgPart above)
 *  @param DataSz The size of the current block of data available at the streamPtr address for app consumption
 *  @param isFinal Will be true if the current block of data is the end of the received MQTT msg
 */
typedef void (*mqttAppRecv_func)(dataCntxt_t dataCntxt, uint16_t msgId, mqttMsgSegment_t segment, char* dataPtr, uint16_t dataSz, bool isFinal);


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


/**
 *  @brief Initialize a MQTT protocol control structure.
 *  @param mqttCtrl [in] Pointer to MQTT control structure governing communications.
 *  @param dataCntxt [in] Socket/data context to host this protocol stream.
 *  @param recvCallback [in] Callback function to be invoked when received data is ready.
*/
void mqtt_initControl(mqttCtrl_t *mqttCtrl, dataCntxt_t dataCntxt);


/**
 * @brief Initialize a (subscription) topic control structure
 * @details The topic control supports multilevel wildcards only, by suffixing your topic with /# the wildcard will be detected and 
 * handled during message receipt. The application receive function will be called multiple times per message, each invoke delivering
 * different parts of the incoming message.
 * 
 * @param topicCtrl Pointer to the control to initialize
 * @param topic Topic name to subscribe to on the MQTT server
 * @param qos The MQTT defined quality-of-service for messages serviced in this topic (0=At most once, 1=At least once, 2=Exactly once)
 * @param appTopicRecvCB Pointer to the application function to receive incoming messages for this topic
 */
void mqtt_initTopicControl(mqttTopicCtrl_t* topicCtrl, const char* topic, uint8_t qos, mqttAppRecv_func appTopicRecvCB);

/**
 *  @brief Set the remote server connection values.
*/
void mqtt_setConnection_D(mqttCtrl_t *mqttCtrl, const char *hostUrl, uint16_t hostPort, bool useTls, mqttVersion_t useMqttVersion, const char *deviceId, const char *userId, const char *secret);


/**
 *  @brief Set the remote server connection values.
*/
void mqtt_setConnection(mqttCtrl_t *mqttCtrl, const char *hostUrl, uint16_t hostPort, tlsCtrl_t* tlsCtrl, mqttVersion_t useMqttVersion, const char *deviceId, const char *userId, const char *secret);


/**
 *  @brief Open a remote MQTT server for use.
 *
 *  @param [in] mqttCtrl MQTT stream control to operate with.
 *  @param [in] cleanSession 
 *  @return A resultCode_t value indicating the success or type of failure.
*/
resultCode_t mqtt_start(mqttCtrl_t *mqttCtrl, bool cleanSession);


/**
 *  @brief Open a remote MQTT server for use.
 *  @details The recommended approach is to use mqtt_start() and mqtt_reset() for server connections. The 
 * 
 *  @param [in] mqttCtrl MQTT type stream control to operate on.
 *  @return A resultCode_t value indicating the success or type of failure.
*/
resultCode_t mqtt_open(mqttCtrl_t *mqttCtrl);


/**
 *  @brief Authenicate to MQTT server and create a connected data session.
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  @param cleanSession [in] True if connection should be flushed of prior msgs to start
 *  @return A resultCode_t value indicating the success or type of failure.
*/
resultCode_t mqtt_connect(mqttCtrl_t *mqttCtrl, bool cleanSession);



/**
 *  @brief Subscribe to a MQTT topic on the server.
 *  @param [in] mqttCtrl Pointer to MQTT type stream control to operate on.
 *  @param [in] topic C-string containing the topic you are subcribing to.
 *  @param qos [in] (enum) Received message qos options for subscribed messages 
 *  @return A resultCode_t value indicating the success or type of failure.
*/
resultCode_t mqtt_subscribeTopic(mqttCtrl_t *mqttCtrl, mqttTopicCtrl_t* topicCtrl);


/**
 *  @brief Unsubscribe from a MQTT topic on the server.
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  @param topic [in] C-string containing the topic you are no longer interested in.
 *  @return A resultCode_t value indicating the success or type of failure.
*/
resultCode_t mqtt_cancelTopic(mqttCtrl_t *mqttCtrl, mqttTopicCtrl_t* topicCtrl);


/**
 *  @brief Publish (send) a message to the MQTT server.
 * 
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  @param topic The topic for the message being sent, the server will resend the msg to other clients subscribed to the topic.
 *  @param qos The quality-of-service for this message (delivery assurance consideration)
 *  @param message The message to send (< 4096 chars)
 *  @param messageSz Size of the message
 *  @param timeoutSec The number of seconds to wait for completion of the send operation.
 *  @return A resultCode_t value indicating the success or type of failure.
*/
resultCode_t mqtt_publish(mqttCtrl_t *mqttCtrl, const char *topic, mqttQos_t qos, const char *message, uint16_t messageSz, uint8_t timeoutSec);


// /**
//  *  @brief Publish (send) a message to the MQTT server.
//  * 
//  *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
//  *  @param topic The topic for the message being sent, the server will resend the msg to other clients subscribed to the topic.
//  *  @param qos The quality-of-service for this message (delivery assurance consideration)
//  *  @param message The message to send (< 560 chars, cannot include " char, should be UUEncoded or Base64 Encoded).
//  *  @param timeoutSec The number of seconds to wait for completion of the send operation.
//  *  @return A resultCode_t value indicating the success or type of failure.
// */
// resultCode_t mqtt_publishDirect(mqttCtrl_t *mqttCtrl, const char *topic, mqttQos_t qos, const char *message, uint8_t timeoutSec);


/**
 *  @brief Disconnect and close a connection to a MQTT server
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
*/
void mqtt_close(mqttCtrl_t *mqttCtrl);


/**
 *  @brief Disconnect and close a connection to a MQTT server
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  @param resetModem [in] True if modem should be reset prior to reestablishing MQTT connection.
*/
void mqtt_reset(mqttCtrl_t *mqttCtrl, bool resetModem);


/**
 *  @brief Get current MQTT connection state
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
*/
mqttState_t mqtt_getStatus(mqttCtrl_t *mqttCtrl);


/**
 *  @brief Query device for current MQTT connection state.
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  @return A mqttState_t value indicating the state of the MQTT connection.
*/
mqttState_t mqtt_fetchStatus(mqttCtrl_t *mqttCtrl);


/**
 *  @brief Get the last outgoing message ID.
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  @returns Integer value of the message identifier.
*/
uint16_t mqtt_getSentMsgId(mqttCtrl_t *mqttCtrl);


/**
 *  @brief Get the last incoming message ID.
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  @returns Integer value of the message identifier.
*/
uint16_t mqtt_getRecvMsgId(mqttCtrl_t *mqttCtrl);


/**
 *  @brief Get the MQTT status error code.
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  @returns Integer value of the MQTT status error code.
*/
uint16_t mqtt_getErrCode(mqttCtrl_t *mqttCtrl);


bool mqtt_recover(mqttCtrl_t *mqtt);


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__MQTT_H__ */
