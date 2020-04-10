
#include <ltem1.h>
//#include <LooUQ_LTEm1c\src\ltem1.h>

// #include "src/platform/platformGpio.h"
// #include "src/platform/platformTiming.h"
// #include "src/platform/platformStdio.h"
// #include "src/platform/platformSpi.h"
// #include "src/components/nxp-sc16is741a.h"

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

  DBGPRINTF("LTEm1 c platformBasic");
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
  txBuffer.msb = SC16IS741A_SPR_ADDR << 3;
  txBuffer.lsb = random(256);
  rxBuffer.msb = (SC16IS741A_SPR_ADDR << 3) | 0x80;
  rxBuffer.lsb = 0x00;

  // write: reg addr + data
  spi_transferWord(spi, txBuffer.val);
  // read: reg addr + don't care
  rxBuffer.val = spi_transferWord(spi, rxBuffer.val);

  if (txBuffer.lsb != rxBuffer.lsb)
    indicateFailure("Scratchpad write/read failed."); 

  loopCnt ++;
  indicateLoop(loopCnt, random(1000));
}


/*
========================================================================================================================= */


void atTest() {
  /* test pattern
      AT+QTEMP
  //response ():
      +QTEMP: 30,26,26

      OK
  */
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

