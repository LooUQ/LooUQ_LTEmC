# Circuit River - LTEm1c 

LooUQ Circuit River
LTEm1 modem driver implemented in C99 for portability and a small footprint.

----

![LTEm1c](https://drive.google.com/uc?id=1PMFjFoy0ToDR7PuwlXjVdbManJMVNh15)

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
