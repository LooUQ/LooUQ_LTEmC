# LTEmC Tests/Examples 

Included with the LTEmC source files, there are numerous test/example projects configured for a Arduino .ino files. This is a common format the is widely avaialable and easy to install, regardless of the final target for your use of your selected LTEm device. 

The test/example projects are split into two different categories:
* Testing your environment, usually your port/platform abstractions or physically testing your hardware.
* Learning a new functional area provided by the LTEm system or testing the area to better understand the behavior of the area (sub-system) in your use case.

### *Tests 01 - 04 target testing the basic interaction between LTEmC (the software) and the LTEm device; especially tests 1-3. Each test builds upon successful results from the previous test in the series.*

**ltemc-01-platform:** This project tests the basic I/O abstractions and SPI interface. This test performs a repeated sequence of GPIO and SPI transactions in order to validate the pinout and basic communications with the LTEm device.

**ltemc-02-bgx:** The BGx project extends the platform test to also include the BGx module in the dialog. This is really a device hardware test. If it fails when test 1 passes, this indicates a device problem: most likely the SPI/TX jumper removed or in the wrong position (LTEmC only operates with LTEm devices in SPI mode).

**ltemc-03-iopisr:** This project tests interrupt functionality. Moving on from the previous tests, were I/O was completed in polled operations. Also this test utilizes the two-layer buffering that enables high-speed communications with minimal host MCU overhead.

**ltemc-04-atcmd:** This project is the finally in the platform tests. Successful results indicates that the LTEmC/LTEm device interface is functional and can be utilized by all of the feature consumers in the LTEm used (protocols, network registration, files, etc.).

### *The remainder of the test/example projects are intended to show you how to use various features of the LTEm to accomplish your project's goals.*

**ltemc-05-modeminfo:** This project demonstrates the basics of talking to the LTEm device (modem). It shows how to create a modem object and reference it, it also shows how to get basic information about the modem and the cellular network you are attached to.

**ltemc-06-gnss:** If you plan to use the GNSS (GPS in the US) or geo-fencing features of the LTEm this example shows you how to get started. If the LTEm device features a BG95-M3 or BG77 module, this example demonstrates the steps necessary to switch RF priority (radio path).

**ltemc-07-sockets:** While not as common these days, the LTEm support direct socket communications (as a client) for UDP/TCP and optionally laying on TCP you can use SSL/TLS. These protocols are a bit tricky to implement, feel free to reach out if you have questions.

**ltemc-08-mqttA:** One of the two most common IoT communication protocols, so we have provided two examples. This example demonstrates how to connect to Azure IoTHub over MQTT directly from the BGx module. No Azure client is required for the illustrated use (we do however make use of IoTHub Explorer to generate SAS authentication tokens and send/receive test traffic).

**ltemc-08-mqttH:** A frequent need is for MQTT to connect to one of the brokers available. Rather than force you to setup a Mosquito server, we are using HiveMQ. You can get a free HiveMQ account to test with on you own. Using this example and a HiveMQ account you can perform two-way communications server to your project (we use MQTTX to connect to HiveMQ for testing).

**ltemc-09-http:** The other IoT workhorse protocol HTTP(S) is also directly supported by the LTEm series devices. LTEm devices support GET and POST actions and can operate directly from your code'd data structures or also in the background using the modem's filesystem.

**ltemc-10-files:** As introduce above, the LTEm devices have a flash filesystem for persistent storage of information used in your application. The exact amount varies slightly, but it remains around 2MB making it viable for configuration data, calibration data or even flash images.

**ltemc-11-gpio:** For those of you with the CR-LTEm3F (based on the BG77 module) the LTEmC software allows you to extend your I/O and Analog capabilities via the modules I/O facilities. This example shows you how to read/write I/O ports (single-bit) and read analog voltage inputs.

