# Circuit River - LTEm1c 

LooUQ Circuit River
LTEm1 modem driver implemented in C99 for portability and a small footprint.

----
MQTT Testing

The MQTT test LTEm1c-9-mqtt.ino tests the LTEm1 modem's MQTT functionality using Azure IoTHub. You can obtain a free Azure account to test IoTHub from Microsoft.

You can learn about using IoTHub directly from MQTT in the guide below.
[Azure IoTHub MQTT Support](https://docs.microsoft.com/en-us/azure/iot-hub/iot-hub-mqtt-support)

The test continuously publishes message to IoTHub which you can view in **Azure IoT Explorer** as shown below. You can also publish C2D (cloud-to-device) messages to your
device using IoT Explorer (debugging output shown below in Segger RTT viewer).

\image html "MQTT_D2C.png"

\image html "MQTT_C2D.png"


