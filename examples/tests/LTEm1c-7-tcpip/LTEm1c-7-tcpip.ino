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
#define RECV_BUF_SZ 200
#define SOCKET_ALREADYOPEN 563

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

// test setup
#define CYCLE_RANDOM_RANGE 5000
int loopCnt = 1;
unsigned long cycleBase = 5000;
unsigned long cycleRandom;
unsigned long lastCycle;

// ltem1 variables
socket_result_t result;
socket_t socketNm;
char sendBuf[120 + 1] = {0};
char recvBuf[RECV_BUF_SZ] = {0};


void setup() {
    #ifdef USE_SERIAL
        Serial.begin(115200);
        #if (USE_SERIAL)
        while (!Serial) {}
        #else
        delay(1000);
        #endif
    #endif

    PRINTF("\rLTEm1c test7-TCP/IP\r\n");
    gpio_openPin(LED_BUILTIN, gpioMode_output);
    
    randomSeed(analogRead(APIN_RANDOMSEED));
    lastCycle = timing_millis();
    cycleRandom = random(CYCLE_RANDOM_RANGE);

    ltem1_create(&ltem1_pinConfig, ltem1_functionality_services);

    socket_result_t result = ip_fetchNetworkContexts();
    if (result == PROTOCOL_RESULT_UNAVAILABLE)
    {
        ip_activateContext(DEFAULT_NETWORK_CONTEXT);
    }
    openSocket();

    // force send at start
    lastCycle = timing_millis() + 2*CYCLE_RANDOM_RANGE;
}


void loop() 
{
    /* NOTE: ltem1_dowork() pipeline requires up to 4 invokes for each data receive 
     */
    ltem1_dowork();

    if (timing_millis() - lastCycle > cycleBase + cycleRandom)
    {
        lastCycle = timing_millis();
        cycleRandom = random(CYCLE_RANDOM_RANGE);

        //loopCnt++;
        indicateLoop(loopCnt++, cycleBase + cycleRandom);

        //snprintf(sendBuf, 120, "--noecho %d-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890", loopCnt);      // 2 chunks
        snprintf(sendBuf, 120, "%d-ABCDEFGHIJKLMNOPQRSTUVWXYZ", loopCnt);                                                               // 1 chunk
        result = ip_send(socketNm, sendBuf, strlen(sendBuf));
        PRINTF("Loop %d, sendRslt=%d \r", loopCnt, result);

        if (result != 200)
                indicateFailure("TCPIP TEST-failure on ip_send().");


        if (loopCnt % 25 == 0)                 // close/reopen socket periodically for test
        {
            ip_close(socketNm);
            openSocket();
        }
    }
}


/*
========================================================================================================================= */


void openSocket()
{
    result = ip_open(protocol_udp, "97.83.32.119", 9001, 0, udpReceiver);
    if (result == SOCKET_ALREADYOPEN)
        socketNm = 0;
    else if (result >= LTEM1_SOCKET_COUNT)
    {
        indicateFailure("Failed to open socket.");
    }
}


void udpReceiver(socket_t socketNum)
{
    char recvBuf[RECV_BUF_SZ + 1] = {0};
    uint16_t recvdSz;

    recvdSz = ip_recv(socketNum, recvBuf, RECV_BUF_SZ);
    recvBuf[recvdSz + 1] = '\0';

    PRINTF_DBG2("** appRcvd>> %s <<\r", recvBuf);
}




/* test helpers
========================================================================================================================= */


void indicateLoop(int loopCnt, int waitNext) 
{
    PRINTF_DBG1("\r\nLoop=%i --------------------------------------------\r\n", loopCnt);
    PRINTF_DBG1("FreeMem=%u\r\n", getFreeMemory());
    PRINTF_DBG1("Next send to ECHO server in %i (millis)\r\r", waitNext);

    for (int i = 0; i < 6; i++)
    {
        gpio_writePin(LED_BUILTIN, gpio_pinValue_t::gpioValue_high);
        timing_delay(50);
        gpio_writePin(LED_BUILTIN, gpio_pinValue_t::gpioValue_low);
        timing_delay(50);
    }
    //timing_delay(waitNext);
}


void indicateFailure(char failureMsg[])
{
	PRINTF_ERR("\r\n** %s \r\n", failureMsg);
    PRINTF_ERR("** Test Assertion Failed. \r\n");

    bool halt = true;
    while (halt)
    {
        gpio_writePin(LED_BUILTIN, gpio_pinValue_t::gpioValue_high);
        timing_delay(1000);
        gpio_writePin(LED_BUILTIN, gpio_pinValue_t::gpioValue_low);
        timing_delay(100);
    }
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

