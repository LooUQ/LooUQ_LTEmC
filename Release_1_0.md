Release 1.0

Release 1.0 is a milestone for the LTEm1c software. The modem and the software have been used in 2 commercial projects and have evolved to support ideas presented in those projects. 

# New\Changed Functionality
* Application callbacks for yield and notifications
* Modular build, only include protocols needed
* Separate create() and start() 
* Cleaner support for optional debug printing to J-Link RTT or platform serial port
* And of course... FIXES

# Details
## Callbacks
Two callback conventions have be formalized: yield and notification. The yield callback allows for the application to invoke watchdog keepalives whenevr the LTEm1c code passes through a wait for response block. This is optional and is specified with the *ltem1_setYieldCb(yieldCb)* method. The yieldCb() method takes no parameters and returns a void.

The other callback is for notifying your application of significant events, such as: connecting, disconnecting and errors. This is compatible with the notification callback used in the LQCloud driver. An example callback handler is shown below. The example below shows how the device application can respond to events with local displays/indicators.

```
void notificationCB(uint8_t notifType, const char *notifMsg)
{
    if (notifType < lqcNotifType__INFO)
    {
        PRINTF(DBGCOLOR_info, "LQCloud Info: %s\r", notifMsg);
        #ifdef ENABLE_NEOPIXEL
        neoShow(NEO_YELLOW);
        #endif
        #ifdef ENABLE_OLED
        displayAlert(notifMsg);
        #endif
    }

    else if (notifType == lqcNotifType_connect)
    {
        PRINTF(DBGCOLOR_info, "LQCloud Connected\r");
        // update local indicators like LEDs, OLED, etc.
        #ifdef ENABLE_NEOPIXEL
        neoShow(NEO_GREEN);
        #endif
        #ifdef ENABLE_OLED
        displayClearDisplay();
        displayStatusBar(true, mdminfo_rssiBars(4));
        displayWake();
        #endif
    }
    else if (notifType == lqcNotifType_disconnect)
    {
        PRINTF(DBGCOLOR_warn, "LQCloud Disconnected\r");
        // update local indicators like LEDs, OLED, etc.
        #ifdef ENABLE_NEOPIXEL
        neoShow(NEO_MAGENTA);
        #endif
        #ifdef ENABLE_OLED
        displayStatusBar(false, mdminfo_rssiBars(4));

        char dbgMsg[21] = {0};
        uint8_t pdpCount = ntwk_getActivePdpCntxtCnt();
        uint8_t mqttStat = mqtt_status("", true);
        snprintf(dbgMsg, 20, "LQCdbg pdp=%d mqt=%d", pdpCount, mqttStat);
        displayAlert(dbgMsg);
 
        // displayAlert("No Cloud  Connection");                   // format for auto-split at 10-10-10 chars
        lDelay(PERIOD_FROM_SECONDS(3));
        #endif
    }
    else if (notifType > lqcNotifType__CATASTROPHIC)
    {
        PRINTF(DBGCOLOR_error, "LQCloud-HardFault: %s\r", notifMsg);
        // update local indicators like LEDs, OLED, etc.
        #ifdef ENABLE_NEOPIXEL
        neoShow(NEO_RED);
        #endif
        #ifdef ENABLE_OLED
        displayError(notifMsg);
        #endif

        // try to gather diagnostic info in escalating detail; if later inquiries fail, got something
        lqDiagInfo.notifCd = notifType;
        memcpy(lqDiagInfo.notifMsg, notifMsg, 20);
        lqDiagInfo.mqttState = lqc_getConnectState("", false);          // get MQTT state without forcing I/O to transport
        // now gather data requiring functioning LTEm1 driver
        lqDiagInfo.lteState = (ntwk_getPdpCntxt(1) == NULL) ? 0 : 1;
        lqDiagInfo.mqttState = lqc_getConnectState("", true);           // now try to get current MQTT state from transport

        NVIC_SystemReset();                                             // OR watchdog wait
        while (true) {}                                                 // should never get here
    }
}
```
## New Setup and Start, Protocol Inclusion
The create code, start code and the inclusion of protocols has been separated into distinct methods.  The new approach is to create the required features, then issue a ltem1_start() call. 

```
    ltem1_create(ltem1_pinConfig, appNotifyCB);         // otherwise reference your application notification callback
    //ltem1_create(ltem1_pinConfig, NULL);              // if application doesn't implement a notification callback, provide NULL
    ltem1_start(pdpProtocol_none);                      // start LTEm1 with only IOP configured, AT commands but no protocols. 
```

## Debug Printing
Debugging is of course a necessary part of our lives as developers. For those of you using Segger J-Link products I recommend the RTT (real time terminal) functionality the J-Link software provides. The inclusion of a small macro block makes turning on/off debugging output using J-Link RTT a simple one line change. LooUQ has a Arduino compatible fork of the Segger J-Link RTT source available on GitHub ().

```
#define _DEBUG 0                        // set to non-zero value for PRINTF debugging output, 
// debugging output options             // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if defined(_DEBUG) && _DEBUG > 0
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG == 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#endif
```