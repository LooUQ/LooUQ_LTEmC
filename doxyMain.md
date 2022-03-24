\mainpage
# Circuit River - LTEmC
## Latest Release: v2.3.0 

LTEm1 modem driver implemented in C for portability and a small footprint.

The LooUQ LTEmC driver supports the LooUQ Circuit River LTEm1 LTE modem. The LTEm1 is designed to allow for cost-effective cellular communications in embedded systems by supporting LTE CAT-M1 and LTE CAT-NB1 (NB-IOT) communications. The LTEm1 is certified for use on the Verizon network. It is also fully tested and supported using Hologram SIMs and several US carriers.

**First off, thanks for taking a look at the LTEm1 hardware and LTEmC software!**

## Getting Started
The best way to get started understanding LTEmC and considering it for your projects *today* is to look at the tests folder in the repository. This collection of test/examples will continue to grow in future releases and new use cases are demonstrated. The tests folder contains a series of progressive LTEm1c tests. Starting with simple platform I/O verification through to application protocol support (sockets, MQTT, HTTP), each test builds upon a succesful previous level test. While called **tests**, these projects they serve as **examples of how to do specific use cases**. These are your best source for scaffold code to start your LTEm project.

**Each test shows the process for initializing, starting services, and a loop exercising the functionality of the subsystem under test.**


### LTEmC Subsystems
| Subsystem | Functions |
| ---------  | --------- |
| *atcmd_* | Handles invoking AT commands to the modem's module and handling response parsing. Supports variable command timeouts, thread safety and completion determination. Results are presented in a simple STRUCT with a HTTP type status code, along with the detail string response  |
| *mdminfo_* | Provides several common service functions for understanding the state of the modem and the network connection |
| *gnss_* | The LTEm1 has a multi-constellation GNSS receiver (aka GPS is the US). The GNSS module provides support for accessing this functionality, like knowing exactly where your device is. |
| *geo_* | The LTEm1 supports the creation of geo-fence outlines and monitoring of device positioning relative to the geo-fence boundary. Note: geo_ requires the gnss_ module be built into your project to use these functions. |
| *sockets_* | This is the typical POSIX style sockets layer. Support if TCP/UDP/SSL clients are there. *Note:* server mode is not currently planned, all US networks I have worked with do not support incoming connections for cellular without add-on services like VPNs or other network constructs. I recommend alternatives to attempting direct connection to your device over cellular. |
| *mqtt_* | Support for MQTT client functionality is built here. Attach, connect, subscribe, unsubscribe and publish all are supported. Limited testing so far on QOS levels and advance features like clean and will. The MQTT module supports message properties appended to the topic and has a property parser available. |
| *http_* | Support for HTTP client functionality is found here. Perform HTTP GET or POST actions with standard or custom request headers. |
| *iop_* | Generally you won't directly interact with the iop_ subsystem. It performs the buffer management to/from hardware. It interfaces with the action_, sockets_, and mqtt_ subsystems to perform the necessary transfers.


[LTEm1 Modem Specifications](https://loouq.github.io/resources/CTA-LTEM1_DataSheet.pdf)

[LTEm1/LTEmC FAQ](https://github.com/LooUQ/CircuitRiver-LTEmC/blob/master/LTEm1-FAQ.md)

[Version History](https://github.com/LooUQ/CircuitRiver-LTEmC/blob/master/version-history.md)

[LTEmC Documentation](https://loouq.github.io/sites/LTEmC_doxy/html/index.html)

----
\image html "LTEmC Stack.png"

All of the core (blue) subsystems are functional. Extension subsystems (copper, with wide-borders) are optional and can be built into the application, or omitted to reduced code size. Grey boxes indicate future feature areas. 


## Future Directions
If you have an opinion on any of these functions and their applicability to your product/project please let LooUQ know. Send your thoughts to answers@loouq.com.
* Support for BG95 and BG77
* Power Management - Support for PSM, eDRX, etc.
* File System - Support for file storage on BGx via driver (BG96 has approx. 3MB available)
* FOTA - Support for scheduled FOTA 
