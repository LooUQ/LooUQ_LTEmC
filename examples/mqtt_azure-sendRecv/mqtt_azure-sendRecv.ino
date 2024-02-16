/******************************************************************************
 *  \file mqtt-azure.ino
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020-2022 LooUQ Incorporated.
 *  www.loouq.com
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
 * Test MQTT protocol client send/receive with Azure IoTHub.
 *****************************************************************************/

#include <lq-embed.h>
#define lqLOG_LEVEL lqLOGLEVEL_DBG
//#define DISABLE_ASSERT                                    // ASSERT/_W enabled by default, can be disabled 
//#define ASSERT_ACTION_STOP                                // ASSERTS can be configured to stop at while(){}


/* specify the pin configuration 
 * --------------------------------------------------------------------------------------------- */
#ifdef ARDUINO_ARCH_ESP32
    #define HOST_ESP32_DEVMOD_BMS
#else
    #define HOST_FEATHER_UXPLOR_L
    // #define HOST_FEATHER_UXPLOR             
    // #define HOST_FEATHER_LTEM3F
#endif

#define PDP_DATA_CONTEXT 1
#define PDP_APN_NAME ""

#include <ltemc.h>
#include <lq-collections.h>             // using LooUQ collections with mqtt
#include <lq-str.h>
#include <ltemc-tls.h>
#include <ltemc-mqtt.h>


/* Test is designed for use with Azure IoTHub. If you do not have a Azure subscription you can get a free test account with a preloaded credit.
 * Optionally you can request a trial account for LooUQ's LQ Cloud, which uses Azure IoTHub for device data ingress.
 * 
 * LQ Cloud or Azure IoTHub needs 3 provisioning elements: 
 *   -- The hub address, if you are using LQ Cloud for testing the MQTT_IOTHUB address below is valid. Otherwise supply your MQTT access point (no protocol prefix)
 *   -- A deviceId and 
 *   -- A SAS token (a time-limited password). For deviceId we recommend the modem's IMEI
 * 
 * The Device ID is up to 40 characters long and we suggest using the modem's IMEI. An example of the SAS token is shown on the next line.  
 * 
 * "SharedAccessSignature sr=iothub-dev-pelogical.azure-devices.net%2Fdevices%2Fe8fdd7df-2ca2-4b64-95de-031c6b199299&sig=XbjrqvX4kQXOTefJIaw86jRhfkv1UMJwK%2FFDiWmfqFU%3D&se=1759244275"
 *
 * Your values will be different, update MQTT_IOTHUB_DEVICEID and MQTT_IOTHUB_SASTOKEN with your values. If you obtain the SAS Token from Azure IoT Explorer it will
 * be prefixed with additional information: Hostname, DeviceId and the SharedAccessSignature property keys, these must be removed if using MQTT directly.
 * 
 * HostName=iothub-dev-pelogical.azure-devices.net;DeviceId=867198053158865;SharedAccessSignature=SharedAccessSignature sr=iothub-dev-pelogical.azure-devices.net%2Fdevices%2F867198053158865&sig=YlW6PuH4MA93cINEhgstihwQxWt4Zr9iY9Tnkl%2BuQK4%3D&se=1608052225
 * ^^^                          REMOVE, NOT PART OF THE SAS TOKEN                              ^^^              KEEP ALL FOLLOWING '='

    https://learn.microsoft.com/en-us/azure/iot-hub/iot-hub-mqtt-support
*/

/* put your AZ IoTHub info here, SAS Token is available by using the "Azure IoT explorer" (https://github.com/Azure/azure-iot-explorer/releases/tag/v0.10.16)
 * Note that SAS tokens have an experiry from 5 minutes to years. Yes... the one below is expired. 
 * To cancel a published SAS Token with an extended expirery you will need to change the device key (primary or secondary) it was based on.
 * 
 * If you are a LQCloud user get your SAS token from the Devices..Device Details panel.
 */

#define MQTT_DATACONTEXT (dataCntxt_t)0
#define MQTT_PORT 8883

#define MQTT_IOTHUB "iothub-dev-pelogical.azure-devices.net"
#define MQTT_IOTHUB_DEVICEID "BMS-GT-DEV-8944502210209831154"
#define MQTT_IOTHUB_USERID MQTT_IOTHUB "/" MQTT_IOTHUB_DEVICEID "/?api-version=2021-04-12"
#define MQTT_IOTHUB_SASTOKEN "SharedAccessSignature sr=iothub-dev-pelogical.azure-devices.net%2Fdevices%2FBMS-GT-DEV-8944502210209831154&sig=gwj%2FV5ZTunVBDPJ6zdzy7U7B27YRmk1VgyO9mbkCw%3D&se=1734125900"

// Macros for simplified IoTHub access
#define MQTT_IOTHUB_D2C_TOPIC "devices/" MQTT_IOTHUB_DEVICEID "/messages/events/"
#define MQTT_IOTHUB_C2D_TOPIC "devices/" MQTT_IOTHUB_DEVICEID "/messages/devicebound/#"
#define MQTT_MSG_PROPERTIES "mId=~%d&mV=1.0&mTyp=tdat&evC=user&evN=wind-telemetry&evV=Wind Speed:18.97"
#define MQTT_MSG_BODY_TEMPLATE "devices/" MQTT_IOTHUB_DEVICEID "/messages/events/mId=~%d&mV=1.0&mTyp=tdat&evC=user&evN=wind-telemetry&evV=Wind Speed:%0.2f"


// test setup
uint16_t cycle_interval = 15000;
uint16_t loopCnt = 0;
uint32_t lastCycle;


// LTEm variables
tlsCtrl_t tlsCtrl;
mqttCtrl_t mqttCtrl;                // MQTT control, data to manage MQTT connection to server
mqttTopicCtrl_t topicCtrl;

char mqttTopic[200];                // application buffer to craft TX MQTT topic
char mqttTopicProp[200];
char mqttMessage[200];              // application buffer to craft TX MQTT publish content (body)
resultCode_t result;


void setup() {
    #ifdef DIAGPRINT_SERIAL
        Serial.begin(115200);
        #if (SERIAL_OPT > 0)
        while (!Serial) {}      // force wait for serial ready
        #else
        delay(5000);            // just give it some time
        #endif
    #endif

    lqLOG_NOTICE("\rLTEmC MQTT Azure IoTHub Send/Receive\r\n");

    ltem_create(ltem_pinConfig, NULL, applEvntNotify);
    ntwk_setDefaultNetwork(PDP_DATA_CONTEXT, pdpProtocol_IPV4, PDP_APN_NAME);
    if(!ltem_start(resetAction_swReset))
        lqLOG_WARN("LTEm failed to start completely. Examine logs for details.");

    lqLOG_INFO("Waiting on network...\r");
    ntwkOperator_t *provider = ntwk_awaitOperator(PERIOD_FROM_SECONDS(15));
    while (strlen(provider->name) == 0)
    {
        lqLOG_DBG(lqcDARKYELLOW, ">");
    }
    lqLOG_DBG(lqcINFO, "Network type is %s on %s\r", provider->iotMode, provider->name);

    /* Basic connectivity established, moving on to MQTT setup with Azure IoTHub
     * Azure requires TLS 1.2 and MQTT version 3.11 
     * --------------------------------------------------------------------------------------------
     */
    tls_initControl(&tlsCtrl, tlsVersion_tls12, tlsCipher_default, tlsCertExpiration_default, tlsSecurityLevel_default, true);
    mqtt_initControl(&mqttCtrl, MQTT_DATACONTEXT);
    mqtt_initTopicControl(&topicCtrl, MQTT_IOTHUB_C2D_TOPIC, mqttQos_1, mqttRecvCB, 0);

    mqtt_subscribeTopic(&mqttCtrl, &topicCtrl);
    mqtt_setConnection(&mqttCtrl, MQTT_IOTHUB, MQTT_PORT, &tlsCtrl, mqttVersion_311, MQTT_IOTHUB_DEVICEID, MQTT_IOTHUB_USERID, MQTT_IOTHUB_SASTOKEN);
    mqtt_start(&mqttCtrl, true);

    lastCycle = pMillis();
}

#define MQTT_STOP_DEACTIVATE_LIMIT (3)
#define MQTT_STOP_RESTART_LIMIT (8)

void loop() 
{
    if (pElapsed(lastCycle, cycle_interval))
    {
        lastCycle = pMillis();
        loopCnt++;
        double windspeed = random(0, 4999) * 0.01;

        snprintf(mqttTopic, 200, MQTT_MSG_BODY_TEMPLATE, loopCnt, windspeed);
        snprintf(mqttMessage, 200, "MQTT message for loop=%d", loopCnt);

        resultCode_t rslt;
        uint32_t publishTck = pMillis();

        lqLOG_DBG(lqcWHITE, "Publishing message: %d\r", loopCnt);
        rslt = mqtt_publish(&mqttCtrl, mqttTopic, mqttQos_1, mqttMessage, strlen(mqttMessage), 30);
        if (rslt != resultCode__success)
        {
            lqLOG_DBG(lqcWARN, "Publish Failed! >> %d\r", rslt);
        }

        lqLOG_INFO("\r\nMQTT idle, waiting for next send\r\n\r\n");
    }

    /* NOTE: The ltem_eventMgr() background pipeline processor is required for async receive operations. Event 
     * manager is light weight and has no side effects other than taking time. It should be invoked liberally. 
     *
     * Message receive for MQTT topic subscriptions require ltem_eventMgr() to process.
     */
    ltem_eventMgr();
}


void mqttSetup()
{
    /* Basic connectivity established, moving on to MQTT setup with Azure IoTHub
     * Azure requires TLS 1.2 and MQTT version 3.11 
     * --------------------------------------------------------------------------------------------
     */
    // tls_configure(dataCntxt_0, tlsVersion_tls12, tlsCipher_default, tlsCertExpiration_default, tlsSecurityLevel_default);
    tls_initControl(&tlsCtrl, tlsVersion_tls12, tlsCipher_default, tlsCertExpiration_default, tlsSecurityLevel_default, true);

    mqtt_initControl(&mqttCtrl, MQTT_DATACONTEXT);
    mqtt_initTopicControl(&topicCtrl, MQTT_IOTHUB_C2D_TOPIC, mqttQos_1, mqttRecvCB, 0);

    //mqtt_subscribeTopic(&mqttCtrl, &topicCtrl);
    mqtt_setConnection(&mqttCtrl, MQTT_IOTHUB, MQTT_PORT, &tlsCtrl, mqttVersion_311, MQTT_IOTHUB_DEVICEID, MQTT_IOTHUB_USERID, MQTT_IOTHUB_SASTOKEN);
}


void mqttRecvCB(dataCntxt_t dataCntxt, uint16_t msgId, mqttMsgSegment_t segment, char* dataPtr, uint16_t dataSz, bool isFinal)
{
    lqLOG_DBG(lqDARKCYAN, "AppRcv: context=%d, msgId=%d, segment=%d, blockPtr=%p, blockSz=%d, isFinal=%d\r", dataCntxt, msgId, segment, dataPtr, dataSz, isFinal);

    if (segment == mqttMsgSegment_topic)
    {
        dataPtr[dataSz] = '\0';
        lqLOG_DBG(lqCYAN, "(app:mqttRecvCB) Topic=%s\r", dataPtr);
    }
    else if (segment == mqttMsgSegment_topicExt)
    {
        dataPtr[dataSz] = '\0';
        lqLOG_DBG(lqCYAN, "(app:mqttRecvCB) TopicExt=%s\r", dataPtr);
        char propsBffr[500];
        strcpy(propsBffr, dataPtr);

        // Azure IoTHub appends properties collection to the topic 
        // That is why Azure requires wildcard topic
        keyValueDict_t mqttProps = lq_createQryStrDictionary(propsBffr, strlen(propsBffr));
        lqLOG_DBG(lqcINFO, "Props(%d)\r", mqttProps.count);
        for (size_t i = 0; i < mqttProps.count; i++)
        {
            lqLOG_DBG(lqcCYAN, "%s=%s\r", mqttProps.keys[i], mqttProps.values[i]);
        }
        lqLOG_DBG(0, "\r");
    }
    else if (segment == mqttMsgSegment_msgBody)
    {
        dataPtr[dataSz] = '\0';
        lqLOG_DBG(lqCYAN, "(app:mqttRecvCB) MsgBody=%s\r", dataPtr);
    }

}



/* test helpers
========================================================================================================================= */

void applEvntNotify(appEvent_t eventType, const char *notifyMsg)
{
    if (eventType == appEvent_fault_assertFailed)
        lqLOG_DBG(lqERROR, "LTEmC-HardFault: %s\r", notifyMsg);
    else 
        lqLOG_DBG(lqWHITE, "LTEmC Info: %s\r", notifyMsg);
    return;
}
