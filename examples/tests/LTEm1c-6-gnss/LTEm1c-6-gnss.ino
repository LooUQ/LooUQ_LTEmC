/******************************************************************************
 *  \file LTEm1c-6-gnss.ino
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
 * Test the GNSS/GPS receiver basic functionality (on/off/get location).
 *****************************************************************************/

#include <ltem1c.h>
#include <stdio.h>

//#define USE_SERIAL 0

extern "C" {
#include <SEGGER_RTT.h>
}


const int APIN_RANDOMSEED = 0;

ltem1_pinConfig_t ltem1_pinConfig =
{
  spiCsPin : 13,
  irqPin : 12,
  statusPin : 6,
  powerkeyPin : 11,
  resetPin : 19,
  ringUrcPin : 5,
  wakePin : 10
};

spi_config_t ltem1_spiConfig = 
{
  dataRate : 2000000U,
  dataMode : spi_dataMode_0,
  bitOrder : spi_bitOrder_msbFirst,
  csPin : ltem1_pinConfig.spiCsPin
};


void setup() {
    #ifdef USE_SERIAL
        Serial.begin(115200);
        #if (USE_SERIAL)
        while (!Serial) {}
        #else
        delay(1000);
        #endif
    #endif

    PRINTF("\rLTEm1c test5-modemInfo\r\n");
    gpio_openPin(LED_BUILTIN, gpioMode_output);
    
    randomSeed(analogRead(APIN_RANDOMSEED));

    ltem1_create(&ltem1_pinConfig, ltem1_functionality_services);

    // turn on GNSS
    atcmd_result_t cmdResult = gnss_on();
    PRINTF("GNSS On result=%d (504 is already on)\r", cmdResult);
}


int loopCnt = 0;
gnss_location_t location;

void loop() {
    location = gnss_getLocation();

    char cLat[14];
    char cLon[14];
    
    floatToString(location.lat.val, cLat, 12, 6);
    floatToString(location.lon.val, cLon, 12, 6);

    PRINTF_INFO("Location Information\r");
    PRINTF("Lat=%s, Lon=%s \r", cLat, cLon);

    loopCnt ++;
    indicateLoop(loopCnt, random(1000));
}


/*
========================================================================================================================= */

// void floatToString(float fVal, char *buf, uint8_t bufSz, uint8_t precision)
// {
//     uint32_t max = pow(10, (bufSz - precision - 3));    //remember '-', '.' and '\0'
//     if (abs(fVal) >= max)
//     {
//         *buf = '\0';
//         return;
//     }

//     int iVal = (int)fVal;
//     itoa(iVal, buf, 10);

//     fVal = abs(fVal);
//     iVal = abs(iVal);
//     uint8_t pos = strlen(buf);
//     buf[pos++] = '.';

//     for (size_t i = 0; i < precision; i++)
//     {
//         fVal -= (float)iVal;      // hack off the whole part of the number
//  		fVal *= 10;           // move next digit over
//  		iVal = (int)fVal;
//         char c = (char)iVal + 0x30;
//  		buf[pos++] = c;
//     }
//     buf[pos++] = '\0';
// }









void floatToString(float fVal, uint8_t digits, char *buf, uint8_t bufSz)
//void floattostring(char *buf, float fVal, char digits)
{
    char pos;  // position in string
 	char len;  // length of decimal part of result
 	char* curr;  // temp holder for next digit
 	int value;  // decimal digit(s) to convert
 	pos = 0;  // initialize pos, just to be sure
 
 	value = (int)fVal;  // truncate the floating point number
 	itoa(value, buf, 10);  // this is kinda dangerous depending on the length of str
 	// now str array has the digits before the decimal
 
 	if (fVal < 0 )  // handle negative numbers
 	{
 		fVal *= -1;
 		value *= -1;
 	}
 
    len = strlen(buf);  // find out how big the integer part was
 	pos = len;  // position the pointer to the end of the integer part
 	buf[pos++] = '.';  // add decimal point to string
 	
 	while(pos < (digits + len + 1) )  // process remaining digits
 	{
 		fVal = fVal - (float)value;  // hack off the whole part of the number
 		fVal *= 10;  // move next digit over
 		value = (int)fVal;  // get next digit
 		itoa(value, curr, 10); // convert digit to string
 		buf[pos++] = *curr; // add digit to result string and increment pointer
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
        gpio_writePin(LED_BUILTIN, gpio_pinValue_t::gpioValue_high);
        timing_delay(1000);
        gpio_writePin(LED_BUILTIN, gpio_pinValue_t::gpioValue_low);
        timing_delay(100);
    }
    #endif
}


void indicateLoop(int loopCnt, int waitNext) 
{
    PRINTF_DBG("\r\nLoop=%i \r\n", loopCnt);

    for (int i = 0; i < 6; i++)
    {
        gpio_writePin(LED_BUILTIN, gpio_pinValue_t::gpioValue_high);
        timing_delay(50);
        gpio_writePin(LED_BUILTIN, gpio_pinValue_t::gpioValue_low);
        timing_delay(50);
    }

    PRINTF("FreeMem=%u\r\n", getFreeMemory());
    PRINTF("NextTest (millis)=%i\r\r", waitNext);
    timing_delay(waitNext);
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

