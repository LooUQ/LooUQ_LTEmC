/******************************************************************************
 *  \file ltemc-3-iop.ino
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED
 * "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
 * LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 ******************************************************************************
 * Tests the LTEm interrupt driven Input-Ouput processing subsystem in the 
 * driver which multiplexes the command and protocol streams.
 * Does not require a carrier network (SIM and activation).
 * 
 * The sketch is designed for debug output to observe results.
 *****************************************************************************/

#define _DEBUG 2                        // set to non-zero value for PRINTF debugging output, 
// debugging output options             // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if defined(_DEBUG)
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG >= 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #define PRINTF(c_,f_,__VA_ARGS__...) do { rtt_printf(c_, (f_), ## __VA_ARGS__); } while(0)
    #else
    #define SERIAL_DBG _DEBUG           // enable serial port output using devl host platform serial, _DEBUG 0=start immediately, 1=wait for port
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#endif

// specify the host pin configuration
#define HOST_FEATHER_UXPLOR_L
// #define HOST_FEATHER_UXPLOR             
// #define HOST_FEATHER_LTEM3F

//#include <ltemc.h>                                    // normally found in your appcode, not here for low-level access in unit test
#include <ltemc-internal.h>                             // this appl performs tests on internal, non-public API components 
#include <ltemc-iop.h>
#define SRCFILE "T03"

#define STRCMP(x, y)  (strcmp(x, y) == 0)

cBuffer_t rxBffr;                                           // cBuffer control structure
cBuffer_t* rxBffrPtr = &rxBffr;                             // convenience pointer var
char rawBuffer[220] = {0};                                  // raw buffer managed by rxBffr control

char hostBffr1[255];                                        // display buffer to receive received info from LTEmC
char hostBffr2[255];                                        // display buffer to receive received info from LTEmC

void setup() {
    #ifdef SERIAL_OPT
        Serial.begin(115200);
        #if (SERIAL_OPT > 0)
        while (!Serial) {}                                  // force wait for serial ready
        #else
        delay(5000);                                        // just give it some time
        #endif
    #endif

    PRINTF(dbgColor__red, "LTEmC Test3: iop\r");
    randomSeed(analogRead(7));
    lqDiag_setNotifyCallback(appEvntNotify);                // configure LTEMC ASSERTS to callback into application

    ltem_create(ltem_pinConfig, NULL, appEvntNotify);       // create LTEmC modem (no yield CB for testing)
    startLTEm();                                            // test defined initialize\start, can't use ltem_start() for this test scenario

    cbffr_init(rxBffrPtr, rawBuffer, sizeof(rawBuffer));
    g_lqLTEM.iop->rxBffr = rxBffrPtr;                       // override LTEm created buffer with test instance

    // pDelay(1000);
    // cbffr_reset(rxBffrPtr);
    char cmd[] = "ATE0\r";
    IOP_startTx(cmd, strlen(cmd));
    pDelay(100);
    cbffr_reset(rxBffrPtr);
}


int loopCnt = 0;

void loop() 
{
    /* Optional command strings for testing, must be from RAM (can't use const char* to create)
     */
    uint8_t expectedCnt = 79;
    // char cmd[] = "AT+GSN;+QCCID\r";                                  // short response (expect 57 char response)
    char cmd[] = "AT+GSN;+QCCID;+GMI;+GMM\r";                           // long response (expect 79 char response)
    // char cmd[] = "AT+QPOWD\r";                                       // Is something wrong? Is is rx or tx (tx works if BG powers down)
    PRINTF(0, "Invoking...\r", cmd);

    IOP_startTx(cmd, strlen(cmd));                                      // send, wait for complete
    PRINTF(0, "Sent %s\r", cmd);
    pDelay(500);                                                        // give BGx some time to respond, interrupt will fire and fill rx buffer

    uint16_t occupiedCnt = cbffr_getOccupied(rxBffrPtr);                // move to variable for break conditional
    ASSERT(occupiedCnt == expectedCnt);
    PRINTF(dbgColor__green, "Got %d chars (so far)\r", occupiedCnt);

    char* copyFrom;
    uint16_t firstCnt = (int)(expectedCnt / 2);
    uint16_t blockSz1;
    uint16_t blockSz2;

    if (loopCnt % 2 == 1)
    {
        PRINTF(dbgColor__green, "\r\rUsing POP\r");
        memset(hostBffr1, 0, sizeof(hostBffr1));                            // make it easy for str functions, PRINTF, and human eyes

        cbffr_pop(rxBffrPtr, hostBffr1, sizeof(hostBffr1));                 // move everything in rxBffr to hostBffr
        PRINTF(dbgColor__cyan, "Resp(%d chars): %s\r", strlen(hostBffr1), hostBffr1);
    }
    else
    {
        PRINTF(dbgColor__green, "\r\rUsing POP BLOCK\r");                   // I know this is testing two things at once... sorry
        memset(hostBffr2, 0, sizeof(hostBffr2));                            // make it easy for str functions, PRINTF, and human eyes

        blockSz1 = cbffr_popBlock(rxBffrPtr, &copyFrom, expectedCnt);       // move everything in rxBffr to hostBffr
        memcpy(hostBffr2, copyFrom, blockSz1);
        cbffr_popBlockFinalize(rxBffrPtr, true);

        blockSz2 = cbffr_popBlock(rxBffrPtr, &copyFrom, expectedCnt);
        ASSERT(blockSz1 + blockSz2 == expectedCnt);
        if (blockSz2 > 0)
        {
            memcpy(hostBffr2 + blockSz1, copyFrom, blockSz2);
            cbffr_popBlockFinalize(rxBffrPtr, true);
        }
        PRINTF(dbgColor__green, "Blocks: 1=%d, 2=%d\r", blockSz1, blockSz2);

        PRINTF(dbgColor__cyan, "Resp(%d chars): %s\r", strlen(hostBffr2), hostBffr2);
    }
    occupiedCnt = cbffr_getOccupied(rxBffrPtr);                             // move to variable for break conditional
    PRINTF(dbgColor__green, "rxBffr has %d chars now.\r", occupiedCnt);

    if (loopCnt > 1)
    {
        uint16_t cmpFault = strcmp(hostBffr1, hostBffr2);
        ASSERT(cmpFault == 0);
    }
    ASSERT(occupiedCnt == 0);

    loopCnt ++;
    indicateLoop(loopCnt, 1000);                            // BGx reference says 300mS cmd response time, we will wait 1000
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

    spi_start(g_lqLTEM.spi);

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
        PRINTF(dbgColor__error, "LTEmC-HardFault: %s\r", notifyMsg);
    }
    else 
    {
        PRINTF(dbgColor__white, "LTEmC Info: %s\r", notifyMsg);
    }
    return;
}


void indicateLoop(int loopCnt, int waitNext) 
{
    PRINTF(dbgColor__magenta, "\r\nLoop=%i \r\n", loopCnt);
    PRINTF(dbgColor__none, "FreeMem=%u\r\n", getFreeMemory());
    PRINTF(dbgColor__none, "NextTest (millis)=%i\r\r", waitNext);
    pDelay(waitNext);
}


void indicateFailure(char failureMsg[])
{
	PRINTF(dbgColor__error, "\r\n** %s \r", failureMsg);
    PRINTF(dbgColor__error, "** Test Assertion Failed. \r");

    #if 1
    PRINTF(dbgColor__error, "** Halting Execution \r");
    while (1)
    {
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        pDelay(1000);
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        pDelay(100);
    }
    #endif
}


/* Check free memory (stack-heap) 
 * - Remove if not needed for production
--------------------------------------------------------------------------------- */

#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__

int getFreeMemory() 
{
    char top;
    #ifdef __arm__
    return &top - reinterpret_cast<char*>(sbrk(0));
    #elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
    return &top - __brkval;
    #else  // __arm__
    return __brkval ? &top - __brkval : &top - __malloc_heap_start;
    #endif  // __arm__
}

