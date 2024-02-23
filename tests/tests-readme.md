# LTEmC Tests

LooUQ provides a series of tests to validate basic LTEmC operations. These are used by LooUQ internally to evaluate "porting" efforts to new MCU/framework targets. They can also be useful for troubleshooting issues encountered during device application development. The current suite of 4 tests go from basic GPIO/SPI validation up to full interrupt based command execution.

There is a user focus suite of "examples" for common use-case scenarios to assist you with accelerating your IoT project/product. These are found in the **\examples** folder.


## ltemc-01-platform
This test performs a continuous loop of I/O operations to verify GPIO mapping and SPI basic functionality. 
***

## ltemc-02-bgx
Adds to platform the Quectel module (BGx/EGx). This validates that SPI/UART translation is working properly and a complete path between host system and the Quectel module is functioning.
***

## ltemc-03-iopisr
The above tests essentially operate in "polled" mode where the host is waiting on events to proceed. This test adds IOP sw-module (Input/Output Processor) support for interrupts to signal modem message status.
***

## ltemc-04-atcmd
The AT-CMD sw-module is the command and control section of LTEmC and is responsible for orchestration of actions and data transfers. This test is the culmination of complete baseline functionality that all service modules (MQTT, HTTP, network, GNSS/GEO, etc.) rely on.
***

***LooUQ is happy to assist anyone wishing to port LTEmC to other frameworks or host MCUs. As of version 4.0.0, LTEmC supports Arduino and ESP32 (currently tested as an Arduio project); LooUQ is in the process of testing LTEmC under ESP-IDF/FreeRTOS.***

***LTEmC is written to C99 dependencies. The tests above and the examples in the companion dirctory are created as .INO projects due to the familiarity with Arduino (as a least common denominator)***

***If you are looking to port LTEmC, please reach out and we will help you plan your efforts.***

