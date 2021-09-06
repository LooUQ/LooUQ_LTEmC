Release 2.0

LooUQ is ready for tomorrow with the release of LTEmC version 2. Version 2 started in early June as version 1.1, intending to add HTTP support and the ability to dynamically load/unload protocols. To accomplish these goals it became apparent these objectives, there were some structural changes required. As these started being tackled, it took on a life of its own and numerous previously indentified issues got folded in to create the LTEmC that would stand up to the demands of the next several years; including additional protocols, support for 2 new modem models planned for the next 6 months, and new hardware features. 

The primary goal of the version 2.0 release is to enable the above future roadmap with little or no change to the exposed public API. LooUQ has accomplished this by reversing several dependencies and giving the application ownership of the data buffers and encapsulation of the protocol controls (sockets, MQTT, HTTP, etc.).


Previous versions of LTEmC allocated all buffers and protocol controls (structs). This resulted in memory bload, as the worse-case assumtions lead to larger buffers and multiple storage locations for protocol controls. This approach has been replaced with application defined (owned) data receive buffers and protocol controls. 
- At a minimum, to implement a protocol, your application will need to create a receive buffer (rxBuffer) and a protocol control. LTEmC only references these via pointers.
    - These can be local variables, if short lived. 
    - They can be dynamic and created by malloc(). 
    - Or, they can be global variables. 
- Protocol instances can share a rxBuffer. *Theoretically it is possible for different protocols to share a buffer but this has not been tested.*
- Each protocol has an _initControl() function to initialize your protocol instance control.
- There are 6 protocol "slots" defined in an enum as dataContext_0 through dataContext_5 the a protocol instance can be assigned to. 
- In version 2.1, with support added for the BGx file system, there is a special slot streamPeer_6.
- Use of the ltem_doWork() function in the application's main loop() is only required if your application utilizes protocols with asynchronous receive events. Sockets and MQTT both have asynchronous receive behavior; HTTP(S) receives are fully synchronous with page funtions.

## Example Code Setup

```
    // LTEm variables
    mqttCtrl_t mqttCtrl;                // MQTT control, data to manage MQTT connection to server
    uint8_t receiveBuffer[640];         // Data buffer where received information is returned (can be local, global, or dynamic... your call)

    char mqttTopic[200];                // application buffer to craft TX MQTT topic
    char mqttMessage[200];              // application buffer to craft TX MQTT publish content (body)
    resultCode_t result;


    void setup() 
    {
        /* define a global ltemc instance, then initialize and start communications */
        ltem_create(ltem_pinConfig, appNotifyCB);
        ltem_start();

        /* setup SSL\TLS if your application requires it MQTT over TLS or HTTPS, then initialize your protocol(s) */
        tls_configure(dataContext_5, tlsVersion_tls12, tlsCipher_default, tlsCertExpiration_default, tlsSecurityLevel_default);
        mqtt_initControl(&mqttCtrl, dataContext_5, mqtt__useTls, mqttVersion_311, receiveBuffer, sizeof(receiveBuffer), mqttRecvCB);

        /* do whatever your protocol needs to start host communications, here it is open, connect, and subscribe to topics */
        rslt = mqtt_open(&mqttCtrl, MQTT_IOTHUB, MQTT_PORT);
        rslt = mqtt_connect(&mqttCtrl, MQTT_IOTHUB_DEVICEID, MQTT_IOTHUB_USERID, MQTT_IOTHUB_SASTOKEN, mqttSession_cleanStart);
        rslt = mqtt_subscribe(&mqttCtrl, MQTT_IOTHUB_C2D_TOPIC, mqttQos_1);
    }

    void loop()
    {
        /* compose your MQTT message into a topic and message */
        snprintf(mqttTopic, 200, MQTT_MSG_BODY_TEMPLATE, loopCnt, windspeed);
        snprintf(mqttMessage, 200, "MQTT message for loop=%d", loopCnt);
        mqtt_publish(&mqttCtrl, mqttTopic, mqttQos_1, mqttMessage);

        /* MQTT support asychronous background message receives,
         * ltem_doWork() marshals the received data back to your application via the receive callback  */
        ltem_doWork();
    }

To fully understand how to setup each of the currently supported protocols (Sockets, MQTT, and HTTP), review the associated "test" INO file. Each of these have numerous comments on the protocol's setup and can serve as a blueprint for your application if running under the Arduino framework. If you have questions, please reach out by messaging answers@loouq.com. 