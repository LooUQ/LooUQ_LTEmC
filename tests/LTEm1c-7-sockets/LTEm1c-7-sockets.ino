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

#define HOST_FEATHER_UXPLOR
#include <platform_pins.h>
#include <ltem1c.h>

#define _DEBUG
#include "dbgprint.h"
//#define USE_SERIAL 0


// test setup
#define CYCLE_INTERVAL 5000
#define SEND_BUFFER_SZ 201
#define TCPIP_TEST_SERVER "97.83.32.119"
const int APIN_RANDOMSEED = 0;
uint16_t loopCnt = 0;
uint32_t lastCycle;
bool sendImmediate = false;


// ltem1c 
#define DEFAULT_NETWORK_CONTEXT 1
socketId_t scktNm;
socketResult_t scktResult;
char sendBuf[SEND_BUFFER_SZ] = {0};


void setup() {
    #ifdef USE_SERIAL
        Serial.begin(115200);
        #if (USE_SERIAL)
        while (!Serial) {}
        #else
        delay(1000);
        #endif
    #endif

    PRINTF(dbgColor_white, "\rLTEm1c Test: 7-Sockets\r\n");
    gpio_openPin(LED_BUILTIN, gpioMode_output);

    ltem1_create(ltem1_pinConfig, ltem1Start_powerOn, ltem1Functionality_services);

    PRINTF(dbgColor_none, "Waiting on network...\r");
    networkOperator_t networkOp = ntwk_awaitOperator(30000);
    if (strlen(networkOp.operName) == 0)
        indicateFailure("Timout (30s) waiting for cellular network.");
    PRINTF(dbgColor_info, "Network type is %s on %s\r", networkOp.ntwkMode, networkOp.operName);

    resultCode_t resultCode = ntwk_fetchDataContexts();
    if (resultCode == RESULT_CODE_NOTFOUND)
    {
        resultCode = ntwk_activateContext(DEFAULT_NETWORK_CONTEXT);
        PRINTF(0, "Network PDP context activation=%d\r", resultCode);
    }

    // open socket
    scktResult = sckt_open(0, protocol_udp, TCPIP_TEST_SERVER, 9011, 0, ipReceiver);
    if (scktResult == SOCKET_RESULT_PREVOPEN)
        PRINTF(dbgColor_warn, "Socket 0 found already open!\r");
    else if (scktResult != RESULT_CODE_SUCCESS)
    {
        PRINTF(dbgColor_error, "Socket 0 open failed, resultCode=%d\r", scktResult);
        indicateFailure("Failed to open socket.");
    }
}

uint16_t txCnt = 0;
uint16_t rxCnt = 0;
uint16_t drops = 0;
uint16_t prevRx = 0;

void loop() 
{
    if (timing_millis() - lastCycle >= CYCLE_INTERVAL || sendImmediate)
    {
        sendImmediate = false;
        lastCycle = timing_millis();
        showStats();

        #define SEND_TEST 0

        #if SEND_TEST == 0
        /* test for short-send, fits is 1 TX chunk */
        snprintf(sendBuf, SEND_BUFFER_SZ, "%d-%lu ABCDEFGHIJKLMNOPQRSTUVWXYZ", loopCnt, timing_millis());
        uint16_t sendSz = strlen(sendBuf);
        #elif SEND_TEST == 1
        /* test for longer, 2+ tx chunks */
        snprintf(sendBuf, SEND_BUFFER_SZ, "#%d-0123456789-ABCDEFGHIJKLMNOPQRSTUVWXYZ-abcdefghijklmnopqrstuvwxyz--0123456789-ABCDEFGHIJKLMNOPQRSTUVWXYZ-abcdefghijklmnopqrstuvwxyz", loopCnt);
        uint16_t sendSz = strlen(sendBuf);
        #else
        /* test for transparent data, sockets should allow binary and ignore embedded \0 */
        memcpy(sendBuf, "0123456789\0ABCDEFGHIJKLMNOPQRSTUVWXYZ\0abcdefghijklmnopqrstuvwxyz", 64); 
        uint16_t sendSz = 64;
        #endif


        // typical send loop, there could be a blocking cmd or IO so retry a small number of times
        uint8_t retries = 0;
        do
        {
            scktResult = sckt_send(scktNm, sendBuf, sendSz);
            retries++;
            ltem1_doWork();
        } while (scktResult == RESULT_CODE_CONFLICT && retries < 10);
        txCnt = (scktResult == RESULT_CODE_SUCCESS) ? ++txCnt : txCnt;

        PRINTF((scktResult == RESULT_CODE_SUCCESS) ? dbgColor_cyan : dbgColor_warn, "Send Loop %d, sendRslt=%d \r", loopCnt, scktResult);
        loopCnt++;
    }
    /*
     * NOTE: ltem1_doWork() pipeline requires multiple invokes to complete data receives. 
     * doWork() has no side effects other than taking time and SHOULD BE INVOKED LIBERALLY.
     */
    ltem1_doWork();
}


/**
 *  \brief Application provides a receiver function for incoming (recv'd) data, template given here
 * 
 *  \param [in] socketId  Numeric identifier for the socket receiving the data
 *  \param [in] data  Pointer to character received data ()
*/
void ipReceiver(socketId_t socketId, void *data, uint16_t dataSz)
{
    // char recvBuf[XFRBUFFER_SZ] = {0};
    // uint16_t recvdSz;

    // recvdSz = ip_recv(socketId, recvBuf, XFRBUFFER_SZ);
    // recvBuf[recvdSz + 1] = '\0';
    
    char temp[dataSz + 1];
    
    memcpy(temp, data, dataSz);
    temp[dataSz] = '\0';

    rxCnt++;
    PRINTF(dbgColor_info, "appRcvd (@tick=%d) %s\r", timing_millis(), temp);
}




/* test helpers
========================================================================================================================= */


void showStats() 
{
    if (loopCnt == 0) return;
    
    PRINTF(dbgColor_magenta, "\rFreeMem=%u  ", getFreeMemory());
    PRINTF(dbgColor_magenta, "<<Loop=%d\r", loopCnt);
    PRINTF(dbgColor_magenta, "Tx=%d,  Rx=%d (d=%d)\r", txCnt, rxCnt, drops);
    for (int i = 0; i < 5; i++)
    {
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        timing_delay(50);
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        timing_delay(50);
    }
    if (rxCnt == prevRx)
    {
        drops++;
        PRINTF(dbgColor_warn, "\rMissed recv! (drops=%d)\r", drops);
    }
    prevRx = rxCnt;
}


void indicateFailure(char failureMsg[])
{
	PRINTF(dbgColor_error, "\r\n** %s \r\n", failureMsg);
    PRINTF(dbgColor_error, "** Test Assertion Failed. \r\n");

    int halt = 1;
    while (halt)
    {
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
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

