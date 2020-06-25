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

#define _DEBUG
#include "platform/platform_stdio.h"

#define DEFAULT_NETWORK_CONTEXT 1
#define BUFFER_SZ 201
#define TCPIP_TEST_SERVER "97.83.32.119"


const int APIN_RANDOMSEED = 0;

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

spiConfig_t ltem1_spiConfig = 
{
  dataRate : 2000000U,
  dataMode : spiDataMode_0,
  bitOrder : spiBitOrder_msbFirst,
  csPin : ltem1_pinConfig.spiCsPin
};

// test setup
#define CYCLE_INTERVAL 5000
uint16_t loopCnt = 0;
uint32_t lastCycle;

// ltem1 variables
socketResult_t result;
socketId_t socketId;
char sendBuf[BUFFER_SZ] = {0};
char recvBuf[BUFFER_SZ] = {0};


void setup() {
    #ifdef USE_SERIAL
        Serial.begin(115200);
        #if (USE_SERIAL)
        while (!Serial) {}
        #else
        delay(1000);
        #endif
    #endif

    PRINTF(dbgColor_white, "\rLTEm1c test7-TCP/IP\r\n");
    gpio_openPin(LED_BUILTIN, gpioMode_output);

    ltem1_create(&ltem1_pinConfig, ltem1Functionality_services);

    PRINTF(dbgColor_none, "Waiting on network...");
    do
    {
        if (ntwk_getOperator().operName[0] == NULL)
            timing_delay(1000);
    } while (g_ltem1->network->networkOperator->operName[0] == NULL);
    PRINTF(dbgColor_info, "Operator is %s\r", g_ltem1->network->networkOperator->operName);

    socketResult_t result = ntwk_fetchDataContexts();
    if (result == ACTION_RESULT_NOTFOUND)
    {
        ntwk_activateContext(DEFAULT_NETWORK_CONTEXT);
    }

    test_openSocket();

    // force send at start
    lastCycle = timing_millis() + 2*CYCLE_INTERVAL;
}


void loop() 
{
    if (timing_millis() - lastCycle >= CYCLE_INTERVAL)
    {
        lastCycle = timing_millis();
        loopCnt++;
        PRINTF(dbgColor_dCyan, "\rFreeMem=%u  <<Loop=%d\r", getFreeMemory(), loopCnt);

        snprintf(sendBuf, 120, "%d-%lu ABCDEFGHIJKLMNOPQRSTUVWXYZ", loopCnt, timing_millis());                                      // 1 chunk
        //snprintf(sendBuf, 120, "--noecho %d-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890", loopCnt);    // 2 chunks
        PRINTF(dbgColor_dGreen, "sendBuf=%s\r", sendBuf);

        result = ip_send(socketId, sendBuf, strlen(sendBuf), NULL, NULL);
        PRINTF((result == ACTION_RESULT_SUCCESS) ? dbgColor_cyan : dbgColor_warn, "Send Loop %d, sendRslt=%d \r", loopCnt, result);
    }

    /*
     * NOTE: ltem1_doWork() pipeline requires multiple invokes to complete data receives. 
     * doWork() has no side effects other than taking time and SHOULD BE INVOKED LIBERALLY.
     */
    ltem1_doWork();
}


/*
========================================================================================================================= */


void test_openSocket()
{
    result = ip_open(0, protocol_udp, TCPIP_TEST_SERVER, 9011, 0, ipReceiver);
    if (result != ACTION_RESULT_SUCCESS && result != 563)
    {
        indicateFailure("Failed to open socket.");
    }
    if (result == 563)
        PRINTF(dbgColor_warn, "Socket 0 found already open!\r");
}


void ipReceiver(socketId_t socketId, const char *data, uint16_t dataSz, const char *rmtHost, const char *rmtPort)
{
    // char recvBuf[XFRBUFFER_SZ] = {0};
    // uint16_t recvdSz;

    // recvdSz = ip_recv(socketId, recvBuf, XFRBUFFER_SZ);
    // recvBuf[recvdSz + 1] = '\0';
    
    char temp[dataSz + 1];
    
    strncpy(temp, data, dataSz);
    temp[dataSz] = '\0';

    PRINTF(dbgColor_info, "appRcvd <%s> @tick=%d\r", temp, timing_millis());
}




/* test helpers
========================================================================================================================= */


void showStats(uint16_t loopCnt, uint32_t waitNext) 
{
    PRINTF(dbgColor_magenta, "\rFreeMem=%u  ", getFreeMemory());
    // PRINTF_WHITE("Next send to ECHO server in %d (millis)\r", waitNext);
    PRINTF(dbgColor_magenta, "<<Loop=%d\r", loopCnt);

    // for (int i = 0; i < 6; i++)
    // {
    //     gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
    //     timing_delay(50);
    //     gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
    //     timing_delay(50);
    // }
}


void indicateFailure(char failureMsg[])
{
	PRINTF(dbgColor_error, "\r\n** %s \r\n", failureMsg);
    PRINTF(dbgColor_error, "** Test Assertion Failed. \r\n");

    int halt = 1;
    while (halt)
    {
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        // timing_delay(1000);
        // gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        // timing_delay(100);
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

