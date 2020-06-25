/******************************************************************************
 *  \file LTEm1c-8-mqtt.ino
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
 ******************************************************************************
 * Test MQTT protocol client send/receive. 
 * Uses Azure IoTHub and the LooUQ Cloud as server side
 *****************************************************************************/

#include <ltem1c.h>
#include <stdio.h>

#define _DEBUG
#include "platform/platform_stdio.h"

#define DEFAULT_NETWORK_CONTEXT 1
#define XFRBUFFER_SZ 201
#define SOCKET_ALREADYOPEN 563

const int APIN_RANDOMSEED = 0;

ltem1PinConfig_t ltem1_pinConfig =
{
  spiCsPin : 13,
  irqPin : 12,
  statusPin : 6,
  powerkeyPin : 11,
  resetPin : 19,
  ringUrcPin : 5,
  wakePin : 10
};

spiConfig_t ltem1_spiConfig = 
{
  dataRate : 2000000U,
  dataMode : spiDataMode_0,
  bitOrder : spiBitOrder_msbFirst,
  csPin : ltem1_pinConfig.spiCsPin
};


#define MQTT_IOTHUB "iothub-dev-pelogical.azure-devices.net"
#define MQTT_PORT 8883

#define MQTT_IOTHUB_DEVICEID "e8fdd7df-2ca2-4b64-95de-031c6b199299"
#define MQTT_IOTHUB_USERID "iothub-dev-pelogical.azure-devices.net/e8fdd7df-2ca2-4b64-95de-031c6b199299/?api-version=2018-06-30"
#define MQTT_IOTHUB_PASSWORD "SharedAccessSignature sr=iothub-dev-pelogical.azure-devices.net%2Fdevices%2Fe8fdd7df-2ca2-4b64-95de-031c6b199299&sig=6hTmu6e11E9CCKo1Ppeg8qxTfSRIfFwaau0crXeF9kQ%3D&se=2058955139"

#define MQTT_IOTHUB_D2C_TOPIC "devices/e8fdd7df-2ca2-4b64-95de-031c6b199299/messages/events/"
#define MQTT_IOTHUB_C2D_TOPIC "devices/e8fdd7df-2ca2-4b64-95de-031c6b199299/messages/devicebound/#"
#define MQTT_MSG_PROPERTIES "mId=~%d&mV=1.0&mTyp=tdat&evC=user&evN=wind-telemetry&evV=Wind Speed:18.97"


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
    #ifdef USE_SERIAL
        Serial.begin(115200);
        #if (USE_SERIAL)
        while (!Serial) {}
        #else
        delay(1000);
        #endif
    #endif

    PRINTF(dbgColor_white, "\rLTEm1c test8-MQTT\r\n");
    gpio_openPin(LED_BUILTIN, gpioMode_output);

    ltem1_create(&ltem1_pinConfig, ltem1Functionality_services);
    mqtt_create();

    PRINTF(dbgColor_none, "Waiting on network...");
    do
    {
        if (ntwk_getOperator().operName[0] == NULL)
            timing_delay(1000);
            
    } while (g_ltem1->network->networkOperator->operName[0] == NULL);
    PRINTF(dbgColor_info, "Operator is %s\r", g_ltem1->network->networkOperator->operName);

    socketResult_t result = ntwk_fetchDataContexts();
    if (result == ACTION_RESULT_NOTFOUND)
    {
        ntwk_activateContext(DEFAULT_NETWORK_CONTEXT);
    }

    /* Basic connectivity established, moving on to MQTT setup with Azure IoTHub
    */


    result = mqtt_open(mqttConnectionId, MQTT_IOTHUB, MQTT_PORT, sslVersion_tls12, mqttVersion_311);
    result = mqtt_connect(mqttConnectionId, MQTT_IOTHUB_DEVICEID, MQTT_IOTHUB_USERID, MQTT_IOTHUB_PASSWORD);

    result = mqtt_subscribe(mqttConnectionId, MQTT_IOTHUB_C2D_TOPIC, mqttQos_0, mqttReceiver);

    // force send at start
    lastCycle = timing_millis() + 2*CYCLE_INTERVAL;
}



void loop() 
{
    if (timing_millis() - lastCycle >= CYCLE_INTERVAL)
    {
        lastCycle = timing_millis();
        PRINTF(dbgColor_white, "\rLoop=%i>>\r", loopCnt);

        snprintf(mqttTopic, 200, "devices/e8fdd7df-2ca2-4b64-95de-031c6b199299/messages/events/mId=~%d&mV=1.0&mTyp=tdat&evC=user&evN=wind-telemetry&evV=Wind Speed:18.97", loopCnt);
        snprintf(mqttMessage, 200, "MQTT message for loop=%d", loopCnt);
        mqtt_publish(mqttConnectionId, mqttTopic, mqttQos_0, mqttMessage);

        loopCnt++;
        PRINTF(dbgColor_dMagenta, "\rFreeMem=%u  ", getFreeMemory());
        PRINTF(dbgColor_dMagenta, "<<Loop=%d\r", loopCnt);
    }

    /*
     * NOTE: ltem1_doWork() pipeline requires up to 3 invokes for each data receive. DoWork has no side effects 
     * other than taking time and should be invoked liberally.
     */
    ltem1_doWork();
}


void mqttReceiver(socketId_t socketId, const char * topic, const char * message)
{
    PRINTF(dbgColor_info, "mqttMsg for topic: %s, is:%s @tick=%d\r", topic, message, timing_millis());
}





/* test helpers
========================================================================================================================= */




void indicateFailure(char failureMsg[])
{
	PRINTF(dbgColor_error, "\r\n** %s \r\n", failureMsg);
    PRINTF(dbgColor_error, "** Test Assertion Failed. \r\n");

    int halt = 1;
    while (halt)
    {
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
    }
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

