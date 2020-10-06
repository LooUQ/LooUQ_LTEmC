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


#define MQTT_MESSAGE_SZ 1548
#define MQTT_TOPIC_OFFSET_MAX 24
#define MQTT_TOPIC_NAME_SZ 120
#define MQTT_TOPIC_PROPS_SZ 120
#define MQTT_TOPIC_TOTAL_SZ (MQTT_TOPIC_NAME_SZ + MQTT_TOPIC_PROPS_SZ)
#define MQTT_TOPIC_PUBOVRHD_SZ 27
#define MQTT_TOPIC_MAXCNT 3
#define MQTT_SOCKET_ID 5
#define MQTT_PROPERTIES_CNT 8


/* Example connection strings key/SAS token
  HostName=iothub-dev-pelogical.azure-devices.net;DeviceId=e8fdd7df-2ca2-4b64-95de-031c6b199299;SharedAccessKey=xx0p0kTA/PIUYCzOncQYWwTyzcrcNuXdQXjlKUBdkc0=
  HostName=iothub-dev-pelogical.azure-devices.net;DeviceId=e8fdd7df-2ca2-4b64-95de-031c6b199299;SharedAccessSignature=SharedAccessSignature sr=iothub-dev-pelogical.azure-devices.net%2Fdevices%2Fe8fdd7df-2ca2-4b64-95de-031c6b199299&sig=JTz%2BmVBf1BRmJr%2FgZiSpzSyHRfo4Uaxkj5POWe4h2YY%3D&se=1592909595
*/

/* Azure IoTHub cloud to device 
    devices/{device_id}/messages/devicebound/#  
    Example: devices/e8fdd7df-2ca2-4b64-95de-031c6b199299/messages/devicebound/#
*/


typedef enum mqttResult_tag
{
    mqttResult_success = 0,
    mqttResult_retransmission = 1,
    mqttResult_failed = 2
} mqttResult_t;


typedef enum sslVersion_tag
{
    sslVersion_none = 255,
    sslVersion_ssl30 = 0,
    sslVersion_tls10 = 1,
    sslVersion_tls11 = 2,
    sslVersion_tls12 = 3,
    sslVersion_any = 4
} sslVersion_t;


typedef enum mqttVersion_tag
{
    mqttVersion_3 = 3,
    mqttVersion_311 = 4
} mqttVersion_t;

typedef enum mqttQos_tag
{
    mqttQos_0 = 0,
    mqttQos_1 = 1,
    mqttQos_2 = 2,
    mqttQos_maxAzure = 1
} mqttQos_t;


typedef enum mqttSession_tag
{
    mqttSession_preserve = 0,
    mqttSession_cleanStart = 1,
} mqttSession_t;


typedef enum mqttStatus_tag
{
    mqttStatus_idle = 0,
    mqttStatus_open = 1,
    mqttStatus_conn = 2
} mqttStatus_t;


typedef struct mqttMsgProps_tag
{
    uint8_t count;
    char *names[MQTT_PROPERTIES_CNT];
    char *values[MQTT_PROPERTIES_CNT];
} mqttMsgProps_t;



typedef void (*mqttRecv_func_t)(char *topic, char *props, char *message);


typedef struct mqttSubscription_tag
{
    char topicName[MQTT_TOPIC_NAME_SZ];
    char wildcard;
    mqttRecv_func_t receiver_func;
} mqttSubscription_t;


typedef struct mqtt_tag
{
    mqttStatus_t state;
    uint16_t msgId;
    mqttSubscription_t subscriptions[MQTT_TOPIC_MAXCNT];
    char *firstChunkBegin;                  // BGx MQTT sends data in notification, 1st chunk (~64 chars) will land in cmd buffer (all URCs go there)
    uint8_t firstChunkSz;                   // at recv complete, this chunk needs copied to the start of the data buffer and removed from cmd buffer (IMMEDIATELY)
                                            // struct below is populated when recv buffer is complete and ready
    // bool recvComplete;                      // set within ISR to signal that EOT phrase recv'd and doWork can process into topic/message and deliv to application
    uint8_t dataBufferIndx;                 // index to IOP data buffer holding last completed message (set to IOP_NO_BUF if no recv ready)
} mqtt_t;



#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


void mqtt_create();

mqttStatus_t mqtt_status(const char *host);
socketResult_t mqtt_open(const char *host, uint16_t port, sslVersion_t useSslVersion, mqttVersion_t useMqttVersion);
socketResult_t mqtt_connect(const char *clientId, const char *username, const char *password, mqttSession_t clean);
void mqtt_close();


socketResult_t mqtt_subscribe(const char *topic, mqttQos_t qos, mqttRecv_func_t rcvr_func);
socketResult_t mqtt_unsubscribe(const char *topic);
socketResult_t mqtt_publish(const char *topic, mqttQos_t qos, const char *message);

void mqtt_doWork();

mqttMsgProps_t mqtt_parseTopicProperties(char *topicProps);


#ifdef __cplusplus
}
#endif // !__cplusplus


#endif  /* !__MQTT_H__ */
