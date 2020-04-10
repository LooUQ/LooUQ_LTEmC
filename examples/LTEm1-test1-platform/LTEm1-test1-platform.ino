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

#include <ltem1.h>

const int APIN_RANDOMSEED = 7;

ltem1_pinConfig_t ltem1_pinConfig =
{
  spiCsPin : 5,
  spiIrqPin : 12,
  statusPin : 6,
  powerkeyPin : 11,
  resetPin : 19,
  ringUrcPin : 0,
  wakePin : 0
};

spi_config_t ltem1_spiConfig = 
{
  dataRate : 2000000U,
  dataMode : spi_dataMode_0,
  bitOrder : spi_bitOrder_msbFirst,
  csPin : ltem1_pinConfig.spiCsPin
};

ltem1_device ltem1;
spi_device spi; 


void setup() {
  Serial.begin(115200);
  #if 0
  while (!Serial) {}
  #else
  delay(5000);
  #endif

  DBGPRINTF("LTEm1 C Test1: platformBasic");
  gpio_openPin(LED_BUILTIN, gpioMode_output);
  DBGPRINTF("LED pin = %i \r\n", LED_BUILTIN);

  randomSeed(analogRead(APIN_RANDOMSEED));

  ltem1 = ltem1_init(&ltem1_pinConfig, false);
	if (ltem1 == NULL)
	{
    indicateFailure("LTEm1 create failed."); 
	}
  ltem1_initIO(ltem1);
  ltem1_powerOn(ltem1);

	spi = spi_init(ltem1_spiConfig);
	if (spi == NULL)
	{
    indicateFailure("SPI open failed."); 
	}
}

int loopCnt = 0;

union regBuffer { uint16_t val; struct { uint8_t lsb; uint8_t msb; }; };
regBuffer txBuffer;
regBuffer rxBuffer;


void loop() {
  uint8_t testPattern = random(256);
  txBuffer.msb = SC16IS741A_SPR_ADDR << 3;
  txBuffer.lsb = testPattern;
  rxBuffer.msb = (SC16IS741A_SPR_ADDR << 3) | 0x80;
  // rxBuffer.lsb doesn't matter prior to read

  spi_transferWord(ltem1->bridge->spi, txBuffer.val);
  rxBuffer.val = spi_transferWord(ltem1->bridge->spi, rxBuffer.val);

  DBGPRINTF("Writing scratchpad regiser with transferWord...");
  if (testPattern != rxBuffer.lsb)
    indicateFailure("Scratchpad write/read failed (transferWord)."); 


  // SPI operations are destructive to register addr; reset addr and incr pattern to differentiate
  txBuffer.msb = SC16IS741A_SPR_ADDR << 3;
  rxBuffer.msb = (SC16IS741A_SPR_ADDR << 3) | 0x80;  // write: reg addr + data
  txBuffer.lsb = ++testPattern;

  spi_transferBuffer(ltem1->bridge->spi, &txBuffer, 2);
  spi_transferBuffer(ltem1->bridge->spi, &rxBuffer, 2);

  if (testPattern != rxBuffer.lsb)
    indicateFailure("Scratchpad write/read failed (transferBuffer)."); 


  loopCnt ++;
  indicateLoop(loopCnt, random(1000));
}



/* test helpers
========================================================================================================================= */

void indicateFailure(char failureMsg[])
{
	DBGPRINTF("\r\n** %s \r\n", failureMsg);
  DBGPRINTF("** Test Assertion Failed. \r\n");

  #if 1
  DBGPRINTF("** Halting Execution \r\n");
  while (1)
  {
      gpio_writePin(LED_BUILTIN, gpio_pinValue_t::gpioValue_high);
      timing_delay(1000);
      gpio_writePin(LED_BUILTIN, gpio_pinValue_t::gpioValue_low);
      timing_delay(100);
  }
  #endif
}


void indicateLoop(int loopCnt, int waitNext) 
{
  DBGPRINTF("Loop: %i \r\n", loopCnt);
  DBGPRINTF("      Tx: %i \r\n", txBuffer.lsb);
  DBGPRINTF("      Rx: %i \r\n", rxBuffer.lsb);

  for (int i = 0; i < 6; i++)
  {
    gpio_writePin(LED_BUILTIN, gpio_pinValue_t::gpioValue_high);
    timing_delay(50);
    gpio_writePin(LED_BUILTIN, gpio_pinValue_t::gpioValue_low);
    timing_delay(50);
  }

  DBGPRINTF("Free memory: %u \r\n", getFreeMemory());
  DBGPRINTF("Next test in (millis): %i\r\n\r\n", waitNext);
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

