# Circuit River - LTEm1c 

LooUQ Circuit River
LTEm1 modem driver implemented in C99 for portability and a small footprint.

## Pre-Release
----

The LooUQ LTEm1c driver supports the LooUQ Circuit River LTEm1 LTE modem. The LTEm1 is designed to allow for cost-effective cellular communications in embedded systems by supporting LTE CAT-M1 and LTE CAT-NB1 (NB-IOT) communications. The LTEm1 is certified for use on the Verizon network and a registered device for the Sprint (T-Mobile) Curiosity Network. It is also fully tested and supported using Hologram SIMs.

[LTEm1 Modem Specifications](https://drive.google.com/uc?id=1amRN84WPJqlxu36hTU_9TU1F0aX2Kv86)

----
![LTEm1c](https://drive.google.com/uc?id=1PMFjFoy0ToDR7PuwlXjVdbManJMVNh15)

All of the core (blue) subsystems are functional. Extension subsystems (copper, with wide-borders) are optional and can be built into the application, or omitted to reduced code size. Grey boxes indicate future feature areas. 

## Version History ##
* v0.1 Initial publish
  * Network (cellular) connectivity, GNSS, and AT command support
  * Initial TCP/UDP send/recv (very small packets)
  * Tests (Arduino) for above
* v0.2 (June 25) TCP/UDP large packets (<=1460 bytes), MQTT(s) functional, Geo-Fence support
  * Improved performance in interrupt servicing
  * Some memory optimizations
  * Background tasks alignment for RTOS and Arduino Scheduler compatibility
  * Geo-Fence support 
  * FOTA partial support for DFOTA via HTTPS 
