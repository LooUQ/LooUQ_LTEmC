
#define ENABLE_DIAGPRINT                    // expand DIAGPRINT into debug output
//#define ENABLE_DIAGPRINT_VERBOSE            // expand DIAGPRINT and DIAGPRINT_V into debug output
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

// LTEmC Includes
#include <ltemc-internal.h>                                     // this appl performs tests on internal, non-public API components 
#include <ltemc-iop.h>

// LTEmC variables
// cBuffer_t rxBffr;                                            // cBuffer control structure
// cBuffer_t* rxBffrPtr = &rxBffr;                              // convenience pointer var
// char rawBuffer[220] = {0};                                   // raw buffer managed by rxBffr control

// test controls
bBuffer_t* rxBffrPtr;                                           // convenience pointer var
char hostBffr1[255] = {0};                                      // display buffer to receive received info from LTEmC
char hostBffr2[255] = {0};                                      // display buffer to receive received info from LTEmC

uint16_t loopCnt = 0;
uint16_t cycle_interval = 2000;
uint32_t lastCycle;
uint16_t errorCnt = 0;
resultCode_t rslt;

// custom ASSERT processing for this test
#ifdef ASSERT
    #undef ASSERT
    #define ASSERT(b,s) if(!b) indicateFailure(s)
#endif

#define APPRDY_TIMEOUT 8000

void setup() 
{
    #ifdef DIAGPRINT_SERIAL
        Serial.begin(115200);
        delay(5000);                // just give it some time
    #endif
    DPRINT(PRNT_RED,"\n\n*** ltemc-03-iopisr started ***\n\n");
    randomSeed(analogRead(0));
    // lqDiag_setNotifyCallback(appEvntNotify);                 // configure LTEMC ASSERTS to callback into application

    ltem_create(ltem_pinConfig, NULL, appEvntNotify);           // create LTEmC modem (no yield CB for testing)
    ltem_start(resetAction_powerReset);

    //(optionally) override LTEm created buffer with test instance
    // cbffr_init(rxBffrPtr, rawBuffer, sizeof(rawBuffer));
    // g_lqLTEM.iop->rxBffr = rxBffrPtr;
    rxBffrPtr = g_lqLTEM.iop->rxBffr;                           // convenience pointer

    char cmd[] = "ATE0\r";                                      // turn off cmd echo for cleaner result parsing
    IOP_startTx(cmd, strlen(cmd));
    pDelay(500);
    DPRINT(PRNT_DEFAULT, "ATE0 response:");
    DPRINT(PRNT_INFO, "%s\r\n", rxBffrPtr->tail);
    bbffr_reset(rxBffrPtr);                                     // discard response

    lastCycle = pMillis() + cycle_interval;
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
        uint8_t expectedCnt = 79;                                          // set dependent on cmds invoked on BGx

        DPRINT(0, "Sending (%d) %s\r\n", strlen(cmd), cmd);
        IOP_startTx(cmd, strlen(cmd));                                      // start send and wait for complete (ISR handles transfers out until complete)
        pDelay(1000);                                                        // give BGx some time to respond, interrupt will fire and fill rx buffer

        uint16_t occupiedCnt = bbffr_getOccupied(rxBffrPtr);                // move to variable for break conditional
        if (occupiedCnt == expectedCnt)
            DPRINT(PRNT_GREEN, "Got %d chars of %d expected\r\n", occupiedCnt, expectedCnt);
        else
            DPRINT(PRNT_WARN, "Got %d chars of %d expected\r\n", occupiedCnt, expectedCnt);
        ASSERT(occupiedCnt == expectedCnt, "Buffer occupied not as expected");

        char* copyFrom;
        uint16_t firstCnt = (int)(expectedCnt / 2);
        uint16_t blockSz1;
        uint16_t blockSz2;

        if (loopCnt % 2 == 1)
        {
            DPRINT(PRNT_GREEN, "\r\rUsing POP\r\n");
            memset(hostBffr1, 0, sizeof(hostBffr1));                            // make it easy for str functions, PRINTF, and human eyes

            bbffr_pop(rxBffrPtr, hostBffr1, sizeof(hostBffr1));                 // move everything in rxBffr to hostBffr
            DPRINT(PRNT_CYAN, "Resp(%d chars): %s\r\n", strlen(hostBffr1), hostBffr1);
        }
        else
        {
            DPRINT(PRNT_GREEN, "\r\rUsing POP BLOCK\r\n");                   // I know this is testing two things at once... sorry
            memset(hostBffr2, 0, sizeof(hostBffr2));                            // make it easy for str functions, PRINTF, and human eyes

            blockSz1 = bbffr_popBlock(rxBffrPtr, &copyFrom, expectedCnt);       // move everything in rxBffr to hostBffr
            memcpy(hostBffr2, copyFrom, blockSz1);
            bbffr_popBlockFinalize(rxBffrPtr, true);

            blockSz2 = bbffr_popBlock(rxBffrPtr, &copyFrom, expectedCnt);
            ASSERT(blockSz1 + blockSz2 == expectedCnt, "Total cBffr blocks not expected");
            if (blockSz2 > 0)
            {
                memcpy(hostBffr2 + blockSz1, copyFrom, blockSz2);
                bbffr_popBlockFinalize(rxBffrPtr, true);
            }
            DPRINT(PRNT_GREEN, "Blocks: 1=%d, 2=%d\r\n", blockSz1, blockSz2);

            DPRINT(PRNT_CYAN, "Resp(%d chars): %s\r\n", strlen(hostBffr2), hostBffr2);
        }
        occupiedCnt = bbffr_getOccupied(rxBffrPtr);                             // move to variable for break conditional
        DPRINT(PRNT_GREEN, "rxBffr has %d chars now.\r\n", occupiedCnt);

        if (loopCnt > 1)                                                        // skip on loop 1, only 1 buffer filled
        {
            uint16_t cmpFault = strcmp(hostBffr1, hostBffr2);
            ASSERT(cmpFault == 0, "Buffers do not compare as equal");
            ASSERT(occupiedCnt == 0, "bBffr is empty");
        }

        DPRINT(0,"Loop=%d \n\n", loopCnt);
     }
}


/* test helpers
========================================================================================================================= */

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


void indicateFailure(const char* failureMsg)
{
	DPRINT(PRNT_ERROR, "\r\n** %s \r\n", failureMsg);
    DPRINT(PRNT_ERROR, "IsrCount=%d  errors=%d\r\n", g_lqLTEM.isrInvokeCnt, errorCnt);
    DPRINT(PRNT_ERROR, "** Test Assertion Failed. \r\n");

    errorCnt++;
    #if 0
    DPRINT(PRNT_ERROR, "** Halting Execution \r\n");
    while (1)
    {
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        pDelay(1000);
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        pDelay(100);
    }
    #endif
}
