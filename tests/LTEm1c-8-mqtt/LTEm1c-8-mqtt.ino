/******************************************************************************
 *  \file LTEm1c-8-mqtt.ino
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
 *****************************************************************************/

// define options for how to assemble this build
#define HOST_FEATHER_UXPLOR             // specify the pin configuration

#include <ltem1c.h>
#include <mqtt.h>

#define _DEBUG                          // enable/expand 
// debugging output options             UNCOMMENT one of the next two lines to direct debug (PRINTF) output
#include <jlinkRtt.h>                   // output debug PRINTF macros to J-Link RTT channel
// #define SERIAL_OPT 1                    // enable serial port comm with devl host (1=force ready test)

#include "lqc_collections.h"

#define DEFAULT_NETWORK_CONTEXT 1
#define XFRBUFFER_SZ 201
#define SOCKET_ALREADYOPEN 563

#define ASSERT(expected_true, failMsg)  if(!(expected_true))  appNotifyRecvr(255, failMsg)
#define ASSERT_NOTEMPTY(string, failMsg)  if(string[0] == '\0') appNotifyRecvr(255, failMsg)


/* Test is designed for use with Azure IoTHub. If you do not have a Azure subscription you can get a free test account with a preloaded credit.
 * Optionally you can request a trial account for LooUQ's LQ Cloud, which uses Azure IoTHub for device data ingress.
 * 
 * LQ Cloud or Azure IoTHub needs 3 provisioning elements: 
 *   -- The hub address, if you are using LQ Cloud for testing the MQTT_IOTHUB address below is valid. Otherwise supply your MQTT access point (no protocol prefix)
 *   -- A deviceId and 
 *   -- A SAS token (a timelimited password). For deviceId we recommend the modem's
 * 
 * The Device ID is up to 40 characters long and we suggest using the modem's IMEI. An example of the SAS token is shown on the next line.  
 * 
 * "SharedAccessSignature sr=iothub-dev-pelogical.azure-devices.net%2Fdevices%2Fe8fdd7df-2ca2-4b64-95de-031c6b199299&sig=XbjrqvX4kQXOTefJIaw86jRhfkv1UMJwK%2FFDiWmfqFU%3D&se=1759244275"
 *
 * Your values will be different, update MQTT_IOTHUB_DEVICEID and MQTT_IOTHUB_SASTOKEN with your values. If you obtain the SAS Token from Azure IoT Explorer it will
 * be prefixed with additional information: Hostname, DeviceId and the SharedAccessSignature property keys, these must be removed for using MQTT directly.
 * 
 * HostName=iothub-dev-pelogical.azure-devices.net;DeviceId=867198053158865;SharedAccessSignature=SharedAccessSignature sr=iothub-dev-pelogical.azure-devices.net%2Fdevices%2F867198053158865&sig=YlW6PuH4MA93cINEhgstihwQxWt4Zr9iY9Tnkl%2BuQK4%3D&se=1608052225
 * ^^^                          REMOVE, NOT PART OF THE SAS TOKEN                              ^^^              KEEP ALL FOLLOWING '='
*/

#define MQTT_IOTHUB "iothub-dev-pelogical.azure-devices.net"
#define MQTT_PORT 8883

#define MQTT_IOTHUB_DEVICEID "864508030074113"
#define MQTT_IOTHUB_USERID "iothub-dev-pelogical.azure-devices.net/" MQTT_IOTHUB_DEVICEID "/?api-version=2018-06-30"
#define MQTT_IOTHUB_SASTOKEN "SharedAccessSignature sr=iothub-dev-pelogical.azure-devices.net%2Fdevices%2F864508030074113&sig=bHV12YRyVnKJHtABYi%2BSGlnkvBCpRD0rb7Ak9rg2fxM%3D&se=2872679808"

#define MQTT_IOTHUB_D2C_TOPIC "devices/" MQTT_IOTHUB_DEVICEID "/messages/events/"
#define MQTT_IOTHUB_C2D_TOPIC "devices/" MQTT_IOTHUB_DEVICEID "/messages/devicebound/#"
#define MQTT_MSG_PROPERTIES "mId=~%d&mV=1.0&mTyp=tdat&evC=user&evN=wind-telemetry&evV=Wind Speed:18.97"
#define MQTT_MSG_BODY_TEMPLATE "devices/" MQTT_IOTHUB_DEVICEID "/messages/events/mId=~%d&mV=1.0&mTyp=tdat&evC=user&evN=wind-telemetry&evV=Wind Speed:18.97"

// test setup
#define CYCLE_INTERVAL 5000
uint16_t loopCnt = 1;
uint32_t lastCycle;

// ltem1 variables
socketResult_t result;
socketId_t mqttConnectionId = 1;
char mqttTopic[200];
char mqttMessage[200];


void setup() {
    #ifdef SERIAL_OPT
        Serial.begin(115200);
        #if (SERIAL_OPT > 0)
        while (!Serial) {}      // force wait for serial ready
        #else
        delay(5000);            // just give it some time
        #endif
    #endif

    PRINTFC(dbgColor_white, "\rLTEm1c test8-MQTT\r\n");
    gpio_openPin(LED_BUILTIN, gpioMode_output);

    ltem1_create(ltem1_pinConfig, appNotifyRecvr);

    PRINTFC(dbgColor_none, "Waiting on network...\r");
    networkOperator_t networkOp = ntwk_awaitOperator(30000);
    if (strlen(networkOp.operName) == 0)
        appNotifyRecvr(255, "Timout (30s) waiting for cellular network.");
    PRINTFC(dbgColor_info, "Network type is %s on %s\r", networkOp.ntwkMode, networkOp.operName);

    uint8_t cntxtCnt = ntwk_getActivePdpCntxtCnt();
    if (cntxtCnt == 0)
    {
        ntwk_activatePdpContext(DEFAULT_NETWORK_CONTEXT);
    }

    /* Basic connectivity established, moving on to MQTT setup with Azure IoTHub
    */

    ASSERT(mqtt_open(MQTT_IOTHUB, MQTT_PORT, sslVersion_tls12, mqttVersion_311) == RESULT_CODE_SUCCESS, "MQTT open failed.");
    ASSERT(mqtt_connect(MQTT_IOTHUB_DEVICEID, MQTT_IOTHUB_USERID, MQTT_IOTHUB_SASTOKEN, mqttSession_cleanStart) == RESULT_CODE_SUCCESS,"MQTT connect failed.");
    ASSERT(mqtt_subscribe(MQTT_IOTHUB_C2D_TOPIC, mqttQos_1, mqttReceiver) == RESULT_CODE_SUCCESS, "MQTT subscribe to IoTHub C2D messages failed.");
}


bool publishToCloud = true;

void loop() 
{
    if (lTimerExpired(lastCycle, CYCLE_INTERVAL))
    {
        lastCycle = lMillis();

        if (publishToCloud)
        {
            snprintf(mqttTopic, 200, MQTT_MSG_BODY_TEMPLATE, loopCnt);
            snprintf(mqttMessage, 200, "MQTT message for loop=%d", loopCnt);
            mqtt_publish(mqttTopic, mqttQos_1, mqttMessage);
        }
        else
            PRINTFC(dbgColor_info, "Publish skipped, disabled\r");

        loopCnt++;
        PRINTFC(dbgColor_magenta, "\rFreeMem=%u  <<Loop=%d>>\r", getFreeMemory(), loopCnt);
    }

    /* NOTE: ltem1_doWork() pipeline requires up to 3 invokes for each data receive. DoWork has no side effects 
     * other than taking time and should be invoked liberally. */
    ltem1_doWork();
}


void mqttReceiver(char *topic, char *topicProps, char *message)
{
    PRINTFC(dbgColor_info, "\r**MQTT--MSG** @tick=%d\r", lMillis());
    PRINTFC(dbgColor_cyan, "\rt(%d): %s", strlen(topic), topic);
    PRINTFC(dbgColor_cyan, "\rp(%d): %s", strlen(topicProps), topicProps);
    PRINTFC(dbgColor_cyan, "\rm(%d): %s", strlen(message), message);

    // use local copy of LQ Cloud query string processor
    keyValueDict_t mqttProps = lqc_createDictFromQueryString(topicProps);
    PRINTFC(dbgColor_info, "\rProps(%d)\r", mqttProps.count);
    for (size_t i = 0; i < mqttProps.count; i++)
    {
        PRINTFC(dbgColor_cyan, "%s=%s\r", mqttProps.keys[i], mqttProps.values[i]);
    }
    PRINTFC(0, "\r");
}



/* test helpers
========================================================================================================================= */

void appNotifyRecvr(uint8_t notifType, const char *notifMsg)
{
	PRINTFC(dbgColor_error, "\r\n** %s \r\n", notifMsg);
    PRINTFC(dbgColor_error, "** Test Assertion Failed. \r\n");

    int halt = 1;
    while (halt) {}
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

