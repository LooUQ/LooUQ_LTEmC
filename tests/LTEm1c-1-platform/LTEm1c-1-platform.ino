/******************************************************************************
 *  \file LTEm1c-1-platform.ino
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
 * The test1-platformAbstraction.ino tests the LTEm1 for proper I/O operations 
 * via the driver's platform abstraction functions. GPIO, timing, SPI 
 * and PRINTFC.
 * 
 * This sketch is designed to be console attached for observation of serial 
 * output.
 *****************************************************************************/

// define options for how to assemble this build
#define HOST_FEATHER_UXPLOR             // specify the pin configuration
// debugging options
#define _DEBUG                          // enable/expand 
// #define JLINK_RTT                       // enable JLink debugger RTT terminal fuctionality
#define SERIAL_OPT 1                    // enable serial port comm with devl host (1=force ready test)

#include <ltem1c.h>

// this test has no reference to global g_ltem1 variable
// so we need a surrogate spi pointer here to test locally 
spiDevice_t *spi; 


void setup() {
    #ifdef SERIAL_OPT
        Serial.begin(115200);
        #if (SERIAL_OPT > 0)
        while (!Serial) {}      // force wait for serial ready
        #else
        delay(5000);            // just give it some time
        #endif
    #endif

    PRINTFC(0, "LTEm1c Test1: platformIO \r\n");
    gpio_openPin(LED_BUILTIN, gpioMode_output);
    PRINTFC(0, "LED pin = %i \r\n", LED_BUILTIN);

    randomSeed(analogRead(7));

	gpio_writePin(ltem1_pinConfig.powerkeyPin, gpioValue_low);
	gpio_writePin(ltem1_pinConfig.resetPin, gpioValue_low);
	gpio_writePin(ltem1_pinConfig.spiCsPin, gpioValue_high);
	gpio_openPin(ltem1_pinConfig.powerkeyPin, gpioMode_output);
	gpio_openPin(ltem1_pinConfig.statusPin, gpioMode_input);
	
    powerModemOn();

	spi = spi_create(ltem1_pinConfig.spiCsPin);
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
    txBuffer.msb = SC16IS741A_SPR_ADDR << 3;
    rxBuffer.msb = (SC16IS741A_SPR_ADDR << 3) | 0x80;
    txBuffer.lsb = testPattern;
    // rxBuffer.lsb doesn't matter prior to read

    PRINTFC(0, "Writing scratchpad regiser with transferWord...");
    spi_transferWord(spi, txBuffer.val);
    rxBuffer.val = spi_transferWord(spi, rxBuffer.val);

    if (testPattern != rxBuffer.lsb)
        indicateFailure("Scratchpad write/read failed (transferWord)."); 


    // SPI operations are destructive to register addr; reset addr and incr pattern to differentiate
    txBuffer.msb = SC16IS741A_SPR_ADDR << 3;
    rxBuffer.msb = (SC16IS741A_SPR_ADDR << 3) | 0x80;  // write: reg addr + data
    txBuffer.lsb = ++testPattern;

    spi_transferBuffer(spi, txBuffer.msb, &txBuffer.lsb, 1);
    spi_transferBuffer(spi, rxBuffer.msb, &rxBuffer.lsb, 1);

    if (testPattern != rxBuffer.lsb)
        indicateFailure("Scratchpad write/read failed (transferBuffer)."); 

    loopCnt ++;
    indicateLoop(loopCnt, random(1000));
}

/*
========================================================================================================================= */


void powerModemOn()
{
	if (!gpio_readPin(ltem1_pinConfig.statusPin))
	{
		PRINTFC(0, "Powering LTEm1 On...");
		gpio_writePin(ltem1_pinConfig.powerkeyPin, gpioValue_high);
		lDelay(QBG_POWERON_DELAY);
		gpio_writePin(ltem1_pinConfig.powerkeyPin, gpioValue_low);
		while (!gpio_readPin(ltem1_pinConfig.statusPin))
		{
			lDelay(500);
		}
		PRINTFC(0, "DONE.\r\n");
	}
	else
	{
		PRINTFC(0, "LTEm1 is already powered on.\r\n");
	}
}



/* test helpers
========================================================================================================================= */

void indicateLoop(int loopCnt, int waitNext) 
{
    PRINTFC(dbgColor_info, "Loop: %i \r\n", loopCnt);
    PRINTFC(dbgColor_info, "      Tx: %i \r\n", testPattern);
    PRINTFC(dbgColor_info, "      Rx: %i \r\n", rxBuffer.lsb);

    for (int i = 0; i < 6; i++)
    {
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        lDelay(50);
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        lDelay(50);
    }

    PRINTFC(dbgColor_magenta, "Free memory: %u \r\n", getFreeMemory());
    PRINTFC(0, "Next test in (millis): %i\r\n\r\n", waitNext);
    lDelay(waitNext);
}


void indicateFailure(char failureMsg[])
{
	PRINTFC(dbgColor_error, "\r\n** %s \r\n", failureMsg);
    PRINTFC(dbgColor_error, "** Test Assertion Failed. \r\n");

    int halt = 1;
    PRINTFC(dbgColor_error, "** Halting Execution \r\n");
    while (halt)
    {
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        lDelay(1000);
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        lDelay(100);
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
