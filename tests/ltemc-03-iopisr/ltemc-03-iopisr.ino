
#define ENABLE_DIAGPRINT                    // expand DIAGPRINT into debug output
#define ENABLE_DIAGPRINT_VERBOSE            // expand DIAGPRINT and DIAGPRINT_V into debug output
#define ENABLE_ASSERT
//#include <jlinkRtt.h>                     // Use J-Link RTT channel for debug output (not platform serial)
#include <lqdiag.h>

/* specify the pin configuration 
 * --------------------------------------------------------------------------------------------- */
// #define HOST_FEATHER_UXPLOR             
// #define HOST_FEATHER_LTEM3F
// #define HOST_FEATHER_UXPLOR_L
#define HOST_ESP32_DEVMOD_BMS

#define PERIOD_FROM_SECONDS(period)  (period * 1000)
#define PERIOD_FROM_MINUTES(period)  (period * 1000 * 60)
#define ELAPSED(start, timeout) ((start == 0) ? 0 : millis() - start > timeout)
#define STRCMP(x,y)  (strcmp(x,y) == 0)

// LTEmC Includes
#include <ltemc-internal.h>                             // this appl performs tests on internal, non-public API components 
#include <ltemc-iop.h>

// LTEmC variables
cBuffer_t rxBffr;                                           // cBuffer control structure
cBuffer_t* rxBffrPtr = &rxBffr;                             // convenience pointer var
char rawBuffer[220] = {0};                                  // raw buffer managed by rxBffr control
char hostBffr1[255];                                        // display buffer to receive received info from LTEmC
char hostBffr2[255];                                        // display buffer to receive received info from LTEmC

// test controls
uint16_t loopCnt = 0;
uint16_t cycle_interval = 2000;
uint32_t lastCycle;
uint16_t errorCnt = 0;

// custom ASSERT processing for this test
#ifdef ASSERT
    #undef ASSERT
    #define ASSERT(b,s) if(!b) indicateFailure(s)
#endif


void setup() 
{
    #ifdef DIAGPRINT_SERIAL
        Serial.begin(115200);
        delay(5000);                // just give it some time
    #endif
    DIAGPRINT(PRNT_RED,"\n\n*** ltemc-03-iopisr started ***\n\n");
    randomSeed(analogRead(0));
    // lqDiag_setNotifyCallback(appEvntNotify);                 // configure LTEMC ASSERTS to callback into application

    ltem_create(ltem_pinConfig, NULL, appEvntNotify);           // create LTEmC modem (no yield CB for testing)
    startLTEm();                                                // test defined initialize\start, can't use ltem_start() for this test scenario

    cbffr_init(rxBffrPtr, rawBuffer, sizeof(rawBuffer));
    g_lqLTEM.iop->rxBffr = rxBffrPtr;                           // override LTEm created buffer with test instance

    char cmd[] = "ATE0\r\n";
    IOP_startTx(cmd, strlen(cmd));
    pDelay(100);
    cbffr_reset(rxBffrPtr);

    lastCycle = cycle_interval;
}


void loop() 
{
    if (ELAPSED(lastCycle, cycle_interval))
    {
        lastCycle = millis();
        loopCnt++;

        // Optional command strings for testing, must be from RAM (can't use const char* to create) and need to be set in loop (send is destructive)
        // char cmd[] = "AT+GSN;+QCCID\r\n";                                // short response (expect 57 char response)
        char cmd[] = "AT+GSN;+QCCID;+GMI;+GMM\r\0";                         // long response (expect 79 char response)
        // char cmd[] = "AT+QPOWD\r\n";                                     // Is something wrong? Is is rx or tx (tx works if BG powers down)
        uint8_t expectedCnt = 79;                                           // set dependent on cmds invoked on BGx

        PRINTF(0, "Sending (%d) %s\r\n", strlen(cmd), cmd);
        IOP_startTx(cmd, strlen(cmd));                                      // start send and wait for complete (ISR handles transfers out until complete)
        pDelay(500);                                                        // give BGx some time to respond, interrupt will fire and fill rx buffer

        uint16_t occupiedCnt = cbffr_getOccupied(rxBffrPtr);                // move to variable for break conditional
        PRINTF(dbgColor__green, "Got %d chars of %d expected\r\n", occupiedCnt, expectedCnt);
        ASSERT(occupiedCnt == expectedCnt, "Buffer occupied not as expected");

        char* copyFrom;
        uint16_t firstCnt = (int)(expectedCnt / 2);
        uint16_t blockSz1;
        uint16_t blockSz2;

        if (loopCnt % 2 == 1)
        {
            PRINTF(dbgColor__green, "\r\rUsing POP\r\n");
            memset(hostBffr1, 0, sizeof(hostBffr1));                            // make it easy for str functions, PRINTF, and human eyes

            cbffr_pop(rxBffrPtr, hostBffr1, sizeof(hostBffr1));                 // move everything in rxBffr to hostBffr
            PRINTF(dbgColor__cyan, "Resp(%d chars): %s\r\n", strlen(hostBffr1), hostBffr1);
        }
        else
        {
            PRINTF(dbgColor__green, "\r\rUsing POP BLOCK\r\n");                   // I know this is testing two things at once... sorry
            memset(hostBffr2, 0, sizeof(hostBffr2));                            // make it easy for str functions, PRINTF, and human eyes

            blockSz1 = cbffr_popBlock(rxBffrPtr, &copyFrom, expectedCnt);       // move everything in rxBffr to hostBffr
            memcpy(hostBffr2, copyFrom, blockSz1);
            cbffr_popBlockFinalize(rxBffrPtr, true);

            blockSz2 = cbffr_popBlock(rxBffrPtr, &copyFrom, expectedCnt);
            ASSERT(blockSz1 + blockSz2 == expectedCnt, "Total cBffr blocks not expected");
            if (blockSz2 > 0)
            {
                memcpy(hostBffr2 + blockSz1, copyFrom, blockSz2);
                cbffr_popBlockFinalize(rxBffrPtr, true);
            }
            PRINTF(dbgColor__green, "Blocks: 1=%d, 2=%d\r\n", blockSz1, blockSz2);

            PRINTF(dbgColor__cyan, "Resp(%d chars): %s\r\n", strlen(hostBffr2), hostBffr2);
        }
        occupiedCnt = cbffr_getOccupied(rxBffrPtr);                             // move to variable for break conditional
        PRINTF(dbgColor__green, "rxBffr has %d chars now.\r\n", occupiedCnt);

        if (loopCnt > 1)
        {
            uint16_t cmpFault = strcmp(hostBffr1, hostBffr2);
            ASSERT(cmpFault == 0, "Buffers do not compare as equal");
        }
        ASSERT(occupiedCnt == 0, "cBffr is empty");

        PRINTF(0,"Loop=%d \n\n", loopCnt);
     }
}


/* test helpers
========================================================================================================================= */

void startLTEm()
{
    // initialize the HOST side of the LTEm interface
	// ensure pin is in default "logical" state prior to opening
	platform_writePin(g_lqLTEM.pinConfig.powerkeyPin, gpioValue_low);
	platform_writePin(g_lqLTEM.pinConfig.resetPin, gpioValue_low);
	platform_writePin(g_lqLTEM.pinConfig.spiCsPin, gpioValue_high);

	platform_openPin(g_lqLTEM.pinConfig.powerkeyPin, gpioMode_output);		// powerKey: normal low
	platform_openPin(g_lqLTEM.pinConfig.resetPin, gpioMode_output);			// resetPin: normal low
	platform_openPin(g_lqLTEM.pinConfig.spiCsPin, gpioMode_output);			// spiCsPin: invert, normal gpioValue_high

	platform_openPin(g_lqLTEM.pinConfig.statusPin, gpioMode_input);
	platform_openPin(g_lqLTEM.pinConfig.irqPin, gpioMode_inputPullUp);

    spi_start(g_lqLTEM.platformSpi);

    QBG_reset(resetAction_powerReset);                                      // force power cycle here, limited initial state conditioning
    SC16IS7xx_start();                                                      // start (resets previously powered on) NXP SPI-UART bridge

    if (g_lqLTEM.deviceState != deviceState_appReady)
    {
        IOP_awaitAppReady();                                                // wait for BGx to signal out firmware ready
    }
    SC16IS7xx_enableIrqMode();
    IOP_attachIrq();
}


void appEvntNotify(appEvent_t eventType, const char *notifyMsg)
{
    if (eventType == appEvent_fault_assertFailed)
    if (eventType == appEvent_fault_assertFailed)
    {
        PRINTF(dbgColor__error, "LTEmC-HardFault: %s\r\n", notifyMsg);
    }
    else 
    {
        PRINTF(dbgColor__white, "LTEmC Info: %s\r\n", notifyMsg);
    }
    return;
}


void indicateFailure(const char* failureMsg)
{
	DIAGPRINT(PRNT_ERROR, "\r\n** %s \r\n", failureMsg);
    DIAGPRINT(PRNT_ERROR, "IsrCount=%d  errors=%d\r\n", g_lqLTEM.isrInvokeCnt, errorCnt);
    DIAGPRINT(PRNT_ERROR, "** Test Assertion Failed. \r\n");

    errorCnt++;
    #if 0
    DIAGPRINT(PRNT_ERROR, "** Halting Execution \r\n");
    while (1)
    {
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        pDelay(1000);
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        pDelay(100);
    }
    #endif
}
