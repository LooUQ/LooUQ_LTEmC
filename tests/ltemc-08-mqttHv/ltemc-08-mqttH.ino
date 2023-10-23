

#define ENABLE_DIAGPRINT                    // expand DPRINT into debug output
//#define ENABLE_DIAGPRINT_VERBOSE            // expand DPRINT and DPRINT_V into debug output
#define ENABLE_ASSERT
#include <lqdiag.h>

/* specify the pin configuration 
 * --------------------------------------------------------------------------------------------- */
#ifdef ARDUINO_ARCH_ESP32
    #define HOST_ESP32_DEVMOD_BMS
#else
    #define HOST_FEATHER_UXPLOR_L
    // #define HOST_FEATHER_UXPLOR             
    // #define HOST_FEATHER_LTEM3F
#endif

#define PDP_DATA_CONTEXT 1
#define PDP_APN_NAME "hologram"

#define PERIOD_FROM_SECONDS(period)  (period * 1000)
#define PERIOD_FROM_MINUTES(period)  (period * 1000 * 60)
#define ELAPSED(start, timeout) ((start == 0) ? 0 : millis() - start > timeout)
#define STRCMP(n, h)  (strcmp(n, h) == 0)

// LooUQ HiveMQ
#define MQTT_SERVER "5e1f31d52a144f1bb4b2bcb1b56ab5c8.s2.eu.hivemq.cloud"
#define MQTT_PORT 8883
#define MQTT_DATACONTEXT (dataCntxt_t)0
#define MQTT_TOPIC "bms"
#define MQTT_DEVICEID "bms-1"
#define MQTT_USERID "loouq"
#define MQTT_PASSWORD "RjAGYn7NH5bj"


// /* You will need to set the values for the MQTT_* defines to match your HiveMQ account environment
//  *
//  * SERVER, TOPIC, DEVICEID, USERID and PASSWORD values below are for example and MUST be changed.
//  * 
//  * - TOPIC can be your entire enterprise or a whatever grouping is appropriate. 
//  * - DEVICEID should be unique to each IoT device communicating with HiveMQ.
//  * - USERID and PASSWORD are the account details you established when setting up your HiveMQ account.
// */
// #define MQTT_SERVER "<long-hex-string>.s2.eu.hivemq.cloud"
// #define MQTT_PORT 8883
// #define MQTT_DATACONTEXT (dataCntxt_t)0
// #define MQTT_TOPIC "hmq-demo"
// #define MQTT_DEVICEID "hmq-1"
// #define MQTT_USERID "<yourUserID>"
// #define MQTT_PASSWORD "<yourPassword>"

// LTEmC Includes
#include <ltemc.h>
#include <lq-str.h>
#include <ltemc-tls.h>
#include <ltemc-mqtt.h>
#include <ltemc-gnss.h>

// test controls
uint8_t testPattern;
uint16_t loopCnt = 0;
uint16_t msgId = 100;
uint16_t cycle_interval = 10000;
uint32_t lastCycle;


#define MQTT_MESSAGE_SZ 1547

// LTEm variables
tlsCtrl_t tlsCtrl;                      // SSL/TLS settings
mqttCtrl_t mqttCtrl;                    // MQTT control, data to manage MQTT server connection
// mqttTopicCtrl_t topicCtrl;
// char mqttTopic[200];                 // application buffer to craft TX MQTT topic, if topic changes per message (like Azure IoTHub)
// char mqttTopicProp[200];             // required for IoTHub topic template
char mqttMsg[MQTT_MESSAGE_SZ];          // application buffer to craft TX MQTT publish content (body)
resultCode_t rslt;
uint16_t msgSz = 0;


void setup() 
{
    #ifdef DIAGPRINT_SERIAL
        Serial.begin(115200);
        delay(5000);                // just give it some time
    #endif
    DPRINT(PRNT_DEFAULT,"\n\n*** LTEmC Test: ltemc-08-mqttH started ***\n\n");

    ltem_create(ltem_pinConfig, NULL, appEvntNotify);
    ntwk_setDefaultNetwork(PDP_DATA_CONTEXT, pdpProtocol_IPV4, PDP_APN_NAME);
    ltem_start(resetAction_swReset);
    DPRINT(PRNT_DEFAULT, "-----------------------\r\n");

    DPRINT(PRNT_DEFAULT, "Waiting on network...\r\n");
    providerInfo_t *provider = ntwk_awaitProvider(PERIOD_FROM_SECONDS(15));
    while (strlen(provider->name) == 0)
    {
        DPRINT(PRNT_WARN, ">");
    }
    DPRINT(PRNT_INFO, "Network type is %s on %s\r\n", provider->iotMode, provider->name);

    /* Basic connectivity established, moving on to MQTT setup with HiveMQ
     * HiveMQ requires TLS, MQTT version 3.11, and SNI enabled
     * ----------------------------------------------------------------------------------------- */
    // tls_configure(dataCntxt_0, tlsVersion_tls12, tlsCipher_default, tlsCertExpiration_default, tlsSecurityLevel_default);
    // tls_configSni(dataCntxt_0, true);

    tls_initControl(&tlsCtrl, tlsVersion_tls12, tlsCipher_default, tlsCertExpiration_default, tlsSecurityLevel_default, true);
    mqtt_initControl(&mqttCtrl, MQTT_DATACONTEXT);
    // mqtt_initTopicControl(&topicCtrl, MQTT_IOTHUB_C2D_TOPIC, mqttQos_1, mqttRecvCB);
    // mqtt_subscribeTopic(&mqttCtrl, &topicCtrl);
    mqtt_setConnection(&mqttCtrl, MQTT_SERVER, MQTT_PORT, &tlsCtrl, mqttVersion_311, MQTT_DEVICEID, MQTT_USERID, MQTT_PASSWORD);

    DPRINT(PRNT_INFO, "MQTT Initialized\r\n");
    mqtt_start(&mqttCtrl, true);
    DPRINT(PRNT_INFO, "MQTT Started\r\n");

    // prepare for running loop
    randomSeed(analogRead(0));
    lastCycle = cycle_interval;
}


void loop() 
{
    if (ELAPSED(lastCycle, cycle_interval))
    {
        lastCycle = millis();
        loopCnt++;
        msgId = loopCnt + 100;
        uint32_t publishTck = pMillis();

        uint32_t windspeed = random(0, 4999);
        msgSz = snprintf(mqttMsg, sizeof(mqttMsg), "bms-poc,%s,%d,%d", MQTT_DEVICEID, loopCnt, windspeed);
        DPRINT(PRNT_DEFAULT, "Sending MQTT Message: %s\r\n", mqttMsg);

        rslt = mqtt_publish(&mqttCtrl, MQTT_TOPIC, mqttQos_1, mqttMsg, strlen(mqttMsg), 30);
        if (rslt != resultCode__success)
        {
            DPRINT(PRNT_ERROR, "Publish Failed! >> %d\r\n", rslt);
        }

        DPRINT(PRNT_DEFAULT,"Loop=%d \n\n", loopCnt);
    }
    /* NOTE: ltem1_eventMgr() background pipeline processor is required for async receive operations; like MQTT topic subscriptions.
     *       Event manager is light weight and has no side effects other than taking time. It should be invoked liberally. 
     */
    ltem_eventMgr();
}


/* Test Helpers
 * ============================================================================================= */


void indicateFailure(const char* failureMsg)
{
	DPRINT(PRNT_ERROR, "\r\n** %s \r\n", failureMsg);
    DPRINT(PRNT_ERROR, "** Test Assertion Failed. \r\n");

    int halt = 1;
    DPRINT(PRNT_ERROR, "** Halting Execution \r\n");
    while (halt)
    {
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        delay(1000);
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        delay(100);
    }
}


//typedef void (*eventNotifFunc_t)(uint8_t notifType, uint8_t notifAssm, uint8_t notifInst, const char *notifMsg);

void appEvntNotify(appEvent_t eventType, const char *notifyMsg)
{
    if (eventType == appEvent_fault_assertFailed)
    if (eventType == appEvent_fault_assertFailed)
    {
        DPRINT(PRNT_ERROR, "LTEmC-HardFault: %s\r\n", notifyMsg);
    }
    else 
    {
        DPRINT(PRNT_WHITE, "LTEmC Info: %s\r\n", notifyMsg);
    }
    return;
}
