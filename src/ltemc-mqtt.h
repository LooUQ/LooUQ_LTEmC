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

#include "ltemc-internal.h"


enum mqtt__constants
{
    mqtt__useTls = 1,
    mqtt__notUsingTls = 0,
    mqtt__publishTimeout = 7500,

    mqtt__messageSz = 1548,                                             ///< Maximum message size for BGx family (BG96, BG95, BG77)

    mqtt__topic_offset = 24,
    mqtt__topic_nameSz = 90,                                            ///< Azure IoTHub typically 50-70 chars
    mqtt__topic_propsSz = 320,                                          ///< typically 250-300 bytes
    mqtt__topic_publishCmdOvrhdSz = 27,                                 ///< when publishing, number of extra chars in outgoing buffer added to AT cmd
    mqtt__topic_subscriptionCnt = 2,                                    ///< number of slots for MQTT service subscriptions (reduce for mem conservation)
    mqtt__topic_bufferSz = (mqtt__topic_nameSz + mqtt__topic_propsSz),  ///< Total topic size (name+props) for buffer sizing
    mqtt__topic_publishCmdSz = (mqtt__topic_bufferSz + mqtt__topic_publishCmdOvrhdSz),

    mqtt__propertiesCnt = 12                                            ///< Azure IoTHub 3-sysProps, 3-props, plus your application
};

// #define MQTT_PUBLISH_TIMEOUT 2500                                   ///< in millis, BGx documentation says 15s but in practice this is typically
//                                                                     ///< 550ms - 750ms, excessively long timeout creates situation where a msg
//                                                                     ///< timeout indirectly causes a subsequent msg timeout

// #define MQTT_MESSAGE_SZ 1548                                        ///< BGx max publish size
// #define MQTT_TOPIC_OFFSET_MAX 24                                    ///< Number of BGx preamble chars to get to topic

/* MQTT TOPIC
 BGx documentation doesn't state a max for topic length. The information below is
 derived from the topic's construction for AZURE IoTHub connectivity.build
 ------------------------------------------------------------------*/
// #define MQTT_TOPIC_NAME_SZ 90                                       ///< Azure IoTHub typically 50-70 chars
// #define MQTT_TOPIC_PROPS_SZ 320                                     ///< typically 250-300 bytes
// #define MQTT_TOPIC_SZ (MQTT_TOPIC_NAME_SZ + MQTT_TOPIC_PROPS_SZ)    ///< Total topic size (name+props) for buffer sizing
// #define MQTT_TOPIC_PUBCMD_OVRHD_SZ 27                               ///< when publishing, number of extra chars in outgoing buffer added to AT cmd
// #define MQTT_TOPIC_PUBBUF_SZ (MQTT_TOPIC_NAME_SZ + MQTT_TOPIC_PROPS_SZ + MQTT_TOPIC_PUBCMD_OVRHD_SZ)

// #define MQTT_TOPIC_CNT 2                                            ///< number of slots for MQTT service subscriptions (reduce for mem conservation)
// // #define MQTT_SOCKET_ID 5                                            ///< MQTT assigned BGx socket (this is behind-the-scenes and not readily visible)
// #define MQTT_PROPERTIES_CNT 12                                      ///< Azure IoTHub 3-sysProps, 3-props, plus your application


/* Example connection strings key/SAS token
  HostName=iothub-dev-pelogical.azure-devices.net;DeviceId=e8fdd7df-2ca2-4b64-95de-031c6b199299;SharedAccessKey=xx0p0kTA/PIUYCzOncQYWwTyzcrcNuXdQXjlKUBdkc0=
  HostName=iothub-dev-pelogical.azure-devices.net;DeviceId=e8fdd7df-2ca2-4b64-95de-031c6b199299;SharedAccessSignature=SharedAccessSignature sr=iothub-dev-pelogical.azure-devices.net%2Fdevices%2Fe8fdd7df-2ca2-4b64-95de-031c6b199299&sig=JTz%2BmVBf1BRmJr%2FgZiSpzSyHRfo4Uaxkj5POWe4h2YY%3D&se=1592909595
*/

/* Azure IoTHub cloud to device 
    devices/{device_id}/messages/devicebound/#  
    Example: devices/e8fdd7df-2ca2-4b64-95de-031c6b199299/messages/devicebound/#
*/

/** 
 *  \brief Emum describing the results of a MQTT publish.
*/
typedef enum mqttResult_tag
{
    mqttResult_success = 0,         ///< Sucessful publish.
    mqttResult_retransmission = 1,  ///< Publish Sucessful, with retransmission.
    mqttResult_failed = 2           ///< Publish failed.
} mqttResult_t;


/** 
 *  \brief Enum of available MQTT protocol version options.
*/
typedef enum mqttVersion_tag
{
    mqttVersion_3 = 3,              ///< Set behavior to MQTT version 3.0
    mqttVersion_311 = 4             ///< Set behavior to MQTT version 3.1.1, Note: this is not MQTT 4.0
} mqttVersion_t;


/** 
 *  \brief Enum specifying the MQTT QOS for a publish and subscribed topics.
*/
typedef enum mqttQos_tag
{
    mqttQos_0 = 0,                  ///< QOS 0: At most once (aka send and forget)
    mqttQos_1 = 1,                  ///< QOS 1: At least once
    mqttQos_2 = 2,                  ///< QOS 2: Exactly once
    mqttQos_maxAzure = 1            ///< MS Azure IoTHub max QOS (setting to QOS2 will force disconnect)
} mqttQos_t;


/** 
 *  \brief Enum specifying behavior for messages pre-session establishment (session starts with connect, not open).
*/
typedef enum mqttSession_tag
{
    mqttSession_preserve = 0,       ///< Preserve message that arrive previous to session and deliver to application.
    mqttSession_cleanStart = 1,     ///< Clear out any previously received messages and start session "clean".
} mqttSession_t;


/** 
 *  \brief Emum describing current state of the MQTT protocol service.
*/
typedef enum mqttStatus_tag
{
    mqttStatus_closed = 0,          ///< MQTT is idle, not active.
    mqttStatus_invalidHost = 1,
    mqttStatus_open = 2,            ///< MQTT is open, open but not connected.
    mqttStatus_pending = 3,         ///< MQTT is attempting connect (BGx MQTT problem area that needs to be detected, reset BGx if stays here).
    mqttStatus_connected = 4        ///< MQTT is connected, in session with server.
} mqttStatus_t;


// /** 
//  *  \brief typedef of MQTT subscription receiver function (required signature).
// */
// typedef void (*mqtt_recvFunc_t)(char *topic, char *props, char *message);


/** 
 *  \brief Struct describing a MQTT topic subscription.
*/
typedef struct mqttTopicSub_tag
{
    char topicName[mqtt__topic_nameSz];     ///< Topic name. Note if the topic registered with '#' wildcard, this is removed from the topic name.
    char wildcard;                          ///< Set to '#' if multilevel wildcard specified when subscribing to topic.
} mqttTopicSub_t;


// /** 
//  *  \brief Struct describing the MQTT service.
// */
// typedef struct mqtt_tag
// {
//     mqttStatus_t state;                     ///< Current state of the MQTT protocol services on device.
//     uint16_t msgId;                         ///< MQTT in-flight message ID, automatically incremented, rolls at max value.
//     mqttSubscription_t subscriptions[MQTT_TOPIC_MAXCNT];        ///< Array of MQTT topic subscriptions.
//     char *firstChunkBegin;                  ///< BGx MQTT sends data in notification, 1st chunk (~64 chars) will land in cmd buffer (all URCs go there)
//     uint8_t firstChunkSz;                   ///< at recv complete, this chunk needs copied to the start of the data buffer and removed from cmd buffer (IMMEDIATELY)
//                                             ///< struct below is populated when recv buffer is complete and ready
//     // bool recvComplete;                   ///< set within ISR to signal that EOT phrase recv'd and doWork can process into topic/message and deliv to application
//     uint8_t dataBufferIndx;                 ///< index to IOP data buffer holding last completed message (set to IOP_NO_BUF if no recv ready)
// } mqtt_t;

/** 
 *  \brief Callback function for data received event. Notifies application that new data is available and needs serviced.
 * 
 *  The *data and dataSz values are for convenience, since the application supplied the buffer to LTEmC.
 * 
 *  \param peerId [in] Data peer (data context or filesys) 
 *  \param handle [in] Subordinate data ID to give application information about received data
 *  \param data [in] Pointer to received data buffer
 *  \param dataSz [in] The number of bytes available
*/
typedef void (*mqttRecvFunc_t)(dataContext_t dataCntxt, uint16_t msgId, const char *topic, char *topicProps, char *message, uint16_t messageSz);


/** 
 *  \brief Struct representing the state of a MQTT stream service.
*/
typedef struct mqttCtrl_tag
{
    uint8_t ctrlMagic;                                      ///< magic flag to validate incoming requests 
    dataContext_t dataCntxt;                                ///< Data context where this control operates
    protocol_t protocol;                                    ///< Control's protocol : UDP/TCP/SSL, MQTT, HTTP, etc.
    bool useTls;                                            ///< flag indicating SSL/TLS applied to stream
    rxDataBufferCtrl_t recvBufCtrl;                         ///< RX smart buffer 

    mqttRecvFunc_t dataRecvCB;                              ///< callback to application, signals data ready
    mqttVersion_t useMqttVersion;
    mqttStatus_t state;                                     ///< Current state of the MQTT protocol services on device.
    uint16_t msgId;                                         ///< MQTT message ID for QOS, automatically incremented, rolls at max value.
    mqttTopicSub_t topicSubs[mqtt__topic_subscriptionCnt];  ///< Array of MQTT topic subscriptions.
    uint32_t doWorkLastTck;                                 ///< last check for URC/dataPending
    uint16_t lastBufferReqd;                                ///< last receive buffer required size, provides feedback to developer to minimize buffer sizing     
} mqttCtrl_t;


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

//void mqtt_create();
void mqtt_initControl(mqttCtrl_t *mqttCtrl, dataContext_t dataCntxt, bool useTls, mqttVersion_t useMqttVersion, uint8_t *recvBuf, uint16_t recvBufSz, mqttRecvFunc_t recvCallback);

resultCode_t mqtt_open(mqttCtrl_t *mqttCtrl, const char *host, uint16_t port);
resultCode_t mqtt_connect(mqttCtrl_t *mqttCtrl, const char *clientId, const char *username, const char *password, mqttSession_t cleanSession);
void mqtt_close(mqttCtrl_t *mqttCtrl);
uint8_t mqtt_subscribe(mqttCtrl_t *mqttCtrl, const char *topic, mqttQos_t qos);
resultCode_t mqtt_unsubscribe(mqttCtrl_t *mqttCtrl, const char *topic);

resultCode_t mqtt_publish(mqttCtrl_t *mqttCtrl, const char *topic, mqttQos_t qos, const char *message, uint8_t timeoutSeconds);

mqttStatus_t mqtt_getStatus(mqttCtrl_t *mqttCtrl, const char *host);
uint8_t mqtt_getContextState(dataContext_t cntxt, const char *host);
uint16_t mqtt_getMsgId(mqttCtrl_t *mqttCtrl);
uint16_t mqtt_getLastBufferReqd(mqttCtrl_t *mqttCtrl);

#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__MQTT_H__ */
