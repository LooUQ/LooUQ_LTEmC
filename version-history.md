# LTEm1c Version History
#### Major or breaking changes will be called out with section explaining the change and the release header will be decorated with either *Major* or *Breaking*

## v0.5.0 - Fixes and Network PDP context handling (Dec 28, 2020)
* Fixed MQTT not trapping publish errors
* Fixed ACTION parsing on multistep actions with custom response parser (affected MQTT publish)
* Changed NETWORK module PDP context handling
    * Changed internal active PDP table organization
    * Change function signatures to better match behaviors
    * Added reset to deactivate\activate all presently active PDP contexts
* Change IOP pdp deactivate detect logic to match internal implementation in NETWORK module

## 0.4.2 (Dec 15, 2020)
*	Updated ISR for intermittent missed event when receiving MQTT publish
*	Reformatted tests/LTEm1c-8-mqtt.ino for readability and test setup with Azure IoTHub
*	Removed utils.c from LTEm1c library
*	Functionality is now part of LQ Cloud but can be used independently as lqc_collections (.h\.c)

## v0.4.1 - Misc (Nov 28, 2020)
* Remove dependency on JLink RTT 
* Fixed parse issue in GNSS getLocation()

## v0.3 - IOP Redesign (Oct 6, 2020)
Overhaul of the IOP buffering system. 
* Simplified receive buffer management and eliminated copies from receive pipeline
    * Small copy <60 chars remains (required) for MQTT receives
* Reduce code in ISR path, push processing to doWork() background path
* Finishing MQTT support
    * Publish to topic
    * Subscribe\unsubscribe to topics
    * Support for C2D (cloud-to-device) topic based properties
    * Receiver events per subscription (subscriptions can share a receiver function or have distinct receiver functions)
* Multiple fixes

## v0.2 - Fixes and Enhancements (Jun 25, 2020)
* TCP/UDP large packets (60 < packets <=1460 bytes), MQTT(s) functional, Geo-Fence support
* Improved performance in interrupt servicing
* Some memory optimizations
* Background tasks alignment for RTOS and Arduino Scheduler compatibility
* Added Geo-Fence support 
* FOTA partial support for DFOTA via HTTPS (file system FOTA is a roadmap feature )

## v0.1 - Initial publish
First release of a functional LTEm1 driver. Supported sockets (TCP\UDP\SSL) and limited MQTT. 
* Pure C99
* Hardware/platform abstraction. Tested on SAMD21 and Arduino foundation but adaptable to RTOS 
* Abstraction of AT commands workflow, simple methods allow for invoke and await pattern
* Minimal copy design
* Network (cellular) connectivity, GNSS, and AT command support
* Initial TCP/UDP send/recv (very small packets)
* GNSS (GPS) support
* Tests (VS Code Arduino) for above
* Central IOP (input\output processor) to handle BGx communications and buffer management
* Implements TCP\UDP\SSL clients using a Sockets pattern with receiver events
* Started MQTT support

