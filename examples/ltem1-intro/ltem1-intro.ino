/******************************************************************************
 *  \file ltem1-intro.ino
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
 * Example of a simple weather station that can be easily developed and located
 * remotely by using the LooUQ LTEm1 cellular modem via the LooUQ_LTEm1c 
 * software driver. This example integrates with Microsoft Azure IoT Hub.
 *****************************************************************************/

#include <stdio.h>
#include <Wire.h>

#include <ltem1c.h>
#include <SparkFunBME280.h>

#define _DEBUG
#include "platform/platform_stdio.h"

#define DEFAULT_NETWORK_CONTEXT 1

// ltem1PinConfig_t feather_pinConfig =
// {
//   spiCsPin : 13,
//   irqPin : 12,
//   statusPin : 6,
//   powerkeyPin : 11,
//   resetPin : 19,
//   ringUrcPin : 5,
//   wakePin : 10
// };

ltem1PinConfig_t uxplor_pinConfig =
{
  spiCsPin : 13,
  irqPin : 12,
  statusPin : 6,
  powerkeyPin : 11,
  resetPin : 10,
};

spiConfig_t ltem1_spiConfig = 
{
  dataRate : 2000000U,
  dataMode : spiDataMode_0,
  bitOrder : spiBitOrder_msbFirst,
  csPin : uxplor_pinConfig.spiCsPin
};

BME280 bmeSensor;

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

    /* Throughout the example I will use a PRINTF() macro to output debugging information.
     * Using the macro serves two objectives: 1) it can be expanded or not expanded to a concrete
     * print function easily, 2) it can be mapped easily to the desired concrete print function, which
     * in my case is the Segger J-Link RTT printf function.
     * For more information on using Segger J-Link RTT see my hackster article on VSCode Arduino.
     * https://www.hackster.io/greg-loouq/developing-embedded-samd-applications-with-vs-code-arduino-cccb7c
    */
    PRINTF(dbgColor_white, "\rIntroducing the LTEm1/LTEm1c\r\n");
    gpio_openPin(LED_BUILTIN, gpioMode_output);

    asm(".global _printf_float");

    Wire.begin();
    if (bmeSensor.beginI2C() == false) //Begin communication over I2C
    {
        Serial.println("The sensor did not respond. Please check wiring.");
        while(1); //Freeze
    }

    // Initialize the LTEm1 and power it up
    // The LTEm1c driver can create the memory structures for the driver and
    // defer powering on the modem, if desired.
    // If powering on the modem is deferred, you will use ltem1_start() to turn it on and Initialize it.
    ltem1_create(&uxplor_pinConfig, ltem1Start_powerOn, ltem1Functionality_services);

    // MQTT is an optional module in the LTEm1c library, you will setup it independently 
    mqtt_create();

    // now we are checking for cellular network availability, if the modem was already turned-on
    // it will be immediate. If we turned the modem on during ltem1_create() there could be 20-30 lag
    // before the network will be identified and connectivity established.
    PRINTF(dbgColor_none, "Waiting on network...");
    do
    {
        if (ntwk_getOperator().operName[0] == NULL)
            timing_delay(1000);
            
    } while (g_ltem1->network->networkOperator->operName[0] == NULL);
    PRINTF(dbgColor_info, "Network operator is %s\r", g_ltem1->network->networkOperator->operName);

    // cellular carriers connect via PDP (packet data protocol) contexts
    // the PDP context is responsible for basic IP network services
    socketResult_t result = ntwk_fetchDataContexts();
    if (result == ACTION_RESULT_NOTFOUND)
    {
        ntwk_activateContext(DEFAULT_NETWORK_CONTEXT);
    }

    /* I will be using the GPS information to tag the data going to the cloud
     * and also use the altitude value to correct the raw barametric pressure 
     * readings to sea level (as most weather reporting agencies do).
     * 
     * Turning the GNSS (aka GPS in US) hardware on, no other initialization required
    */
    actionResult_t cmdResult = gnss_on();

    /* Basic cellular\IP network connectivity established
     * Moving on to MQTT setup with Azure IoTHub 
    */

   /* Starting the MQTT protocol: I am talking with MS Azure IoTHub over MQTTS
    * IoTHub requires SSL\TLS, so setting the connection to use TLS 1.2
   */
    result = mqtt_open(mqttConnectionId, MQTT_IOTHUB, MQTT_PORT, sslVersion_tls12, mqttVersion_311);
    result = mqtt_connect(mqttConnectionId, MQTT_IOTHUB_DEVICEID, MQTT_IOTHUB_USERID, MQTT_IOTHUB_PASSWORD);

    /* While not using the Cloud-2-Device functionality in this example, the following MQTT subscription
     * would allow the device to be sent commands (securely, once connected with IoTHub). I will be
     * demonstrating this in an up coming published example.
    */
    result = mqtt_subscribe(mqttConnectionId, MQTT_IOTHUB_C2D_TOPIC, mqttQos_0, mqttReceiver);
    PRINTF(0, "subscribeResult=%d\r", result);
}

/* Setup some variables for MQTT content. In this example I will be hand crafting the JSON content to 
 * send to the cloud. In the next example (published soon) I will be using ArduinoJson to perform 
 * serialization.
*/ 
#define BODY_SZ 400
char msgBody[BODY_SZ];
gnssLocation_t location;


void loop() 
{
    // Breaking the delay() habit, track millis() for a more robust set of loop timed events
    if (timing_millis() - lastCycle >= CYCLE_INTERVAL)
    {
        lastCycle = timing_millis();

        PRINTF(dbgColor_white, "\rLoop=%i>>\r", loopCnt);

        // Read the BME sensor
        bmeSensor.setMode(MODE_FORCED); //Wake up sensor and take reading
        long startTime = millis();
        while(bmeSensor.isMeasuring() == false) ; //Wait for sensor to start measurment
        while(bmeSensor.isMeasuring() == true) ; //Hang out while sensor completes the reading    
        long endTime = millis();
        PRINTF(dbgColor_cyan, " Sensor acquire time(ms): %d\r", endTime - startTime);
        //Sensor is now back asleep, ready to get the data

        // read GNSS for location information
        location = gnss_getLocation();
        if (location.statusCode == 200)
        {
            char cLat[12];
            char cLon[12];

            PRINTF(dbgColor_none, "Location Information\r");
            PRINTF(dbgColor_info, "Lat=%f, Lon=%f \r", location.lat.val, location.lon.val);
        }
        else
            PRINTF(dbgColor_warn, "Location is not available (GNSS not fixed)\r");

        /* Start building the MQTT message contents.
         * The IoTHUB topic contains optional properties, there are a couple properties required by LooUQ Cloud
         * These "properties" are added to the publishing topic like HTTP query string.
        */
        snprintf(mqttTopic, 200, "devices/e8fdd7df-2ca2-4b64-95de-031c6b199299/messages/events/mId=~%d&mV=1.0&mTyp=tdat&evC=user&evN=env-telemetry", loopCnt);

        /* Serializing the msgBody here is done in a simple fashion using C snprintf(). 
         * I am running on an Adafruit SAMD21 device with float support in printf.
         * You can see raw creation of a JSON body for my MQTT message isn't difficult. It future
         * examples I will be showing how to standardize this process with ArduinoJson.
        */
        double envT = bmeSensor.readTempF();
        double envH = bmeSensor.readFloatHumidity();
        double envP = bmeSensor.readFloatPressure() / 3386.4;     // as in Hg

        // convert from meters to feet, then factor altitude (1 inch Hg/1000 feet)
        double envPc = envP + (location.altitude * 3.281 / 1000);

        snprintf(msgBody, BODY_SZ, "{\"temperature\":%3.1f, \"humidity\":%3.1f,\"barPressure\":%3.2f,\"barPressureCSL\":%3.2f,\"latitude\":%4.6f,\"longitude\":%4.6f,\"altitude\":%5.1f}"
            , envT, envH, envP, envPc, location.lat.val, location.lon.val, location.altitude * 3.281);

        PRINTF(dbgColor_info, "%s\r", msgBody);

        // now send it to Azure
        mqtt_publish(mqttConnectionId, mqttTopic, mqttQos_0, msgBody);

        /* I like to check for free memory in my applications, I frequently send this
         * and power information back to the cloud for monitoring my device as part of
         * an effort to be confident in the health of the remote device.
         * After all, if done correctly, we will only see our data, not our device.
        */
        loopCnt++;
        PRINTF(dbgColor_dMagenta, "\rFreeMem=%u  ", getFreeMemory());
        PRINTF(dbgColor_dMagenta, "<<Loop=%d\r", loopCnt);
    }

    /*
     * NOTE: ltem1_doWork() performs background tasks and quickly returns. 
     * DoWork has no side effects other than taking time and should be invoked liberally.
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

