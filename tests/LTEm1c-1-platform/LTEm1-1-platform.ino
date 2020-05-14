/******************************************************************************
 *  \file platformSpi.h
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
 * and PRINTF.
 * 
 * This sketch is designed to be console attached for observation of serial 
 * output.
 *****************************************************************************/

#include <ltem1c.h>

//#define USE_SERIAL 0

extern "C" {
#include <SEGGER_RTT.h>
}


const int APIN_RANDOMSEED = 7;

ltem1PinConfig_t ltem1_pinConfig =
{
  spiCsPin : 13,
  irqPin : 12,
  statusPin : 6,
  powerkeyPin : 11,
  resetPin : 19,
  ringUrcPin : 5,
  wakePin : 10
};


spiDevice_t *spi; 


void setup() {
    #ifdef USE_SERIAL
        Serial.begin(115200);
        #if (USE_SERIAL)
        while (!Serial) {}
        #else
        delay(5000);
        #endif
    #endif

    PRINTF("LTEm1 C Test1: platformBasic \r\n");
    gpio_openPin(LED_BUILTIN, gpioMode_output);
    PRINTF("LED pin = %i \r\n", LED_BUILTIN);

    randomSeed(analogRead(APIN_RANDOMSEED));

	gpio_writePin(ltem1_pinConfig.powerkeyPin, gpioValue_low);
	gpio_writePin(ltem1_pinConfig.resetPin, gpioValue_low);
	gpio_writePin(ltem1_pinConfig.spiCsPin, gpioValue_high);
	gpio_openPin(ltem1_pinConfig.powerkeyPin, gpioMode_output);		    // powerKey: normal low
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

    spi_transferWord(spi, txBuffer.val);
    rxBuffer.val = spi_transferWord(spi, rxBuffer.val);

    PRINTF("Writing scratchpad regiser with transferWord...");
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
		PRINTF("Powering LTEm1 On...");
		gpio_writePin(ltem1_pinConfig.powerkeyPin, gpioValue_high);
		timing_delay(QBG_POWERON_DELAY);
		gpio_writePin(ltem1_pinConfig.powerkeyPin, gpioValue_low);
		while (!gpio_readPin(ltem1_pinConfig.statusPin))
		{
			timing_delay(500);
		}
		PRINTF("DONE.\r\n");
	}
	else
	{
		PRINTF("LTEm1 is already powered on.\r\n");
	}
}



/* test helpers
========================================================================================================================= */

void indicateFailure(char failureMsg[])
{
	PRINTF_ERR("\r\n** %s \r\n", failureMsg);
    PRINTF_ERR("** Test Assertion Failed. \r\n");

    #if 1
    PRINTF_ERR("** Halting Execution \r\n");
    while (1)
    {
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        timing_delay(1000);
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        timing_delay(100);
    }
    #endif
}


void indicateLoop(int loopCnt, int waitNext) 
{
    PRINTF("Loop: %i \r\n", loopCnt);
    PRINTF("      Tx: %i \r\n", testPattern);
    PRINTF("      Rx: %i \r\n", rxBuffer.lsb);

    for (int i = 0; i < 6; i++)
    {
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        timing_delay(50);
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        timing_delay(50);
    }

    PRINTF("Free memory: %u \r\n", getFreeMemory());
    PRINTF("Next test in (millis): %i\r\n\r\n", waitNext);
    timing_delay(waitNext);
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
