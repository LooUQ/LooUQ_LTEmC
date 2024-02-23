# LTEmC Examples

LooUQ provides a series of examples for common use-case scenarios to assist you with accelerating your IoT project/product. These examples can be cherry-picked to find the parts you want for a complete solution.

**Note:** *There is also a "tests" folder containing applications to test modem hardware functionality. These are primarily for LTEmC users porting the driver to new environments/frameworks.*

## ltem_getModemInfo
Demonstrates how to create/start modem instance and retrieve basic information about the module and installed SIM card.
***

## http_basicRequest
***

## http_customPOSTRequest
***

## mqtt_hivemq-sendRecv
This application performs a continuous loop of sends and waiting for received messages. There is a companion C# (.Net 8) console application to sign on to HiveMQ and periodically send messages to the device. HiveMQ Cloud provides a free low-volume cluster; the cluster supports multiple host and device connections ([HiveMQ pricing](https://www.hivemq.com/pricing/)). 
***

## mqtt_azure-sendRecv
Example to demonstrate creating credentials and send/receive with Azure IoTHub over MQTTS. The host side for interacting with the device is [Azure IoT Explorer](https://github.com/Azure/azure-iot-explorer/releases). Using IoT Explorer you can monitor the device-to-cloud messages and you can also send cloud-to-device messages (the two actions above cannot be done simultaneously from a single instance).
***

## mqtt_azure-periodicRF
On the CR-LTEm2 (BG95-M3 based) and CR-LTEm3F (BG77 based) there is a single radio path (RF) that is shared between the cellular and GNSS circuits. This design prevents ***simultaneous*** cellular/GNSS operations. This example demonstrates how to periodically fetch GNSS location information and then perform MQTT send of data.
***

## sckt_sendRecv
While most IoT communications uses HTTP or MQTT, there may exist use cases where direct UDP/TCP/SSL/TLS socket communications are needed. The LTEm producs and LTEmC can fully support this. You will need a remote socket server to communicate with, LooUQ recommends [Packet Sender](https://packetsender.com/), this is a free download to start and a resonable ask for a donation. Please note that Packet Sender is a quality offering created and maintained by Dan Nagle. 
***

## gnss_getLocation
The example shows how to get the location of the modem, test if it is "locked" in and also demonstrates how to multiplex the single RF path on the LTEm2 and LTEm3F (the LTEm1 has separate RF paths for cellular and GNSS).
***

## file_workingWithFilesystem
All of the LooUQ LTEm series modems have flash storage available to the host application (approximately 2MB). This storage is persistent across power cycle and restart events. Review this example to learn how to get general filesystem information, list files, and read/write/delete files.
***

## gpio_performingIOADC **(LTEm3F only)**
This example demonstrates how to both read and write GPIO lines/pins. Also included on the LTEm3F is 2 ADC (0-3v3) analog channels; the functions for ADC read are also shown. If desired, you can create a custom "wrap" cable to assist with GPIO testing.
***

## howTo_fetchConfig *(composite)*
Using modem information, HTTP GET and the modem's file system, this example demonstrates how to request a settings/config collection and persistently store it for use by a host application.
***

## howTo_memfaultOTA_ESP32 *(composite)*
This example shows how you can fetch a binary from Memfault, validate it and configure ESP to transition to it. This example only functions on an ESP32 for Arduino project and was tested using an ESPxxxxxxx development board.
***
