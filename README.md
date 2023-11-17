# LooUQ - LTEmC

LooUQ's universal modem device driver for LooUQ’s embedded system LTE modems: LTEm1, LTEm2 and LTEm3F (coming soon).

The LooUQ LTEmC driver supports the LooUQ LTEm line of embedded modem devices. The LTEm modem series is designed to allow for cost-effective cellular communications in embedded systems by supporting LTE CAT-M1 and LTE CAT-NB1 (NB-IOT) communications. The LTEm1 and LTEm2 modems are certified for use on the Verizon network and tested on all major US networks.

### Latest Release: v3.0.1

## Introducing LTEmC Generation 3

Over the last 5 years, LTEmC has continually matured to meet the needs of projects. The developments have addressed new needs, while improving stability and efficiency. A big push in the 1st half of 2023 to consolidate ideas from various projects has coalesced into version 3.0.1.

v3.x is a whole new driver in most respects. The API surface has undergone numerous refinements. For those users on previous versions, we are here to help when you are ready to update your codebase. 

### Version Highlights

* The I/O buffering has been completely redesigned from the ground up; replacing previous approaches used in v1 and v2. The new buffer (cBuffer) was designed with several requirements: block based, minimal copy, circular, support for linear access for devices and application consumers. This new buffer implements the receive side of the driver and enabled numerous enhancements in the ATCMD (command/data supervisor) and IOP (input/output processor) systems.

* The IOP module was tuned to make use of the efficiencies introduced by cBuffer and to address some potential delays when servicing interrupts.

* The ATCMD module has a completely new API, while leveraging most of the core flow processing. The changes support more detailed diagnostic capture for command responses from the BGx. Additionally, the ATCMD changes support a simplified methodology for AT commands with an embedded data transfer; this simplified the protocol (HTTP/MQTT/sockets) and file system implementations.

* For protocols, v3.x supports HTTP(S), MQTT(S) and client sockets (UDP/TCP/SSL/TLS). These are processed in the BGx firmware and exposed to the host application as easy to use function APIs. Multiple streams for any protocol as supported (ex: two HTTP connections along with one MQTT connection); the maximum protocol streams are set to 4 but can be increased to 6.

* LTEmC v3.x can be easily ported to most any ARM framework and extended with new protocols/features.


## LTEmC Overview

* Protocols (HTTP/MQTT/etc.) and file system (persistent storage) are only loaded and consume resources if included in your application.
* While LooUQ believes that the API is rich and complete, LTEmC supports easy extension for new services and protocols as needs arise. If the BGx module supports it, LTEmC can too.
* All MAKE files are structured for minimal linked object size. Functions are defined for individual linkage.
* Source code is extensively documented with Doxygen attributes and the built Doxygen manual is available here.
* LTEmC was written to C99 standards for portability. The test files where created as Arduino .ino files to demonstrate ease of implementation for one of the most popular embedded frameworks.
* LTEmC can be ported to any framework easily. This is accomplished by creating a handful of “platform” functions. No direct timer or hardware-heavy dependent code is located in the LTEmC codebase. LooUQ is happy to assist with porting, just reach out.
* LTEmC is open source with a GPL version 3 license. This makes it possible to use the LTEm series modems and the LTEmC software in your non-profit or commercial application. If your organization needs a proprietary commercial license, please contact LooUQ at info@loouq.com to discuss your needs.



[Version History](https://github.com/LooUQ/CircuitRiver-LTEmC/tree/main/extras/version-history.md)

[LTEmC Documentation Site](https://loouq.github.io/sites/ltemc_doxy/html/index.html)

----
![LTEmC Stack](https://loouq.github.io/content/LTEmC_Stack.png)

All of the core (blue boxes) subsystems are implemented today and have been field tested for over a year. Extension subsystems (copper boxes) are optional and can be built into the application or omitted. Grey boxes indicate future feature areas under development. 

# Getting Started
*First off, thanks for taking a look at the LooUQ LTEm hardware and LTEmC software!*

The best way to get started with LTEmC v3.x is to look at the tests folder and look at the protocols your application will likely need.
* Test 1, 2 and 3 (platform, bgx, iopisr) are only going to be of interest to users that are looking to port LTEmC to platforms other than Arduino. LooUQ choose to start with the Arduino framework, but we have closely examined the effort to support Microchip ASF and other leading MCU suppliers. More on porting below.
* Test 4 (atcmd) is a general-purpose modem test. Running it provides assurance that all core subsystems are performing as expected.
* Tests 5-11 demonstrate how to implement a feature in your application.
    * Sockets and MQTT will require that you provide a service to test against. Sockets can use an open-sourced utility: Packet Sender. LooUQ uses Azure IoT-Hub for testing MQTT.
    * Note that test 11 (gpio) only applies to the CR-LTEM3F modem with GPIO and ADC capabilities, the LTEm3F is arriving this summer.

And as always, feel free to ask. LooUQ has answers.loouq.com and answers@loouq.com available for support at any time.

### Release Cadence
Version 3.0.1 is ready now. It has been in development as a beta branch since early 2023. A maintenance release is anticipated in June to coincide with the release of LQCloud version 1.0.1. Later this summer, the version numbering will switch to 3.1.x indicating that the code has reached a milestone in field testing.

