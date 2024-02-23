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


/* Test is designed for use with HiveMQ. There is a C# host application available to pair with the functionality here.
 * You can get a free HiveMQ Cloud account just by signing up.
 * 
*/
#define MQTT_HOST "e9cb510d802c43dba6a0dd6a20a4c.s1.eu.hivemq.cloud"
#define MQTT_PORT (8883)
#define MQTT_USERNAME "device_ltem_1"
#define MQTT_PASSWORD "rnRGB8j#3R5LX"
#define MQTT_DEVICEID "869084063117729"

#define MQTT_DATACONTEXT (dataCntxt_t)0
#define MQTT_PORT 8883

#define C2D_TOPIC "lq_c2d"
#define D2C_TOPIC "lq_d2c"

/* If set to 1, the receive process will reply to the incoming message. This does not disable the periodic send of messages.
 */
#define ECHO_RECV 1

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
char echoMsg[120];                  // response queue buffer


void setup() {
    #ifdef DIAGPRINT_SERIAL
        Serial.begin(115200);
        #if (SERIAL_OPT > 0)
        while (!Serial) {}      // force wait for serial ready
        #else
        delay(5000);            // just give it some time
        #endif
    #endif

    lqLOG_NOTICE("\rLTEmC MQTT HiveMQ Send/Receive\r\n");

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

    mqttSetup();                                                                // local function with setup actions
    mqtt_start(&mqttCtrl, true);                                                // LTEmC mqtt start function (marshalls connection to MQTT host/hub)

    lastCycle = lqMillis();
}

// #define MQTT_STOP_DEACTIVATE_LIMIT (3)
// #define MQTT_STOP_RESTART_LIMIT (8)

void loop() 
{
    if (IS_ELAPSED(lastCycle, cycle_interval))
    {
        lastCycle = lqMillis();
        loopCnt++;
        double windspeed = random(0, 4999) * 0.01;
        snprintf(mqttMessage, 200, "{ \"loopCnt\": %d,  \"windspeed\": %.2f }", loopCnt, windspeed);

        resultCode_t rslt;
        uint32_t publishTck = lqMillis();

        lqLOG_DBG(lqcWHITE, "Publishing message: %d\r", loopCnt);
        rslt = mqtt_publish(&mqttCtrl, D2C_TOPIC, mqttQos_1, mqttMessage, strlen(mqttMessage), 30);
        if (rslt != resultCode__success)
        {
            lqLOG_DBG(lqcWARN, "Publish Failed! >> %d\r", rslt);
        }
        lqLOG_INFO("\r\nMQTT idle, waiting for next scheduled send\r\n\r\n");
    }

    /* NOTE: The ltem_eventMgr() background pipeline processor is required for async receive operations. Event 
     * manager is light weight and has no side effects other than taking time. It should be invoked liberally. 
     *
     * Message receive for MQTT topic subscriptions require ltem_eventMgr() to process.
     */
    ltem_eventMgr();

    /* Echo received message if present.
    */
    if (strlen(echoMsg))
    {
        if (IS_NOTSUCCESS(mqtt_publish(&mqttCtrl, D2C_TOPIC, mqttQos_1, echoMsg, strlen(echoMsg), 30)))
            lqLOG_INFO("Echo recv failed\r\n");        // simple text message body
        echoMsg[0] = '\0';
    }
}


void mqttSetup()
{
    /* Basic connectivity established, moving on to MQTT setup with Azure IoTHub
     * HiveMQ requires TLS 1.2 with SNI and MQTT version 3.11 (or 5)
     * --------------------------------------------------------------------------------------------
     */
    tls_initControl(&tlsCtrl, tlsVersion_tls12, tlsCipher_default, tlsCertExpiration_default, tlsSecurityLevel_default, true);
    mqtt_initControl(&mqttCtrl, MQTT_DATACONTEXT);
    mqtt_initTopicControl(&topicCtrl, C2D_TOPIC, mqttQos_1, mqttRecvCB, 0);

    mqtt_subscribeTopic(&mqttCtrl, &topicCtrl);
    mqtt_setConnection(&mqttCtrl, MQTT_HOST, MQTT_PORT, &tlsCtrl, mqttVersion_311, MQTT_DEVICEID, MQTT_USERNAME, MQTT_PASSWORD);
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
        lqLOG_DBG(lqcCYAN, "(app:mqttRecvCB) MsgBody=%s\r", dataPtr);

        if (ECHO_RECV)
        {
            snprintf(echoMsg, sizeof(echoMsg), "Recv'd: %s\r\n", dataPtr);

            /* *** Publish CANNOT be initiated from with receiver *** 
             * Queue response, receive is still underway until this callback function returns
             */
            lqLOG_DBG(lqcDARKCYAN, "Got it>> %s (queueing for send)\r\n", echoMsg);
        }
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
