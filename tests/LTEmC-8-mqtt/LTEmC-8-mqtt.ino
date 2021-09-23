/******************************************************************************
 *  \file LTEmC-8-mqtt.ino
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
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
 * Test MQTT protocol client send/receive. 
 * Uses Azure IoTHub and the LooUQ Cloud as server side
 * 
 * The sketch is designed for debug output to observe results.
 *****************************************************************************/

#define _DEBUG 2                        // set to non-zero value for PRINTF debugging output, 
// debugging output options             // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if defined(_DEBUG)
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #define PRINTF(c_,f_,__VA_ARGS__...) do { rtt_printf(c_, (f_), ## __VA_ARGS__); } while(0)
    #else
    #define SERIAL_DBG _DEBUG           // enable serial port output using devl host platform serial, _DEBUG 0=start immediately, 1=wait for port
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#endif

                                        // define options for how to assemble this build
#define HOST_FEATHER_UXPLOR             // specify the pin configuration

#include <ltemc.h>
#include <lq-diagnostics.h>
#include <lq-collections.h>             // using LooUQ collections with mqtt
#include <lq-str.h>
#include <ltemc-tls.h>
#include <ltemc-mqtt.h>

#define DEFAULT_NETWORK_CONTEXT 1
#define PERIOD_FROM_SECONDS(period)  (period * 1000)
#define PERIOD_FROM_MINUTES(period)  (period * 1000 * 60)



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
*/

#define MQTT_IOTHUB "iothub-dev-pelogical.azure-devices.net"
#define MQTT_PORT 8883
#define MQTT_DATACONTEXT 5

/* put your AZ IoTHub info here, SAS Token is available by using the "Azure IoT explorer" (https://github.com/Azure/azure-iot-explorer/releases/tag/v0.10.16)
 * Note that SAS tokens have an experiry from 5 minutes to years. Yes... the one below is expired. 
 * To cancel a published SAS Token with an extended expirery you will need to change the device key (primary or secondary) it was based on.
 * 
 * If you are a LQCloud user get your SAS token from the Devices..Device Details panel.
 */
#define MQTT_IOTHUB_DEVICEID "867198053224766"
#define MQTT_IOTHUB_USERID "iothub-dev-pelogical.azure-devices.net/" MQTT_IOTHUB_DEVICEID "/?api-version=2018-06-30"

//#define MQTT_IOTHUB_SASTOKEN "SharedAccessSignature sr=iothub-dev-pelogical.azure-devices.net%2Fdevices%2F" MQTT_IOTHUB_DEVICEID "&sig=J0S6wK5yFEFcUcFpP84GRGeB%2FrIZRgjssfRs09kTeq0%3D&se=1624376687"
#define MQTT_IOTHUB_SASTOKEN "SharedAccessSignature sr=iothub-dev-pelogical.azure-devices.net%2Fdevices%2F867198053224766&sig=zlkmqXDdb9ebeRBOMssHj0XHOSllIpXc5zKdBFgSTec%3D&se=1628439070"

#define MQTT_IOTHUB_D2C_TOPIC "devices/" MQTT_IOTHUB_DEVICEID "/messages/events/"
#define MQTT_IOTHUB_C2D_TOPIC "devices/" MQTT_IOTHUB_DEVICEID "/messages/devicebound/#"
#define MQTT_MSG_PROPERTIES "mId=~%d&mV=1.0&mTyp=tdat&evC=user&evN=wind-telemetry&evV=Wind Speed:18.97"
#define MQTT_MSG_BODY_TEMPLATE "devices/" MQTT_IOTHUB_DEVICEID "/messages/events/mId=~%d&mV=1.0&mTyp=tdat&evC=user&evN=wind-telemetry&evV=Wind Speed:%0.2f"


// test setup
uint16_t cycle_interval = 15000;
uint16_t loopCnt = 0;
uint32_t lastCycle;


// LTEm variables
mqttCtrl_t mqttCtrl;                // MQTT control, data to manage MQTT connection to server
uint8_t receiveBuffer[640];         // Data buffer where received information is returned (can be local, global, or dynamic... your call)

char mqttTopic[200];                // application buffer to craft TX MQTT topic
char mqttMessage[200];              // application buffer to craft TX MQTT publish content (body)
resultCode_t result;


void setup() {
    #ifdef SERIAL_OPT
        Serial.begin(115200);
        #if (SERIAL_OPT > 0)
        while (!Serial) {}      // force wait for serial ready
        #else
        delay(5000);            // just give it some time
        #endif
    #endif

    PRINTF(dbgColor__red, "\rLTEm1c test8-MQTT\r\n");
    lqDiag_registerNotifCallback(appNotifyCB);

    diagnosticInfo_t *diags = lqDiag_getDiagnosticsInfo();

    ltem_create(ltem_pinConfig, appNotifyCB);
    ltem_start();

    PRINTF(dbgColor__none, "Waiting on network...\r");
    networkOperator_t networkOp = ntwk_awaitOperator(30000);
    if (strlen(networkOp.operName) == 0)
        appNotifyCB(255, 0, 0, "Timout (30s) waiting for cellular network.");
    PRINTF(dbgColor__info, "Network type is %s on %s\r", networkOp.ntwkMode, networkOp.operName);

    uint8_t cntxtCnt = ntwk_fetchActivePdpCntxts();
    if (cntxtCnt == 0)
    {
        ntwk_activatePdpContext(DEFAULT_NETWORK_CONTEXT, pdpCntxtProtocolType_IPV4, "");
    }

    /* Basic connectivity established, moving on to MQTT setup with Azure IoTHub
     * Azure requires TLS 1.2 and MQTT version 3.11 */

    tls_configure(dataContext_5, tlsVersion_tls12, tlsCipher_default, tlsCertExpiration_default, tlsSecurityLevel_default);
    mqtt_initControl(&mqttCtrl, dataContext_5, mqtt__useTls, mqttVersion_311, receiveBuffer, sizeof(receiveBuffer), mqttRecvCB);

    resultCode_t rslt;

    rslt = mqtt_open(&mqttCtrl, MQTT_IOTHUB, MQTT_PORT);
    if (rslt != resultCode__success)
    {
        PRINTF(dbgColor__warn, "Open fail status=%d\r", rslt);
        appNotifyCB(255, 0, 0, "MQTT open failed");
    }
    rslt = mqtt_connect(&mqttCtrl, MQTT_IOTHUB_DEVICEID, MQTT_IOTHUB_USERID, MQTT_IOTHUB_SASTOKEN, mqttSession_cleanStart);
    if (rslt != resultCode__success)
    {
        PRINTF(dbgColor__warn, "Connect fail status=%d\r", rslt);
        appNotifyCB(255, 0, 0, "MQTT connect failed.");
    }
    rslt = mqtt_subscribe(&mqttCtrl, MQTT_IOTHUB_C2D_TOPIC, mqttQos_1);
    if (rslt != resultCode__success)
    {
        PRINTF(dbgColor__warn, "Subscribe fail status=%d\r", rslt);
        appNotifyCB(255, 0, 0, "MQTT subscribe to IoTHub C2D messages failed.");
    }
    lastCycle = pMillis() - cycle_interval;
}


void loop() 
{
    if (pElapsed(lastCycle, cycle_interval))
    {
        lastCycle = pMillis();

        double windspeed = random(0, 4999) * 0.01;

        snprintf(mqttTopic, 200, MQTT_MSG_BODY_TEMPLATE, loopCnt, windspeed);
        snprintf(mqttMessage, 200, "MQTT message for loop=%d", loopCnt);

        resultCode_t rslt;

        // /* no retry */
        // rslt = mqtt_publish(&mqttCtrl, mqttTopic, mqttQos_1, mqttMessage);

        // if (rslt != resultCode__success)
        //     PRINTF(dbgColor__warn, "Publish failed!\r");


        /* retry until timeout */
        uint32_t publishTck = pMillis();
        do
        {
            rslt = mqtt_publish(&mqttCtrl, mqttTopic, mqttQos_1, mqttMessage);
            if (pElapsed(publishTck, PERIOD_FROM_SECONDS(1)))
            {
                PRINTF(dbgColor__warn, "Publish attempts timed out, Publish Failed!\r");
                break;
            }
            /* if the publish is being blocked by a recv, critical that doWork() given opportunity to finish IO */
            ltem_doWork();                                                              

        } while (rslt != resultCode__success);

        loopCnt++;
        PRINTF(dbgColor__magenta, "\rFreeMem=%u  <<Loop=%d>>\r", getFreeMemory(), loopCnt);
    }

    /* NOTE: ltem1_doWork() pipeline requires up to 3 invokes for each data receive. DoWork has no side effects 
     * other than taking time and should be invoked liberally. */
    ltem_doWork();
}


void mqttRecvCB(dataContext_t cntxt, uint16_t msgId, const char *topic, char *topicVar, char *message, uint16_t messageSz)
{
    uint16_t newLen = lq_strUriDecode(topicVar, message - topicVar);        // Azure IoTHub URI encodes properties sent as topic suffix 

    PRINTF(dbgColor__info, "\r**MQTT--MSG** @tick=%d BufferSz=%d\r", pMillis(), mqtt_getLastBufferReqd(&mqttCtrl));
    PRINTF(dbgColor__cyan, "   msgId:=%d   topicSz=%d, propsSz=%d, messageSz=%d\r", msgId, strlen(topic), strlen(topicVar), strlen(message));
    PRINTF(dbgColor__cyan, "   topic: %s\r", topic);
    PRINTF(dbgColor__cyan, "   props: %s\r", topicVar);
    PRINTF(dbgColor__cyan, " message: %s\r", message);

    // Azure IoTHub appends properties collection to the topic (that is why Azure requires wildcard topic)
    keyValueDict_t mqttProps = lq_createQryStrDictionary(topicVar, strlen(topicVar));
    PRINTF(dbgColor__info, "Props(%d)\r", mqttProps.count);
    for (size_t i = 0; i < mqttProps.count; i++)
    {
        PRINTF(dbgColor__cyan, "%s=%s\r", mqttProps.keys[i], mqttProps.values[i]);
    }
    PRINTF(0, "\r");
}



/* test helpers
========================================================================================================================= */

void appNotifyCB(uint8_t notifType, uint8_t assm, uint8_t inst, const char *notifMsg)
{
    if (notifType >= lqNotifType__CATASTROPHIC)
    {
        PRINTF(dbgColor__error, "\r\n** %s \r\n", notifMsg);
        volatile int halt = 1;
        while (halt) {}
    }
    else if (notifType >= lqNotifType__WARNING)
        PRINTF(dbgColor__warn, "\r\n** %s \r\n", notifMsg);
    else
        PRINTF(dbgColor__info, "\r\n%s \r\n", notifMsg);
}


/* Check free memory (stack-heap) 
 * - Remove if not needed for production
--------------------------------------------------------------------------------- */

#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__

int getFreeMemory() 
{
    char top;
    #ifdef __arm__
    return &top - reinterpret_cast<char*>(sbrk(0));
    #elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
    return &top - __brkval;
    #else  // __arm__
    return __brkval ? &top - __brkval : &top - __malloc_heap_start;
    #endif  // __arm__
}

