# Circuit River - LTEmC
## Release History

LooUQ Circuit River - LTEmC
Universal modem device driver for LooUQ’s embedded system LTEm modems.

## 3.1.0
What started out as 3.0.3 has evolved into a bigger release with some breaking changes so bumping the version up a level.
### Version Highlights
* Support for ESP32 (Currently tested under VSCode/Arduino CLI)
* Fixes and optimizations across Files_, MQTT_, and HTTP_.  

### Breaking Changes
* Requires LooUQ_EmbedLib released on/after Sep-24-2023 (23ce9ad). 
* All functions from mdmInfo_ module have been moved to the ltem_ module; mdmInfo_ is now obsolete.
* Data callbacks for http_ and files_ have a new signature that includes an offset (page and read respectively).


## 3.0.2
Maintenance release


## 3.0.1
### Introducing LTEmC Generation 3
Over the last 5 years, LTEmC has continually matured to meet the needs of projects. The developments have addressed new needs, while improving stability and efficiency. A big push in the 1st half of 2023 to consolidate ideas from various projects has coalesced into version 3.0.1.

v3.x is a whole new driver in most respects. The API surface has undergone numerous refinements. For those users on previous versions, we are here to help when you are ready to update your codebase. 

### Version Highlights

* The I/O buffering has been completely redesigned from the ground up; replacing previous approaches used in v1 and v2. The new buffer (cBuffer) was designed with several requirements: block based, minimal copy, circular, support for linear access for devices and application consumers. This new buffer implements the receive side of the driver and enabled numerous enhancements in the ATCMD (command/data supervisor) and IOP (input/output processor) systems.

* The IOP module was tuned to make use of the efficiencies introduced by cBuffer and to address some potential delays when servicing interrupts.

* The ATCMD module has a completely new API, while leveraging most of the core flow processing. The changes support more detailed diagnostic capture for command responses from the BGx. Additionally, the ATCMD changes support a simplified methodology for AT commands with an embedded data transfer; this simplified the protocol (HTTP/MQTT/sockets) and file system implementations.

* For protocols, v3.x supports HTTP(S), MQTT(S) and client sockets (UDP/TCP/SSL/TLS). These are processed in the BGx firmware and exposed to the host application as easy to use function APIs. Multiple streams for any protocol as supported (ex: two HTTP connections along with one MQTT connection); the maximum protocol streams are set to 4 but can be increased to 6.

* LTEmC v3.x can be easily ported to most any ARM framework and extended with new protocols/features.

