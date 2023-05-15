# Circuit River - LTEmC

LooUQ Circuit River
LTEm modem driver implemented in C99 for portability and a small footprint.


The LooUQ LTEmC driver supports the LooUQ Circuit River LTEm LTE modems. The LTEm modem series is designed to allow for cost-effective cellular communications in embedded systems by supporting LTE CAT-M1 and LTE CAT-NB1 (NB-IOT) communications. The LTEm1 and LTEm2 modems are certified for use on the Verizon network. Tested on all major US networks and fully compatible with the popular Hologram SIM cards.

[Version History](https://github.com/LooUQ/CircuitRiver-LTEmC/blob/master/version-history.md)

[LTEmC Documentation](https://loouq.github.io/sites/ltemc_doxy/html/index.html)

----
![LTEmC Stack](https://github.com/LooUQ/CircuitRiver-LTEmC/blob/master/LTEmC%20Stack.png)

All of the core (blue) subsystems are functional. Extension subsystems (copper, with wide-borders) are optional and can be built into the application, or omitted to reduced code size. Grey boxes indicate future feature areas. 

## Latest Release: v2.2

## Planned features for v3.01 - May 2023
* File system support on BGx
* Support for LTEm2 (BG95) and LTEm3F (BG77) features
* Initial support for BGx advanced power management


# Getting Started
*First off, thanks for taking a look at the LooUQ LTEm hardware and LTEmC software!*

The best way to get started understanding LTEmC and considering it for your projects *today* is to look at the tests folder in the repository. This collection of test/examples will continue to grow in future releases and new use cases are demonstrated. The tests folder contains a series of progressive LTEmC tests. Starting with simple platform I/O verification through to application protocol support (sockets, MQTT, HTTP), each test builds upon a succesful previous level test. While called **tests**, these projects they serve as **examples of how to do specific use cases**. These are your best source for scaffold code to start your LTEm project.

**Each test shows the process for initializing, starting services, and a loop exercising the functionality of the subsystem under test.**


### LTEmC Subsystems
| Subsystem | Functions |
| ---------  | --------- |
| *atcmd_* | Handles invoking AT commands to the modem's module and handling response parsing. Supports variable command timeouts, thread safety and completion determination. Results are presented in a simple STRUCT with a HTTP type status code, along with the detail string response  |
| *mdminfo_* | Provides several common service functions for understanding the state of the modem and the network connection |
| *gnss_* | The LTEm devices have a multi-constellation GNSS receiver (aka GPS is the US). The GNSS module provides support for accessing this functionality, like knowing exactly where your device is. |
| *geo_* | The LTEm devices support the creation of geo-fence outlines and monitoring of device positioning relative to the geo-fence boundary. Note: geo_ requires the gnss_ module be built into your project to use these functions. |
| *sockets_* | This is the typical POSIX style sockets layer. Support if TCP/UDP/SSL clients are there. *Note:* server mode is not currently planned, all US networks I have worked with do not support incoming connections for cellular without add-on services like VPNs or other network constructs. I recommend alternatives to attempting direct connection to your device over cellular. |
| *mqtt_* | Support for MQTT client functionality is built here. Attach, connect, subscribe, unsubscribe and publish all are supported. Limited testing so far on QOS levels and advance features like clean and will. The MQTT module supports message properties appended to the topic and has a property parser available. |
| *http_* | Support for HTTP client functionality is found here. Perform HTTP GET or POST actions with standard or custom request headers. |
| *iop_* | Generally you won't directly interact with the iop_ subsystem. It performs the buffer management to/from hardware. It interfaces with the action_, sockets_, and mqtt_ subsystems to perform the necessary transfers.

