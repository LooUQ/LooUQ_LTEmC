# LTEm1c Version History

## v0.2 - IOP Redesign
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

## v0.1 - Initial publish
First release of a functional LTEm1 driver. Supported sockets (TCP\UDP\SSL) and limited MQTT. 
* Pure C99
* Hardware/platform abstraction. Tested on SAMD21 and Arduino foundation but adaptable to RTOS 
* Abstraction of AT commands workflow, simple methods allow for invoke and await pattern
* Minimal copy design
* Central IOP (input\output processor) to handle BGx communications and buffer management
* Implements TCP\UDP\SSL clients using a Sockets pattern with receiver events
* Started MQTT support
