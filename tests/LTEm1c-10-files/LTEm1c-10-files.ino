/******************************************************************************
 *  \file LTEm1c-9-files.ino
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
 *  www.loouq.com
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
 * Test\demonstrate using the LTEm1\BG96 file system for persistent 
 * file storage.
 *****************************************************************************/

// define options for how to assemble this build
#define HOST_FEATHER_UXPLOR             // specify the pin configuration
// debugging options
#define _DEBUG                          // enable/expand 
#define JLINK_RTT                       // enable JLink debugger RTT terminal fuctionality
// #define SERIAL_OPT 1                    // enable serial port comm with devl host (1=force ready test)
#include <ltem1c.h>
#include <file.h>

#define ASSERT(expected_true, failMsg)  if(!(expected_true))  indicateFailure(failMsg)
#define ASSERT_NOTEMPTY(string, failMsg)  if(string[0] == '\0') indicateFailure(failMsg)


// test setup
#define CYCLE_INTERVAL 5000
uint16_t loopCnt = 1;
uint32_t lastCycle;

uint16_t fileHandle;


void setup() {
    #ifdef SERIAL_OPT
        Serial.begin(115200);
        #if (SERIAL_OPT > 0)
        while (!Serial) {}      // force wait for serial ready
        #else
        delay(5000);            // just give it some time
        #endif
    #endif

    PRINTFC(dbgColor_white, "\rLTEm1c test8-MQTT\r\n");
    gpio_openPin(LED_BUILTIN, gpioMode_output);

    ltem1_create(ltem1_pinConfig, ltem1Start_powerOn, ltem1Functionality_services);
}


void loop() 
{
    if (timing_millis() - lastCycle >= CYCLE_INTERVAL)
    {
        lastCycle = timing_millis();
        char testFile[12];

        // open (create) file
        snprintf(testFile, 12, "test%d.txt", loopCnt);
        file_open(testFile, fileOpenMode_normalRdWr, fileReadReceiver);
        // write to it

        if (loopCnt > 1)
        {
            // open previous file
            // get file pointer
        }


        loopCnt++;
        PRINTFC(dbgColor_magenta, "\rFreeMem=%u  <<Loop=%d>>\r", getFreeMemory(), loopCnt);
    }

    /* NOTE: ltem1_doWork() pipeline requires up to 3 invokes for each data receive. DoWork has no side effects 
     * other than taking time and should be invoked liberally. */
    ltem1_doWork();
}


void fileReadReceiver(uint16_t fileHandle, void *fileData, uint16_t dataSz)
{
}


/* test helpers
========================================================================================================================= */

void indicateFailure(char failureMsg[])
{
	PRINTFC(dbgColor_error, "\r\n** %s \r\n", failureMsg);
    PRINTFC(dbgColor_error, "** Test Assertion Failed. \r\n");
    gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);

    int halt = 1;
    while (halt) {}
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

