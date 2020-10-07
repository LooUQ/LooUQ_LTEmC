# Circuit River - LTEm1c 

LooUQ Circuit River
LTEm1 modem driver implemented in C99 for portability and a small footprint.

## Pre-Release - v0.3 (release candidate)
----

The LooUQ LTEm1c driver supports the LooUQ Circuit River LTEm1 LTE modem. The LTEm1 is designed to allow for cost-effective cellular communications in embedded systems by supporting LTE CAT-M1 and LTE CAT-NB1 (NB-IOT) communications. The LTEm1 is certified for use on the Verizon network and a registered device for the Sprint (T-Mobile) Curiosity Network. It is also fully tested and supported using Hologram SIMs.

[LTEm1 Modem Specifications](https://github.com/LooUQ/CircuitRiver-LTEm1c/blob/master/LTEm1c%20Stack.png)

[LTEm1/LTEm1c FAQ](https://github.com/LooUQ/CircuitRiver-LTEm1c/blob/master/LTEm1-FAQ.md)

[Version History](https://github.com/LooUQ/CircuitRiver-LTEm1c/blob/master/version-history.md)

----
![LTEm1c](https://github.com/LooUQ/CircuitRiver-LTEm1c/blob/master/LTEm1c%20Stack.png)

All of the core (blue) subsystems are functional. Extension subsystems (copper, with wide-borders) are optional and can be built into the application, or omitted to reduced code size. Grey boxes indicate future feature areas. 


## Getting Started
*1st off, thanks for taking a look at the LTEm1 hardware and LTEm1c software!*

The best way to get started understanding LTEm1c and considering it for your projects *today* is to look at the tests folder in the repository. This will be supplemented in the coming weeks. Working on getting doxygen documentation available soon and some real world example projects. The tests folder contains a series of progressive LTEm1c tests. Starting with simple platform I/O verification, thru MQTT protocol support. Each test builds on a succesful previous level test.

Each test shows the process for initializing, starting services, and a loop exercising the functionality of the subsystem under test.

### LTEm1 Subsystems
| Subsystem | Functions |
| ---------  | --------- |
| *action_* | Handles invoking AT commands to the modem's module and handling response parsing. Supports variable command timeouts, thread safety and completion determination. Results are presented in a simple STRUCT with a HTTP type status code, along with the detail string response  |
| *mdminfo_* | Provides several common service functions for understanding the state of the modem and the network connection |
| *gnss_* | The LTEm1 has a true multi-constellation GNSS receiver (aka GPS is the US). The GNSS module provides support for accessing this functionality. |
| *geo_* | The LTEm1 supports the creation of geo-fence outlines and monitoring of device positioning relative to the geo-fence boundary. Note: geo_ requires the gnss_ module be built into your project to use these functions. |
| *sockets_* | This is the typical POSIX style sockets layer. Support if TCP\UDP\SSL clients are there. *Note:* server mode is not currently planned, all US networks I have worked with do not support incoming connections for cellular without add-on services like VPNs or other network constructs. I recommend alternatives to attempting direct connection to your device over cellular. |
| *mqtt_* | Support for MQTT client functionality is built here. Attach, connect, subscribe, unsubscribe and publish all are supported. Limited testing so far on QOS levels and advance features like clean and will. The MQTT module supports message properties appended to the topic and has a property parser available. |
| *iop_* | Generally you won't directly interact with the iop_ subsystem. It performs the buffer management to/from hardware. It interfaces with the action_, sockets_, and mqtt_ subsystems to perform the necessary transfers.


## Stay Tuned, Greg
