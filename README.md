# Circuit River - LTEm1c 

LooUQ Circuit River
LTEm1 modem driver implemented in C99 for portability and a small footprint.

----

The LooUQ LTEm1c driver supports the LooUQ Circuit River LTEm1 LTE modem. The LTEm1 is designed to allow for cost-effective cellular communications in embedded systems by supporting LTE CAT-M1 and LTE CAT-NB1 (NB-IOT) communications. The LTEm1 is certified for use on the Verizon network and a registered device for the Sprint (T-Mobile) Curiosity Network. It is also fully tested and supported using Hologram SIMs.

[LTEm1 Modem Specifications](https://drive.google.com/uc?id=1amRN84WPJqlxu36hTU_9TU1F0aX2Kv86)

----
![LTEm1c](https://drive.google.com/file/d/1PMFjFoy0ToDR7PuwlXjVdbManJMVNh15/view?usp=sharing)

Blue subsystems are functional, FOTA/MQTT are partially implemented, grey boxes indicate future feature areas. Boxes with wide borders are option items that can be built into application or omitted to reduced code size.

## Version History ##
* v0.1 Initial publish
  * Network (cellular) connectivity, GNSS, and AT command support
  * Initial TCP/UDP send/recv (very small packets)
  * Tests (Arduino) for above
* v0.2 (early June) TCP/UDP large packets (<=1460 bytes), MQTT(s) functional
  * Improved performance in interrupt servicing
  * Some memory optimizations
  * Background tasks alignment for RTOS and Arduino Scheduler compatibility
