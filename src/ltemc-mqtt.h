/******************************************************************************
 *  \file ltemc-mqtt.h
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

#ifndef __MQTT_H__
#define __MQTT_H__


#include "ltemc-types.h"

/** 
 *  @brief typed numeric constants used by MQTT subsystem.
*/
enum mqtt__constants
{
    mqtt__useTls = 1,
    mqtt__notUsingTls = 0,
    mqtt__publishTimeout = 15000,

    mqtt__messageSz = 1548,                                             /// Maximum message size for BGx family (BG96, BG95, BG77)
    mqtt__topic_offset = 24,
    mqtt__topic_nameSz = 90,                                            /// Azure IoTHub typically 50-70 chars
    mqtt__topic_propsSz = 320,                                          /// typically 250-300 bytes
    mqtt__topic_subscriptionCnt = 2,                                    /// number of slots for MQTT service subscriptions (reduce for mem conservation)
    mqtt__topicSz = (mqtt__topic_nameSz + mqtt__topic_propsSz),         /// Total topic size (name+props) for buffer sizing
    mqtt__topic_publishCmdOvrhdSz = 27,                                 /// when publishing, number of extra chars in outgoing buffer added to AT cmd

    mqtt__topic_publishCmdSz = (mqtt__topic_nameSz + mqtt__topic_propsSz + mqtt__topic_publishCmdOvrhdSz),

    /* Azure IoTHub Note: 3-system properties, 3-properties, plus application specified properties
    */
    mqtt__propertiesCnt = 12,

    mqtt__clientIdSz = 20,
    mqtt__userNameSz = 100,
    mqtt__userPasswordSz = 200
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
    mqttResult_success = 0,         /// Sucessful publish.
    mqttResult_retransmission = 1,  /// Publish Sucessful, with retransmission.
    mqttResult_failed = 2           /// Publish failed.
} mqttResult_t;


/** 
 *  @brief Enum of available MQTT protocol version options.
*/
typedef enum mqttVersion_tag
{
    mqttVersion_3 = 3,              /// Set behavior to MQTT version 3.0
    mqttVersion_311 = 4             /// Set behavior to MQTT version 3.1.1, Note: this is not MQTT 4.0
} mqttVersion_t;


/** 
 *  @brief Enum specifying the MQTT QOS for a publish and subscribed topics.
*/
typedef enum mqttQos_tag
{
    mqttQos_0 = 0,                  /// QOS 0: At most once (aka send and forget)
    mqttQos_1 = 1,                  /// QOS 1: At least once
    mqttQos_2 = 2,                  /// QOS 2: Exactly once
    mqttQos_maxAzure = 1            /// MS Azure IoTHub max QOS (setting to QOS2 will force disconnect)
} mqttQos_t;


// /** 
//  *  @brief Enum specifying behavior for messages pre-session establishment (session starts with connect, not open).
// */
// typedef enum mqttSession_tag
// {
//     mqttSession_preserve = 0,       /// Preserve message that arrive previous to session and deliver to application.
//     mqttSession_cleanStart = 1,     /// Clear out any previously received messages and start session "clean".
// } mqttSession_t;


/** 
 *  @brief Emum describing current state of the MQTT protocol service.
*/
typedef enum mqttState_tag
{
    mqttState_closed = 0,           /// MQTT is idle, not active.
    mqttState_open = 1,             /// MQTT is open, open but not connected.
    mqttState_connected = 2,        /// MQTT is connected, in session with server.
    mqttState_invalidHost = 201,
    mqttState_pending = 202         /// MQTT is attempting connect (BGx MQTT problem area that needs to be detected, reset BGx if stays here).
} mqttState_t;


/** 
 *  @brief Struct describing a MQTT topic subscription.
*/
typedef struct mqttTopicSub_tag
{
    char topicName[mqtt__topic_nameSz];     /// Topic name. Note if the topic registered with '#' wildcard, this is removed from the topic name.
    char wildcard;                          /// Set to '#' if multilevel wildcard specified when subscribing to topic.
} mqttTopicSub_t;


typedef enum mqttRecvState_tag
{
    mqttRecvState_none = 0,
    mqttRecvState_signalled,
    mqttRecvState_topicDelivered,
    mqttRecvState_msgUnderway
} mqttRecvState_t;


/** 
 *  @brief Struct representing the state of a MQTT stream service.
*/
typedef struct mqttCtrl_tag
{
    uint8_t dataContext;
    mqttState_t state;                                      /// Current state of the MQTT protocol services on device.
    mqttRecvState_t recvState;
    bool useTls;                                            /// flag indicating SSL/TLS applied to stream
    char hostUrl[host__urlSz];                              /// URL or IP address of host
    uint16_t hostPort;                                      /// IP port number host is listening on (allows for 65535/0)
    char clientId[SET_PROPLEN(mqtt__clientIdSz)];
    char username[SET_PROPLEN(mqtt__userNameSz)];
    char password[SET_PROPLEN(mqtt__userPasswordSz)];
    mqttVersion_t mqttVersion;
    uint16_t lastMsgId;                                     /// MQTT message ID for QOS, automatically incremented, rolls at max value.
} mqttCtrl_t;



/** 
 *  @brief Callback function to signal the availability of a received MQTT message.
 * 
 *  @details The host application will need to use mqtt_readTopic(), mqtt_readMessage() to capture the received information, or
 *   use the mqtt_signalComplete() to tell driver to complete the receive and discard any un-read information (release resources).
 *  =============================================================================================================================
 *  @param dataCntxt [in] The data context receiving data.
 *  @param msgId [in] MQTT ID of the message received.
*/
//typedef void (*mqttRecv_func)(uint8_t dataCntxt, uint16_t msgId, const char *topic, char *topicProps, char *message, uint16_t messageSz);
typedef void (*mqttRecv_func)(uint8_t dataCntxt, uint16_t msgId);


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


/**
 *  @brief Initialize a MQTT protocol control structure.
 *  @param mqttCtrl [in] Pointer to MQTT control structure governing communications.
 *  @param dataCntxt [in] Socket/data context to host this protocol stream.
 *  @param recvBuf [in] Pointer to application provided receive data buffer.
 *  @param recvBufSz [in] Size of provided data buffer.
 *  @param recvCallback [in] Callback function to be invoked when received data is ready.
 *  @return A mqtt object to govern operations for this protocol stream value indicating the state of the MQTT connection.
*/
void mqtt_initControl(mqttCtrl_t *mqttCtrl, uint8_t dataCntxt, uint8_t *recvBuf, uint16_t recvBufSz, mqttRecv_func recvCallback);


/**
 *  @brief Set the remote server connection values.
*/
void mqtt_setConnection(mqttCtrl_t *mqttCtrl, const char *hostUrl, uint16_t hostPort, bool useTls, mqttVersion_t useMqttVersion, const char *deviceId, const char *userId, const char *secret);


/**
 *  @brief Open a remote MQTT server for use.
 *  @param mqttCtrl [in] MQTT type stream control to operate on.
 *  @param host [in] The host IP address or name of the remote server.
 *  @param port [in] The IP port number to use for the communications.
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
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  @param topic [in] C-string containing the topic you are subcribing to.
 *  @param qos [in] (enum) Received message qos options for subscribed messages 
 *  @return A resultCode_t value indicating the success or type of failure.
*/
resultCode_t mqtt_subscribe(mqttCtrl_t *mqttCtrl, const char *topic, mqttQos_t qos);


/**
 *  @brief Unsubscribe from a MQTT topic on the server.
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  @param topic [in] C-string containing the topic you are no longer interested in.
 *  @return A resultCode_t value indicating the success or type of failure.
*/
resultCode_t mqtt_unsubscribe(mqttCtrl_t *mqttCtrl, const char *topic);


/**
 *  @brief Publish (send) a message to the MQTT server.
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  @param topic The topic for the message being sent, the server will resend the msg to other clients subscribed to the topic.
 *  @param qos The quality-of-service for this message (delivery assurance consideration)
 *  @param message The message to send (< 4096 chars)
 *  @param timeoutSec The number of seconds to wait for completion of the send operation.
 *  @return A resultCode_t value indicating the success or type of failure.
*/
resultCode_t mqtt_publish(mqttCtrl_t *mqttCtrl, const char *topic, mqttQos_t qos, const char *message, uint8_t timeoutSec);


/**
 *  @brief Publish (send) a message to the MQTT server.
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  @param topic The topic for the message being sent, the server will resend the msg to other clients subscribed to the topic.
 *  @param qos The quality-of-service for this message (delivery assurance consideration)
 *  @param message The message to send (< 560 chars, cannot include " char, should be UUEncoded or Base64 Encoded).
 *  @param timeoutSec The number of seconds to wait for completion of the send operation.
 *  @return A resultCode_t value indicating the success or type of failure.
*/
resultCode_t mqtt_publishEncoded(mqttCtrl_t *mqttCtrl, const char *topic, mqttQos_t qos, const char *message, uint8_t timeoutSec);


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
 *  @brief Disconnect and close a connection to a MQTT server
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  @param resetModem [in] True if modem should be reset prior to reestablishing MQTT connection.
*/
mqttState_t mqtt_getStatus(mqttCtrl_t *mqttCtrl);



/**
 *  @brief Disconnect and close a connection to a MQTT server
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  @param resetModem [in] True if modem should be reset prior to reestablishing MQTT connection.
*/
uint16_t mqtt_getLastSentMsgId(mqttCtrl_t *mqttCtrl);


/**
 *  @brief Read the topic of a received MQTT message.
 *  @details This must be invoked prior to mqtt_readMessage(). 
 * 
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  @param topicBffr [in] pointer to application location where topic of the received message should be copied.
 *  @param bffrSz [in] The size of the buffer. IMPORTANT: MUST BE SUFFICIENT TO RECEIVE FULL TOPIC.
 *  @return True if topic could be copied, false if there is insufficient room (bffr) to copy the full topic.
*/
resultCode_t mqtt_readTopic(mqttCtrl_t *mqttCtrl, char *topicBffr, uint16_t bffrSz);


/**
 *  @brief Disconnect and close a connection to a MQTT server
 *  @note Message recv state is cleared automatically when mqtt_fetchRecvMessage() returns false (no remaining message to deliver).
 * 
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
 *  @param messageBffr [in] pointer to application location where message should be copied.
 *  @param bffrSz [in] The size of the buffer.
 *  @return False if message transfer is complete, true if there is additional message content to transfer.
*/
bool mqtt_readMessage(mqttCtrl_t *mqttCtrl, char *messageBffr, uint16_t bffrSz);


/**
 *  @brief Close out and discard a message receive underway. This clears message state. 
 *  @note This is not required if mqtt_readMessage() has been invoked and it returns false (no remaining message to deliver).
 * 
 *  @param mqttCtrl [in] Pointer to MQTT type stream control to operate on.
*/
void mqtt_recvServiced(mqttCtrl_t *mqttCtrl);


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__MQTT_H__ */
