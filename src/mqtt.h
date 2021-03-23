/******************************************************************************
 *  \file mqtt.h
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
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
 *****************************************************************************/

#ifndef __MQTT_H__
#define __MQTT_H__

#include "ltem1c.h"

#define MQTT_PUBLISH_TIMEOUT 2500                                   ///< in millis, BGx documentation says 15s but in practice this is typically
                                                                    ///< 550ms - 750ms, excessively long timeout creates situation where a msg
                                                                    ///< timeout, causes a subsequent msg timeout

#define MQTT_MESSAGE_SZ 1548                                        ///< BGx max publish size
#define MQTT_TOPIC_OFFSET_MAX 24                                    ///< Number of BGx preamble chars to get to topic

/* MQTT TOPIC
 BGx documentation doesn't state a max for topic length. The information below is
 derived from the topic's construction for AZURE IoTHub connectivity.build
 ------------------------------------------------------------------*/
#define MQTT_TOPIC_NAME_SZ 90                                       ///< Azure IoTHub typically 50-70 chars
#define MQTT_TOPIC_PROPS_SZ 320                                     ///< typically 250-300 bytes
#define MQTT_TOPIC_SZ (MQTT_TOPIC_NAME_SZ + MQTT_TOPIC_PROPS_SZ)    ///< Total topic size (name+props) for buffer sizing
#define MQTT_TOPIC_PUBCMD_OVRHD_SZ 27                               ///< when publishing, number of extra chars in outgoing buffer added to AT cmd
#define MQTT_TOPIC_PUBBUF_SZ (MQTT_TOPIC_NAME_SZ + MQTT_TOPIC_PROPS_SZ + MQTT_TOPIC_PUBCMD_OVRHD_SZ)

#define MQTT_TOPIC_MAXCNT 2                                         ///< number of slots for MQTT service subscriptions (reduce for mem conservation)
#define MQTT_SOCKET_ID 5                                            ///< MQTT assigned BGx socket (this is behind-the-scenes and not readily visible)
#define MQTT_PROPERTIES_CNT 12                                      ///< Azure IoTHub 3-sysProps, 3-props, plus your application


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
 *  \brief Enum of available SSL version options for an SSL connection. 
*/
typedef enum sslVersion_tag 
{
    sslVersion_none = 255,          ///< Not set
    sslVersion_ssl30 = 0,           ///< Require SSL v3.0 
    sslVersion_tls10 = 1,           ///< Require TLS v1.0
    sslVersion_tls11 = 2,           ///< Require TLS v1.1
    sslVersion_tls12 = 3,           ///< Require TLS v1.2
    sslVersion_any = 4              ///< Any SSL/TLS version is acceptable.
} sslVersion_t;


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
    mqttStatus_open = 1,            ///< MQTT is open, open but not connected.
    mqttStatus_connected = 2        ///< MQTT is connected, in session with server.
} mqttStatus_t;


/** 
 *  \brief typedef of MQTT subscription receiver function (required signature).
*/
typedef void (*mqtt_recvFunc_t)(char *topic, char *props, char *message);


/** 
 *  \brief Struct describing a MQTT topic subscription.
*/
typedef struct mqttSubscription_tag
{
    char topicName[MQTT_TOPIC_NAME_SZ];     ///< Topic name. Note if the topic registered with '#' wildcard, this is removed from the topic name.
    char wildcard;                          ///< Set to '#' if multilevel wildcard specified when subscribing to topic.
    mqtt_recvFunc_t receiver_func;          ///< Function to receive incoming messages (event). Note that receiver_func can be unique or shared amongst subscriptions.
} mqttSubscription_t;


/** 
 *  \brief Struct describing the MQTT service.
*/
typedef struct mqtt_tag
{
    mqttStatus_t state;                     ///< Current state of the MQTT protocol services on device.
    uint16_t msgId;                         ///< MQTT in-flight message ID, automatically incremented, rolls at max value.
    mqttSubscription_t subscriptions[MQTT_TOPIC_MAXCNT];        ///< Array of MQTT topic subscriptions.
    char *firstChunkBegin;                  ///< BGx MQTT sends data in notification, 1st chunk (~64 chars) will land in cmd buffer (all URCs go there)
    uint8_t firstChunkSz;                   ///< at recv complete, this chunk needs copied to the start of the data buffer and removed from cmd buffer (IMMEDIATELY)
                                            ///< struct below is populated when recv buffer is complete and ready
    // bool recvComplete;                   ///< set within ISR to signal that EOT phrase recv'd and doWork can process into topic/message and deliv to application
    uint8_t dataBufferIndx;                 ///< index to IOP data buffer holding last completed message (set to IOP_NO_BUF if no recv ready)
} mqtt_t;

typedef mqtt_t *mqttPtr_t;


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

//mqtt_t *mqtt_create();
void mqtt_create();

mqttStatus_t mqtt_status(const char *host, bool force);
resultCode_t mqtt_open(const char *host, uint16_t port, sslVersion_t useSslVersion, mqttVersion_t useMqttVersion);
resultCode_t mqtt_connect(const char *clientId, const char *username, const char *password, mqttSession_t clean);
void mqtt_close();


resultCode_t mqtt_subscribe(const char *topic, mqttQos_t qos, mqtt_recvFunc_t rcvr_func);
resultCode_t mqtt_unsubscribe(const char *topic);
resultCode_t mqtt_publish(const char *topic, mqttQos_t qos, const char *message);

void mqtt_doWork();


#ifdef __cplusplus
}
#endif // !__cplusplus


#endif  /* !__MQTT_H__ */
