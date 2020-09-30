# LTEm1/LTEm1c FAQ

**What module is the LTEm1 based on?**
> The LooUQ LTEm1 utilizes a Quectel BG96 module, FCC ID: XMR201707BG96

**Why the BG96?**
> The BG96 has gained wide respect and acceptance by the network carriers and offers both LTE IoT specific protocols: CAT-M1 and NB-IOT. Additionally the BG96 has a rich feature set including SSL/TLS, MQTT, HTTP(S), GNSS, geo-fencing, and even a persistant file store. LooUQ has coupled the BG96 with an SPI interface for high-speed communications without the burden of character-by-character host interrupts. For Raspberry PI applications, the LTEm1 and BG96 support access via USB (micro-USB connector).

**Is there software for the LTEm1?**
> Yes indeed. The LTEm1c library, aka this repository, is LooUQ's open-source driver written in C99 to support the LTEm1 hardware with POSIX style functionality.
