# LTEmC Design Assumptions


## Code Conventions
* The LTEmC driver is split into numerous files. Some expose interfaces for public (API) consumption, some are intended for only internal use.
* Intended scope or visibility is indicated with function prefixes
    - {module_name}_ prefix indicates a public function intended for consumption by the device application.
    - S_ is a static local function scoped to the current compilation unit
    - {MODULE_NAME}_ prefix indicates internal to LTEmC and not intended for application use. *These are being rounded up for a future move to the ltemc-internal.h header.*
* Source files attempt to use #pragma regions to keep major blocks identifiable.
    - Regions in all files (wip) are: "LTEm Internal Functions", "Public Functions", "Static Function Definitions"
* **Code cleanup to adhere to these conventions is a focus for the version 2.1 release in July 2021**


## Debug Defines
LooUQ uses the Segger J-Link as a core part of our toolkit. Beyond breakpoints and flashing memory, the J-Link software provides a facility for high-speed terminal streams to a viewer on your development computer. The RTT, or real-time terminal, facility makes it relatively straight forward to report out debugging information. The PRINTF and DBGCOLOR_ macros are designed to take advantage of the RTT facility. Additionally, the RTT viewer can display debugging information in color, the output color is specified with the DBGCOLOR_ macros. The PRINTF macros can fall-back to using the much slower serial capabilities of the Arduino platform. 

Segger RTT Facility: https://wiki.segger.com/UM08001_J-Link_/_J-Trace_User_Guide


## Using ASSERTS and Notification Callbacks
The LTEmC software was designed to be a subsystem within your application. 

### ASSERTS
ASSERTs are macro constructs that signal a serious, and likely unrecoverable condition. They should stay in production code to prevent "weird" errors downstream of the problem. ASSERTS use the ltem_notifyApp() function if it is registered at application start. ASSERTs can also, optionally write a lqDiagnostics_t information block with information about the file, line and conditions of the failed ASSERT. To fully disable ASSERTs, you will need to define the NO_ASSERTS macro in your make workflow or in the lq-assert.h header file (part of LooUQ-Common library).

The lqDiagnostics_t information block can be configured to survive a device reset (provided power stays on to memory). This allows for collection and reporting of this information remotely on reset. For information on how to accomplish this see the article on *lqDiagnostics* available on https://answers.loouq.com. For a working example, the LQCloud client automatically reports this information for a system or watchdog reset; there is a working example in examples\LQCloudTest in the LQCloud Client repository on GitHub.

### WARNING 
WARNINGs are macro constructs that are intended for use during development. They won't break code if accidently left on in production but they will occupy code space. In the few spots where these are implemented they are there to check for non-optimal parameters in a function call. Example: a buffer size that will create unused, but allocate memory.

### ltem_notifyApp()
The **ltem_notifyApp(uint8_t notifyType, const char *notifyMsg)** function is an optional facility, but STRONGLY recommended, to allow the LTEmC driver to signal to the application both recoverable and non-recoverable events. ASSERTS will use this facility if provided with 

The ltem_notifyApp() callback function you provide can perform various important tasks, such as:
* Retries for recoverable failures in hardware, network or protocol application areas. Example; power cycle modem for hardware type notification and restart the communication stack.
* Local display or indications of important conditions (examples: write messages to an OLED display, or set the color of a NeoPixel).
* Logging to flash or a SD-Card of events


## Extensibility Points
LTEmC is extensible. LooUQ believes that the major use cases are covered, but there are hundreds of AT commands on the BGx family. The guide to extensibility is below:
* **atcmd** is the primary extension API. This module handles the IO and command completion for you, with minimal assistance (see completion parsers below).
* It is recommended that you avoid changes to the modules below, as these have dependencies across the LTEmC software.
    - **nxp-sc16is**
    - **quectel-bg**
    - **iop** 
* If you are porting LTEmC to non-Arduino like hardware, you will need to supply the API specified in the lq-platform.h header. If you need help or some directions, please post a question on the answers.loouq.com site.


## IOP - The I/O Processor
IOP performs the duties of direct communications with the LTEm device. This is via the NXP UART/SPI bridge to the BGx module.

* IOP uses two types of receive (RX) buffers: 
  - A single core buffer that is used for AT command responses AND to receive async network messages
  - One or more (one per protocol control) RX data buffers to receive incoming payload data. Note: that the *data* RX buffer is split into 2 pages to allow for simultaneous (like) filling and emptying.
* IOP assumes that the current buffer page, indicated by the ***buffer->iopPg*** is valid and empty at the start of an ISR (interrupt event). If the page has not yet been completely emptied that is a buffer overflow and data loss will acure.

