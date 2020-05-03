/******************************************************************************
 *  \file LTEm1c-7-ipProto.ino
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
 * Test IP protocol client send/receive.
 *****************************************************************************/

#include <ltem1c.h>
#include <stdio.h>

//#define USE_SERIAL 0

extern "C" {
#include <SEGGER_RTT.h>
}

#define DEFAULT_NETWORK_CONTEXT 1

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

protocol_result_t socketNm = 500;

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

    protocol_result_t result = ip_fetchNetworkContexts();
    if (result == PROTOCOL_RESULT_UNAVAILABLE)
    {
        ip_activateContext(DEFAULT_NETWORK_CONTEXT);
    }
}


int loopCnt = 0;
protocol_result_t result;
char sendBuf[120 + 1] = {0};

void loop() 
{
    ltem1_dowork();

    // open UDP client (outgoing)
    socketNm = ip_open(protocol_udp, "97.83.32.119", 9011, 0, udpReceiver);
    if (socketNm >= LTEM1_PROTOCOL_COUNT && socketNm != 563)
    {
        indicateFailure("Failed to open socket.");
    }
    socketNm = 0;

    for (size_t i = 0; i < 5; i++)
    {
        snprintf(sendBuf, 120, "--noecho %d-%d 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890", loopCnt, i);      // 2 chunks
        // snprintf(sendBuf, 120, "--noecho %d-%d 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ", loopCnt, i);                                       // 1 chunk
        result = ip_send(socketNm, sendBuf, strlen(sendBuf));
        PRINTF("Send result=%d \r", result);

        timing_delay(500);
    }
    
    ip_close(socketNm);

    loopCnt ++;
    indicateLoop(loopCnt, random(1000));
}


/*
========================================================================================================================= */

void udpReceiver(const char *recvBuf, uint16_t recvSz)
{
    PRINTF_DBG2(">> %s", recvSz);
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

