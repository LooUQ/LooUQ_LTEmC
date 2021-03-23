/******************************************************************************
 *  \file LTEm1c-7-sockets.ino
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

// define options for how to assemble this build
#define HOST_FEATHER_UXPLOR             // specify the pin configuration

#include <ltem1c.h>
#define _DEBUG                          // enable/expand 
// debugging output options             UNCOMMENT one of the next two lines to direct debug (PRINTF) output
#include <jlinkRtt.h>                   // output debug PRINTF macros to J-Link RTT channel
// #define SERIAL_OPT 1                    // enable serial port comm with devl host (1=force ready test)

#if LTEM1_MQTT == 1
#define success
#endif

// test setup
#define CYCLE_INTERVAL 10000
#define SEND_BUFFER_SZ 201
#define TCPIP_TEST_PROTOCOL 1               //  TCP = 0, UDP = 1, SSL = 2
#define TCPIP_TEST_SERVER "97.83.32.119"
#define TCPIP_TEST_SOCKET 9011

uint16_t loopCnt = 0;
uint32_t lastCycle;

// ltem1c 
#define DEFAULT_NETWORK_CONTEXT 1
socketId_t scktNm;
socketResult_t scktResult;
char sendBuf[SEND_BUFFER_SZ] = {0};


void setup() {
    #ifdef SERIAL_OPT
        Serial.begin(115200);
        #if (SERIAL_OPT > 0)
        while (!Serial) {}      // force wait for serial ready
        #else
        delay(5000);            // just give it some time
        #endif
    #endif

    PRINTFC(dbgColor_error, "\rLTEm1c Test: 7-Sockets\r\n");
    //gpio_openPin(LED_BUILTIN, gpioMode_output);                       // use on-board LED to signal activity

    ltem1_create(ltem1_pinConfig, appNotifRecvr);                       // create base modem object
    sckt_create();                                                      // add optional services : sockets (TCP/UDP/SSL)
    ltem1_start();                                                      // now start modem

    PRINTFC(dbgColor_none, "Waiting on network...\r");
    networkOperator_t networkOp = ntwk_awaitOperator(120 * 1000);
    if (strlen(networkOp.operName) == 0)
        appNotifRecvr(255, "Timeout (120s) waiting for cellular network.");

    PRINTFC(dbgColor_info, "Network type is %s on %s\r", networkOp.ntwkMode, networkOp.operName);

    uint8_t cntxtCnt = ntwk_getActivePdpCntxtCnt();
    if (cntxtCnt == 0)
    {
        ntwk_activatePdpContext(DEFAULT_NETWORK_CONTEXT);
    }

    // open socket
    scktResult = sckt_open(0, (protocol_t)TCPIP_TEST_PROTOCOL, TCPIP_TEST_SERVER, TCPIP_TEST_SOCKET, 0, true, ipReceiver);
    if (scktResult == SOCKET_RESULT_PREVOPEN)
    {
        PRINTFC(dbgColor_warn, "Socket 0 found already open!\r");
    }
    else if (scktResult != RESULT_CODE_SUCCESS)
    {
        PRINTFC(dbgColor_error, "Socket 0 open failed, resultCode=%d\r", scktResult);
        appNotifRecvr(255, "Failed to open socket.");
    }
}

uint16_t txCnt = 0;
uint16_t rxCnt = 0;
uint16_t drops = 0;
uint16_t prevRx = 0;

void loop() 
{
    if (lMillis() - lastCycle >= CYCLE_INTERVAL)
    {
        // PRINTF(dbgColor_magenta, "SPIready=%d\r", sc16is741a_chkCommReady());
        lastCycle = lMillis();
        showStats();

        #define SEND_TEST 0

        #if SEND_TEST == 0
        /* test for short-send, fits is 1 TX chunk */
        snprintf(sendBuf, SEND_BUFFER_SZ, "%d-%lu drops=%d  ABCDEFGHIJKLMNOPQRSTUVWXYZ", loopCnt, lMillis(), drops);
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

        scktResult = sckt_send(scktNm, sendBuf, sendSz);
        if (scktResult == RESULT_CODE_CONFLICT)
        {
            PRINTF(dbgColor_warn, "STOP!\r");
        }
        if (scktResult != RESULT_CODE_SUCCESS)
        {
            PRINTF(dbgColor_warn, "sgnl=%d, scktState=%d\r", mdminfo_rssi(), sckt_getState(0));
            // sckt_close(0);
            // scktResult = sckt_open(0, (protocol_t)TCPIP_TEST_PROTOCOL, TCPIP_TEST_SERVER, TCPIP_TEST_SOCKET, 0, true, ipReceiver);
        }
        txCnt = (scktResult == RESULT_CODE_SUCCESS) ? ++txCnt : txCnt;
        PRINTFC((scktResult == RESULT_CODE_SUCCESS) ? dbgColor_cyan : dbgColor_warn, "Send Loop %d, sendRslt=%d \r", loopCnt, scktResult);

        loopCnt++;
    }
    /*
     * NOTE: ltem1_doWork() pipeline required to complete data receives. doWork() has no side effects other than
     * taking a brief amount of time to check and advance socket pipeline and SHOULD BE INVOKED LIBERALLY.
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
    PRINTFC(dbgColor_info, "appRcvd (@tick=%d) %s\r", lMillis(), temp);
}




/* test helpers
========================================================================================================================= */


void showStats() 
{
    static uint16_t lastTx = 0;
    static uint16_t lastRx = 0;
    static uint16_t lastDrops = 0;

    // for (int i = 0; i < 5; i++)                                          // optional show activity via on-board LED
    // {
    //     gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
    //     lDelay(50);
    //     gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
    //     lDelay(50);
    // }

    uint16_t newTxCnt = txCnt - lastTx;
    uint16_t newRxCnt = rxCnt - lastRx;
    if (newRxCnt < newTxCnt)
        drops++;

    PRINTFC((lastDrops == drops) ? dbgColor_magenta : dbgColor_warn, "\rTx=%d, Rx=%d (d=%d)\r", txCnt, rxCnt, drops);
    PRINTFC(dbgColor_magenta, "FreeMem=%u  Loop=%d\r", getFreeMemory(), loopCnt);

    lastTx = txCnt;
    lastRx = rxCnt;
    lastDrops = drops;
}


void appNotifRecvr(uint8_t notifType, const char *notifMsg)
{
	PRINTFC(dbgColor_error, "\r\n** %s \r\n", notifMsg);
    PRINTFC(dbgColor_error, "** Test Assertion Failed. \r\n");
    gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);

    while (1) {}
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

