/******************************************************************************
 *  \file LTEmC-3-iop.ino
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
    #if _DEBUG == 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #define PRINTF(c_,f_,__VA_ARGS__...) do { rtt_printf(c_, (f_), ## __VA_ARGS__); } while(0)
    #else
    #define SERIAL_DBG _DEBUG           // enable serial port output using devl host platform serial, _DEBUG 0=start immediately, 1=wait for port
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#endif

                                        // define options for how to assemble this build
// #define HOST_FEATHER_UXPLOR             // specify the pin configuration
#define HOST_FEATHER_LTEM3F

//#include <ltemc.h>
#include <ltemc-internal.h>             // this appl performs tests on internal, non-public API components 

#include <ltemc-iop.h>

char *cmdBuf;


void setup() {
    #ifdef SERIAL_OPT
        Serial.begin(115200);
        #if (SERIAL_OPT > 0)
        while (!Serial) {}      // force wait for serial ready
        #else
        delay(5000);            // just give it some time
        #endif
    #endif

    PRINTF(dbgColor__red, "LTEmC Test3: iop\r");
    randomSeed(analogRead(7));
    lqDiag_registerEventCallback(appNotifyCB);                      // configure ASSERTS to callback into application

    ltem_create(ltem_pinConfig, NULL, appNotifyCB);                 // create LTEmC modem (no yield CB for testing)
    startLTEm();                                                    // local initialize\start can't use ltem_start() yet

    cmdBuf = ((iop_t*)g_ltem.iop)->rxCBuffer->_buffer;              // readability var
}


int loopCnt = 0;

void loop() 
{
    // const char *cmd = "AT+GSN\r\0";                           // short response (contained in 1 rxCtrlBlock)
    const char *cmd = "AT+GSN;+QCCID;+GSN;+QCCID\r\0";           // long response (requires multiple rxCtrlBlocks)
    // const char *cmd = "AT+QPOWD\r\0";                         // something is wrong! Is is rx or tx (tx works if BG powers down)

    PRINTF(0, "Invoking cmd: %s \r\n", cmd);

    IOP_resetCoreRxBuffer();                                // send command
    IOP_sendTx(cmd, strlen(cmd), true);
    PRINTF(0, "Command sent\r\n");

    pDelay(1000);                                           // give BGx time to respond

    PRINTF(dbgColor__green, "Got %d chars (so far)\r", strlen(cmdBuf));
    PRINTF(dbgColor__cyan, "Resp: %s\r", cmdBuf);  

    loopCnt ++;
    indicateLoop(loopCnt, 1000);      // BG reference says 300mS cmd response time, we will wait 1000
    PRINTF(dbgColor__magenta, "FreeMem=%u\r", getFreeMemory());
}



/* test helpers
========================================================================================================================= */

void startLTEm()
{
    // initialize the HOST side of the LTEm interface
	// ensure pin is in default "logical" state prior to opening
	platform_writePin(g_ltem.pinConfig.powerkeyPin, gpioValue_low);
	platform_writePin(g_ltem.pinConfig.resetPin, gpioValue_low);
	platform_writePin(g_ltem.pinConfig.spiCsPin, gpioValue_high);

	platform_openPin(g_ltem.pinConfig.powerkeyPin, gpioMode_output);		// powerKey: normal low
	platform_openPin(g_ltem.pinConfig.resetPin, gpioMode_output);			// resetPin: normal low
	platform_openPin(g_ltem.pinConfig.spiCsPin, gpioMode_output);			// spiCsPin: invert, normal gpioValue_high

	platform_openPin(g_ltem.pinConfig.statusPin, gpioMode_input);
	platform_openPin(g_ltem.pinConfig.irqPin, gpioMode_inputPullUp);

    spi_start(g_ltem.spi);

    if (qbg_isPowerOn())                                        // power on BGx, returning prior power-state
    {
		PRINTF(dbgColor__info, "LTEm1 found powered on.\r\n");
        g_ltem.qbgDeviceState = qbgDeviceState_appReady;        // if already "ON", assume running and check for IRQ latched
    }
    else
        qbg_powerOn();

    SC16IS7xx_start();                                          // start (resets previously powered on) NXP SPI-UART bridge
    SC16IS7xx_enableIrqMode();
    IOP_attachIrq();
    IOP_awaitAppReady();                                        // wait for BGx to signal out firmware ready
}


void appNotifyCB(uint8_t notifType, const char *notifMsg)
{
    if (notifType > 200)
    {
        PRINTF(dbgColor__error, "LQCloud-HardFault: %s\r", notifMsg);
        while (1) {}
    }
    PRINTF(dbgColor__info, "LQCloud Info: %s\r", notifMsg);
    return;
}


void indicateLoop(int loopCnt, int waitNext) 
{
    PRINTF(dbgColor__info, "\r\nLoop=%i \r\n", loopCnt);
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

