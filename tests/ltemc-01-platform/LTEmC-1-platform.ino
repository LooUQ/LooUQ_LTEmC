/******************************************************************************
 *  \file LTEmC-1-platform.ino
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
 * Test 1: platform abstraction tests the LTEm device for proper I/O 
 * operations via the driver's platform abstraction functions. GPIO, timing, 
 * SPI and PRINTFC.
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


// define host pinout
#define HOST_FEATHER_UXPLOR_L                                   // original UXPlor Feather 
//#define HOST_FEATHER_UXPLOR                                     // rev 2 UXPlor Feather
//#define HOST_FEATHER_LTEM3F

#include <ltemc.h>
#include <ltemc-nxp-sc16is.h>                                           // need internal references, low-level test here

// this test has no reference to global g_lqLTEM variable
// so we need a surrogate spi pointer here to test locally 
spi_t *spi; 


void setup() {
    #ifdef SERIAL_DBG
        Serial.begin(115200);
        #if (SERIAL_DBG > 0)
        while (!Serial) {}      // force wait for serial ready
        #else
        delay(5000);            // just give it some time
        #endif
    #endif

    PRINTF(dbgColor__red, "LTEmC - Test #1: Platform I/O and SPI\r\n");
    PRINTF(dbgColor__none, "LED pin = %i \r\n", LED_BUILTIN);           // could have used 0 as color code, rather than enum dbgColor__none

    randomSeed(analogRead(7));

	platform_writePin(ltem_pinConfig.powerkeyPin, gpioValue_low);
	platform_writePin(ltem_pinConfig.resetPin, gpioValue_low);
	platform_writePin(ltem_pinConfig.spiCsPin, gpioValue_high);
    
	platform_openPin(ltem_pinConfig.powerkeyPin, gpioMode_output);
	platform_openPin(ltem_pinConfig.statusPin, gpioMode_input);

    PRINTF(dbgColor__none, "Modem status = %i \r", platform_readPin(ltem_pinConfig.statusPin));
    powerModemOn();
    PRINTF(dbgColor__none, "   ON Status = %i \r", platform_readPin(ltem_pinConfig.statusPin));
    powerModemOff();
    PRINTF(dbgColor__none, "  OFF Status = %i \r", platform_readPin(ltem_pinConfig.statusPin));

    PRINTF(dbgColor__info, "Turn modem on for SPI tests\r");
    powerModemOn();
	spi = (spi_t*)spi_create(ltem_pinConfig.spiCsPin);
	if (spi == NULL)
	{
        indicateFailure("SPI create failed."); 
	}
    spi_start(spi);
}


int loopCnt = 0;

union regBuffer { uint16_t val; struct { uint8_t msb; uint8_t lsb; }; };
regBuffer txBuffer;
regBuffer rxBuffer;
uint8_t testPattern;


void loop() {
    testPattern = random(256);
    txBuffer.msb = SC16IS7xx_SPR_regAddr << 3;
    rxBuffer.msb = (SC16IS7xx_SPR_regAddr << 3) | 0x80;
    txBuffer.lsb = testPattern;
    // rxBuffer.lsb doesn't matter prior to read

    PRINTF(0, "Writing scratchpad regiser with transferWord...");
    spi_transferWord(spi, txBuffer.val);
    rxBuffer.val = spi_transferWord(spi, rxBuffer.val);

    if (testPattern != rxBuffer.lsb)
    {
        PRINTF(dbgColor__warn, "Scratchpad write/read failed (transferWord).");
        // indicateFailure("Scratchpad write/read failed (transferWord)."); 
    }

    // // SPI operations are destructive to register addr; reset addr and incr pattern to differentiate
    // txBuffer.msb = SC16IS7xx_SPR_regAddr << 3;
    // rxBuffer.msb = (SC16IS7xx_SPR_regAddr << 3) | 0x80;  // write: reg addr + data
    // txBuffer.lsb = ++testPattern;

    // spi_transferBuffer(spi, txBuffer.msb, &txBuffer.lsb, 1);
    // spi_transferBuffer(spi, rxBuffer.msb, &rxBuffer.lsb, 1);

    // if (testPattern != rxBuffer.lsb)
    //     indicateFailure("Scratchpad write/read failed (transferBuffer)."); 

    loopCnt ++;
    indicateLoop(loopCnt, random(1000));
}

/*
========================================================================================================================= */


void powerModemOn()
{
	if (!platform_readPin(ltem_pinConfig.statusPin))
	{
		PRINTF(0, "Powering LTEm On...");
		platform_writePin(ltem_pinConfig.powerkeyPin, gpioValue_high);
		pDelay(1000);
		platform_writePin(ltem_pinConfig.powerkeyPin, gpioValue_low);
		while (!platform_readPin(ltem_pinConfig.statusPin))
		{
			pDelay(500);
		}
		PRINTF(0, "DONE.\r\n");
	}
	else
	{
		PRINTF(0, "LTEm is already powered on.\r\n");
	}
}


void powerModemOff()
{
	if (platform_readPin(ltem_pinConfig.statusPin))
	{
		PRINTF(0, "Powering LTEm Off...");
		platform_writePin(ltem_pinConfig.powerkeyPin, gpioValue_high);
		pDelay(1000);
		platform_writePin(ltem_pinConfig.powerkeyPin, gpioValue_low);
		while (platform_readPin(ltem_pinConfig.statusPin))
		{
			pDelay(500);
		}
		PRINTF(0, "DONE.\r\n");
	}
	else
	{
		PRINTF(0, "LTEm is already powered off.\r\n");
	}
}


/* test helpers
========================================================================================================================= */

void indicateLoop(int loopCnt, int waitNext) 
{
    PRINTF(dbgColor__info, "Loop: %i \r\n", loopCnt);
    PRINTF(dbgColor__info, "      Tx: %i \r\n", testPattern);
    PRINTF(dbgColor__info, "      Rx: %i \r\n", rxBuffer.lsb);

    for (int i = 0; i < 6; i++)
    {
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        pDelay(50);
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        pDelay(50);
    }

    PRINTF(dbgColor__magenta, "Free memory: %u \r\n", getFreeMemory());
    PRINTF(0, "Next test in (millis): %i\r\n\r\n", waitNext);
    pDelay(waitNext);
}


void indicateFailure(char failureMsg[])
{
	PRINTF(dbgColor__error, "\r\n** %s \r\n", failureMsg);
    PRINTF(dbgColor__error, "** Test Assertion Failed. \r\n");

    int halt = 1;
    PRINTF(dbgColor__error, "** Halting Execution \r\n");
    while (halt)
    {
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        pDelay(1000);
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        pDelay(100);
    }
}


/* Check free memory (stack-heap) 
 * - Remove if not needed for production
*/
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
