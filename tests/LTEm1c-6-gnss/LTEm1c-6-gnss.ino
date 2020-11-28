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

// define options for how to assemble this build
#define HOST_FEATHER_UXPLOR             // specify the pin configuration
// debugging options
#define _DEBUG                          // enable/expand 
#define JLINK_RTT                       // enable JLink debugger RTT terminal fuctionality
//#define SERIAL_OPT 1                    // enable serial port comm with devl host (1=force ready test)

#include <ltem1c.h>


void setup() {
    #ifdef SERIAL_OPT
        Serial.begin(115200);
        #if (SERIAL_OPT > 0)
        while (!Serial) {}      // force wait for serial ready
        #else
        delay(5000);            // just give it some time
        #endif
    #endif

    PRINTFC(dbgColor_white, "\rLTEm1c Test6: gnss\r");
    gpio_openPin(LED_BUILTIN, gpioMode_output);
    
    randomSeed(analogRead(0));

    ltem1_create(ltem1_pinConfig, ltem1Start_powerOn, ltem1Functionality_services);

    // turn on GNSS
    resultCode_t cmdResult = gnss_on();
    PRINTFC(dbgColor_info, "GNSS On result=%d (504 is already on)\r", cmdResult);
}


int loopCnt = 0;
gnssLocation_t location;

void loop() {

    location = gnss_getLocation();

    if (location.statusCode == 200)
    {
        char cLat[14];
        char cLon[14];
        
        PRINTFC(dbgColor_none, "Location Information\r");
        PRINTFC(dbgColor_cyan, "Lat=%4.4f, Lon=%4.4f \r", location.lat.val, location.lon.val);
    }
    else
        PRINTFC(dbgColor_warn, "Location is not available (GNSS not fixed)\r");

    // if (location.statusCode == RESULT_CODE_TIMEOUT)
    // {
    //     uint8_t txAvailable = sc16is741a_readReg(SC16IS741A_TXLVL_ADDR);
    //     uint8_t rxLevel = sc16is741a_readReg(SC16IS741A_RXLVL_ADDR);
    //     uint8_t iirVal = sc16is741a_readReg(SC16IS741A_IIR_ADDR);
    //     uint8_t fifoVal = sc16is741a_readReg(SC16IS741A_FIFO_ADDR);
 
    //     //iop_txSend("AT+QPOWD\r", 9);
    //     int halt = 1;
    //     while(halt){};
    // }

    loopCnt ++;
    indicateLoop(loopCnt, random(1000));
}




/* test helpers
========================================================================================================================= */

void indicateFailure(char failureMsg[])
{
	PRINTFC(dbgColor_error, "\r\n** %s \r\n", failureMsg);
    PRINTFC(dbgColor_error, "** Test Assertion Failed. \r\n");

    bool halt = true;
    while (halt)
    {
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        timing_delay(1000);
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        timing_delay(100);
    }
}


void indicateLoop(int loopCnt, int waitNext) 
{
    PRINTFC(dbgColor_info, "\r\nLoop=%i \r\n", loopCnt);

    for (int i = 0; i < 6; i++)
    {
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        timing_delay(50);
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        timing_delay(50);
    }

    PRINTFC(dbgColor_dMagenta, "FreeMem=%u\r\n", getFreeMemory());
    PRINTFC(dbgColor_dMagenta, "NextTest (millis)=%i\r\r", waitNext);
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

