# LTEmC Version History
#### Major or breaking changes will be called out with section explaining the change and the release header will be decorated with either *Major* or *Breaking*

## v2.3.0
### Moved radio access technology (RAT) functions to network 
* Moved functions to ntwk_ module to set RAT allowed (LTE,GSM), RAT scan sequence (GSM,CAT-M1,NB-IOT), and IOP mode (CAT-M1,NB-IOT)
* RAT functions are beta right now, invoking them will cause BGx to suspend. Future signature and guidance likely in a 2.3.1 release.
* Added timeout parameter for mqtt_publish() due to variability of network and server responsiveness

## v2.2.0
### Enhancements to network module for specification of APN name, PDP IP type (v4\v6\both) username\password, authentication method
*   Changed signatures for ntwk functions for activate PDP and get active contexts
*   Enhanced ntwk PDP options
*   Updates to atcmd functions
    *   Made atcmd_getResult() public to allow for deferred AT response retrieval
#### Breaking Changes
*   atcmd functions
    *   Removed locking option from atcmd_setOptions()
    *   Changed atcmd_tryInvokeAutoLockWithOptions() to atcmd_tryInvokeWithOptions()
*   ntwk
    * Changed ntwk_getActivePdpCntxtCnt() to ntwk_fetchActivePdpCntxts()
    * Added two new parameters to ntwk_activatePdpContext(DEFAULT_NETWORK_CONTEXT, pdpCntxtProtocolType_IPV4, "yourAPNname")

## v2.1.1
### Fixes to HTTP and network modules
*   Updated network to ensure network is IPV4. Future version will allow for complete configurability of the APN\PDP parameters
*   Fixed a timing issue in the http_read()

## v2.1.0 
### Fixes and minor enhancements
*   Fixed intermittent timing issue in HTTP read response
*   Fixed deadlock condition if HTTP request failed 
*   Fixed modem info, where after loss of connection modem reported previous carrier and network type
*   Changed modem reset logic to be more resilient
*   Changed BGx startup logic to be more resilient
*   Changed tests to reflect other changes in this release
#### Breaking Changes
*   Changed signature for application event notification callback to include uint8_t assembly and instance indications.
    *   LTEmC will set assembly to 0xC0, LQCloud will set assembly to 0xC1 
*   Changed name of set yield callback to ltem_setYieldCallback()

## v2.0.0 - Major long-term release
### Added HTTP(S) functionality and redesigned buffering model. Enhancements to AT-Command module to allow for continued extensions to supported features.
*   Added support for HTTP(S) GET/POST 
*   Changed signature for LTEmC create and execute
*   Added granular locking and general usability enhancements to ATCMD module
*   Migrated TLS configuration for a connection into its own module ltemc-tls
*   Changed signature of mqtt_open() to only specify TLS enabled.
*   IOP was re-architected to streamline buffer only operations. Application now provides and owns receive data buffers, allowing for more flexibility and minimum memory usage base on your application's requirements.
*   Support for multiple MQTT and HTTP(S) sessions per modem
*   Decoupled MQTT from fixed data context number 5, HTTP created without fixed data context coupling.
*   Misc fixes to BGx, MQTT, Network modules.

## v1.0.0 - MAJOR RELEASE
### Version 1.0.0 is a major release with several breaking changes. This release is the 1st version fully vetted in commercial projects and sets the stage for future multiple LooUQ LTE modem models.

https://github.com/LooUQ/CircuitRiver-LTEmC/blob/master/Release_1_0.md

You are encouraged to review the Release_V1-0.md file for specifics on the major changes and how to adapt your application code.
*   Project rename: LTEm1c to LTEmC, as this project will be adding support for future LooUQ LTE modems in Q3-2021
*   New instantiation (create) and startup process
*   Support for optional protocol inclusions
*   Support for future shared code support of different modem models (modules)
*   Improved support for Segger RTT debug output
*   Filename and .h file adjustments and cleanup
*   Misc fixes

## v0.5.0 - Fixes and Network PDP context handling (Dec 28, 2020)
* Fixed MQTT not trapping publish errors
* Fixed ACTION parsing on multistep actions with custom response parser (affected MQTT publish)
* Changed NETWORK module PDP context handling
    * Changed internal active PDP table organization
    * Change (_ntwk) function signatures to better match behaviors
    * Added reset to deactivate/activate all presently active PDP contexts
* Change IOP PDP deactivated detect logic to match internal implementation in NETWORK module

## 0.4.2 (Dec 15, 2020)
*	Updated ISR for intermittent missed event when receiving MQTT publish
*	Reformatted tests/LTEm1c-8-mqtt.ino for readability and test setup with Azure IoTHub
*	Removed utils.c from LTEm1c library
*	Functionality is now part of LQ Cloud but can be used independently as lqc_collections (.h\.c)

## v0.4.2 - Misc (Dec-15-2020)
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
    * Subscribe/unsubscribe to topics
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
First release of a functional LTEm1 driver. Supported sockets (TCP/UDP/SSL) and limited MQTT. 
* Pure C99
* Hardware/platform abstraction. Tested on SAMD21 and Arduino foundation but adaptable to RTOS 
* Abstraction of AT commands workflow, simple methods allow for invoke and await pattern
* Minimal copy design
* Network (cellular) connectivity, GNSS, and AT command support
* Initial TCP/UDP send/recv (very small packets)
* GNSS (GPS) support
* Tests (VS Code Arduino) for above
* Central IOP (input/output processor) to handle BGx communications and buffer management
* Implements TCP/UDP/SSL clients using a Sockets pattern with receiver events
* Started MQTT support

