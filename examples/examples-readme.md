# LTEmC Examples

LooUQ provides a series of examples for common use-case scenarios to assist you with accelerating your IoT project/product. These examples can be cherry-picked to find the parts you want for a complete solution.

**Note:** *There is also a "tests" folder containing applications to test modem hardware functionality. These are primarily for LTEmC users porting the driver to new environments/frameworks.*

## ltem_getModemInfo
Demonstrates how to create/start modem instance and retrieve basic information about the module and installed SIM card.
***

## mqtt_hivemq-sendRecv
This application performs a continuous loop of sends and waiting for received messages. There is a companion C# (.Net 8) console application to sign on to HiveMQ and periodically send messages to the device. HiveMQ Cloud provides a free low-volume cluster; the cluster supports multiple host and device connections ([HiveMQ pricing](https://www.hivemq.com/pricing/)). 
***

## mqtt_azure-sendRecv
Example to demonstrate creating credentials and send/receive with Azure IoTHub over MQTTS. The host side for interacting with the device is [Azure IoT Explorer](https://github.com/Azure/azure-iot-explorer/releases). Using IoT Explorer you can monitor the device-to-cloud messages and you can also send cloud-to-device messages (the two actions above cannot be done simultaneously from a single instance).
***

## mqtt_azure-periodicRF
On the CR-LTEm2 (BG95-M3 based) and CR-LTEm3F (BG77 based) there is a single radio path (RF) that is shared between the cellular and GNSS circuits. This design prevents ***simultaneous*** cellular/GNSS operations. This example demonstrates how to periodically fetch GNSS location information and then perform MQTT send of data.







# In progress
## file_workingWithFilesystem
## gnss_getLocation
## gpio_performingIOADC
## howTo_fetchConfig
## howTo_memfaultOTA_ESP32
## http_basicRequest
## http_customPOSTRequest
## sckt_sendRecv