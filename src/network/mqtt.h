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

#include "..\ltem1c.h"
#include "network.h"

#define MQTT_PUBMSG_MAXSZ 1549


typedef struct mqtt_tag
{
    uint16_t msgId[LTEM1_SOCKET_COUNT];
} mqtt_t;


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


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


void mqtt_create();
void mqtt_destroy();

socketId_t mqtt_open(const char *host, uint16_t port, sslVersion_t useSslVersion, mqttVersion_t useMqttVersion);
socketResult_t mqtt_connect(socketId_t socketId, const char *clientId, const char *username, const char *password);
void mqtt_close(socketId_t socketId);

socketResult_t mqtt_subscribe(socketId_t socketId, const char *topic, uint8_t qos);
socketResult_t mqtt_unsubscribe(socketId_t socketId);
socketResult_t mqtt_publish(socketId_t socketId, const char *topic, const char *message);


#ifdef __cplusplus
}
#endif // !__cplusplus


#endif  /* !__MQTT_H__ */
