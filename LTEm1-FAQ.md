# LTEm1/LTEmC FAQ

**What module is the LTEm1 based on?**
> The LooUQ LTEm1 utilizes a Quectel BG96 module, FCC ID: XMR201707BG96

**Why the BG96?**
> The BG96 has gained wide respect and acceptance by the network carriers and offers both LTE IoT specific protocols: CAT-M1 and NB-IOT and fallback to 2G where necessary. Additionally the BG96 has a rich feature set including SSL/TLS, MQTT, HTTP(S), GNSS, geo-fencing, and even a persistant file store. LooUQ has coupled the BG96 with an SPI interface for high-speed communications without the burden of character-by-character host interrupts. For Raspberry PI applications, the LTEm1 and BG96 support access via USB (micro-USB connector).

**Is there software for the LTEm1?**
> Yes indeed. This repository is the LTEmC library is LooUQ's open-source driver written in C99 to support the LTEm1 and future LooUQ LTE modem hardware with POSIX style functionality.

**What are the physical characteristics of the LTEm1?**
> The LTEm1 measures 40mm x 48mm and under 9mm in height. The device requires standard power source of 3.7 volts (LiPo battery). For hosts with ample surge capacity the LTEm1 can run directly from the regulated supply. For most hosts, LooUQ recommends incorporating a small 3.7v LiPo for instantaneous current demands (like turn on), but also to power the LTEm1 in a power failure for a short period time, this allows alerts on the power condition to be sent out to the devices owner. Connections to the LTE and GNSS antennas are via standard U.FL/IPEX connectors.

**Seems like LTEm1 has many features, but what if I don't need all of them?**
> No Problem. The source code of the LTEm1c library allows you to not include optional LTEm1 feature subsystems and save host memory. Optional modules include: GNSS (aka GPS), Geo-Fencing (geo-fencing depends on GNSS), Sockets, MQTT, HTTP, FTP, FOTA, and File System. The easiest way to see all the options is to look at the readme.md file in the respository root. There is a diagram there showing the optional subsystems.

**How much memory does the LTEm1c code take?**
> On a SAMD21 system (Adafruit Feather M0) the code occupies about 40K of flash and uses approximately 10K of RAM. These values are based on a system built with Sockets, MQTT, and GNSS with one external peer server. Updated information on memory footprint will be published with future updates.
