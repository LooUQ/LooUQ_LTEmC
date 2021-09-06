/******************************************************************************
 *  \file LTEmC-7-sockets.ino
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
 * Test IP sockets protocol client send/receive.
 * 
 * The sketch is designed for debug output to observe results.
 *****************************************************************************/

//#include <jlinkRtt.h>

#define _DEBUG 2                        // set to non-zero value for PRINTF debugging output, 
// debugging output options             // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if defined(_DEBUG)
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #define PRINTF(c_,f_,__VA_ARGS__...) do { rtt_printf(c_, (f_), ## __VA_ARGS__); } while(0)
    #else
    #define SERIAL_DBG _DEBUG           // enable serial port output using devl host platform serial, _DEBUG 0=start immediately, 1=wait for port
    #endif
#else
#define PRINTF(c_, f_, ...)
#endif


// define options for how to assemble this build
#define HOST_FEATHER_UXPLOR             // specify the pin configuration

#include <ltemc.h>
#include <ltemc-sckt.h>
#include <lq-diagnostics.h>


/* ----------------------------------------------------------------------------
 * For testing TCP\UDP\SSL LooUQ utilizes PacketSender from NAGLECODE.
 * See https://packetsender.com/documentation for more information.
 * ------------------------------------------------------------------------- */

// #if LTEM1_MQTT == 1
// #define success
// #endif

// test setup
#define CYCLE_INTERVAL 10000
#define SEND_BUFFER_SZ 201
#define SCKTTEST_PROTOCOL 1             // Define test protocol: TCP = 0, UDP = 1, SSL = 2
#define SCKTTEST_HOST "24.247.65.244"   // put your server information here 
#define SCKTTEST_PORT 9011              // and here

#define SCKTTEST_RXBUFSZ 256

uint16_t loopCnt = 0;
uint32_t lastCycle;


// ltem1c 
#define DEFAULT_NETWORK_CONTEXT 1

static scktCtrl_t scktCtrl;                     // handle for socket operations
static uint8_t receiveBuffer[SCKTTEST_RXBUFSZ]; // appl creates a rxBuffer for protocols, sized to your expected flows (will be incorporated into scktCtrl)

void setup() {
    #ifdef SERIAL_OPT
        Serial.begin(115200);
        #if (SERIAL_OPT > 0)
        while (!Serial) {}      // force wait for serial ready
        #else
        delay(5000);            // just give it some time
        #endif
    #endif

    PRINTF(dbgColor__red, "\rLTEmC Test:7 Sockets\r\n");
    lqDiag_registerNotifCallback(appNotifyCB);                      // configure ASSERTS to callback into application

    ltem_create(ltem_pinConfig, appNotifyCB);                       // create LTEmC modem
    ltem_start();                                                   // ... and start it

    PRINTF(dbgColor__dflt, "Waiting on network...\r");
    networkOperator_t networkOp = ntwk_awaitOperator(120);
    if (strlen(networkOp.operName) == 0)
        appNotifyCB(255, "Timeout (120s) waiting for cellular network.");

    PRINTF(dbgColor__info, "Network type is %s on %s\r", networkOp.ntwkMode, networkOp.operName);

    uint8_t cntxtCnt = ntwk_getActivePdpCntxtCnt();
    if (cntxtCnt == 0)
    {
        ntwk_activatePdpContext(DEFAULT_NETWORK_CONTEXT);
    }

    // create a socket control and open it
    sckt_initControl(&scktCtrl, dataContext_0, (protocol_t)SCKTTEST_PROTOCOL, receiveBuffer, sizeof(receiveBuffer), scktRecvCB);
    resultCode_t scktResult = sckt_open(&scktCtrl, SCKTTEST_HOST, SCKTTEST_PORT, 0, true);

    if (scktResult == resultCode__previouslyOpened)
    {
        PRINTF(dbgColor__warn, "Socket 0 found already open!\r");
    }
    else if (scktResult != resultCode__success)
    {
        PRINTF(dbgColor__error, "Socket 0 open failed, resultCode=%d\r", scktResult);
        appNotifyCB(255, "Failed to open socket.");
    }
}


void loop() 
{
    if (pMillis() - lastCycle >= CYCLE_INTERVAL)
    {
        lastCycle = pMillis();
        showStats();

        #define SEND_TEST 0

        #if SEND_TEST == 1
        /* test for short-send, fits is 1 TX chunk */
        snprintf(sendBuf, SEND_BUFFER_SZ, "%d-%lu drops=%d  ABCDEFGHIJKLMNOPQRSTUVWXYZ", loopCnt, pMillis(), drops);
        uint16_t sendSz = strlen(sendBuf);
        #elif SEND_TEST == 1
        /* test for longer, 2+ tx chunks */
        snprintf(sendBuf, SEND_BUFFER_SZ, "#%d-0123456789-ABCDEFGHIJKLMNOPQRSTUVWXYZ-abcdefghijklmnopqrstuvwxyz--0123456789-ABCDEFGHIJKLMNOPQRSTUVWXYZ-abcdefghijklmnopqrstuvwxyz", loopCnt);
        uint16_t sendSz = strlen(sendBuf);
        #else
        /* test for transparent data, sockets should allow binary and ignore embedded \0 */
        char sendBuf[] = "01234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ0abcdefghijklmnopqrstuvwxyz";
        uint16_t sendSz = strlen(sendBuf) - 1;
        sendBuf[25] = 0;                                                // test for data transparency, embedded NULL
        #endif

        resultCode_t sendResult = sckt_send(&scktCtrl, sendBuf, sendSz);

        PRINTF(dbgColor__info, "Send result=%d\r", sendResult);
        
        // if (sendResult != resultCode__success)
        // {
        //     if (scktRecover() == resultCode__success)                 // attempt recovery: close/reopen socket if network closed it remotely
        //         sendResult = sckt_send(&scktCtrl, sendBuf, sendSz);
        // }
        // txCnt = (sendResult == resultCode__success) ? ++txCnt : txCnt;
        // PRINTF((sendResult == resultCode__success) ? dbgColor__cyan : dbgColor__warn, "Send Loop %d, sendRslt=%d \r", loopCnt, sendResult);

        loopCnt++;
    }
    /*
     * NOTE: ltem_doWork() pipeline is required to complete data receives. doWork() has no side effects other
     * than taking a brief amount of time to check and advance socket pipeline and SHOULD BE INVOKED LIBERALLY.
     */
    ltem_doWork();
}


uint16_t scktRecover()
{
    PRINTF(dbgColor__warn, "sgnl=%d, scktState=%d\r", mdminfo_rssi(), sckt_getState(&scktCtrl));
    sckt_close(&scktCtrl);
    return sckt_open(&scktCtrl, SCKTTEST_HOST, SCKTTEST_PORT, 0, true);
}


/**
 *  \brief Application provides a receiver function for incoming (recv'd) data, template given here
 * 
 *  \param [in] socketId  Numeric identifier for the socket receiving the data
 *  \param [in] data  Pointer to character received data ()
*/
void scktRecvCB(streamPeer_t socketId, void *data, uint16_t dataSz)
{
    // char recvBuf[XFRBUFFER_SZ] = {0};
    // uint16_t recvdSz;

    // recvdSz = ip_recv(socketId, recvBuf, XFRBUFFER_SZ);
    // recvBuf[recvdSz + 1] = '\0';
    
    char temp[dataSz + 1];
    
    memcpy(temp, data, dataSz);
    temp[dataSz] = '\0';

    PRINTF(dbgColor__info, "appRcvd %d chars: %s\r", dataSz, temp);
}




/* test helpers
========================================================================================================================= */


void showStats() 
{
    static uint16_t lastTx = 0;
    static uint16_t lastRx = 0;
    static uint16_t lastDrops = 0;

    // uint16_t newTxCnt = txCnt - lastTx;
    // uint16_t newRxCnt = rxCnt - lastRx;
    // if (newRxCnt < newTxCnt)
    //     drops++;

    // PRINTF((lastDrops == drops) ? dbgColor__magenta : dbgColor__warn, "\rTX=%d, RX=%d \r", scktCtrl.statsRxCnt, scktCtrl.statsRxCnt);

    PRINTF(dbgColor__magenta, "\rTX=%d, RX=%d \r", scktCtrl.statsRxCnt, scktCtrl.statsRxCnt);
    PRINTF(dbgColor__magenta, "FreeMem=%u  Loop=%d\r", getFreeMemory(), loopCnt);

    // lastTx = txCnt;
    // lastRx = rxCnt;
    // lastDrops = drops;
}


void appNotifyCB(uint8_t notifType, const char *notifMsg)
{
    if (notifType <= lqNotifType__LQDEVICE)
    {
        PRINTF(dbgColor__info, "\r\n** %s \r\n", notifMsg);
        return;
    }

    switch (notifType)
    {
    case lqNotifType_assertWarning:
        PRINTF(dbgColor__warn, "%s\r\n", notifMsg);
        return;
    case lqNotifType_lqDevice_recvOverflow:
        PRINTF(dbgColor__warn, "ProtocolError: %s\r", notifMsg);
        return;
    case lqNotifType_lqDevice_hwFault:
        PRINTF(dbgColor__warn, "HardwareError: %s\r", notifMsg);
        break;
    case lqNotifType_lqDevice_ntwkFault:
        PRINTF(dbgColor__warn, "NetworkError: %s\r", notifMsg);
        break;
    case lqNotifType_lqDevice_streamFault:
        PRINTF(dbgColor__warn, "ProtocolError: %s\r", notifMsg);
    default:
        PRINTF(dbgColor__error, "\r\n** %s \r\n", notifMsg);
        break;
    }
    do {} while (1);
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

