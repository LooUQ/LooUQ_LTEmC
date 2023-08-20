/******************************************************************************
 *  \file ltemc-7-sockets.ino
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

#define ENABLE_DIAGPRINT                    // expand DPRINT into debug output
//#define ENABLE_DIAGPRINT_VERBOSE            // expand DPRINT and DPRINT_V into debug output
#define ENABLE_ASSERT
#include <lqdiag.h>

/* specify the pin configuration 
 * --------------------------------------------------------------------------------------------- */
#ifdef ARDUINO_ARCH_ESP32
    #define HOST_ESP32_DEVMOD_BMS
#else
    #define HOST_FEATHER_UXPLOR_L
    // #define HOST_FEATHER_UXPLOR             
    // #define HOST_FEATHER_LTEM3F
#endif

#define PDP_DATA_CONTEXT 1
#define PDP_APN_NAME "hologram"


#include <ltemc.h>
#include <ltemc-sckt.h>


/* ----------------------------------------------------------------------------
 * For testing TCP\UDP\SSL LooUQ utilizes PacketSender from NAGLECODE.
 * See https://packetsender.com/documentation for more information.
 * ------------------------------------------------------------------------- */

// test setup
#define CYCLE_INTERVAL 10000
#define SCKTTEST_TXBUFSZ 256
#define SCKTTEST_RXBUFSZ 256

#define SCKTTEST_PROTOCOL streamType_UDP
#define SCKTTEST_HOST "71.13.234.38"    // put your test host information here 
#define SCKTTEST_PORT 9011              // and here


uint16_t loopCnt = 0;
uint32_t lastCycle;

static scktCtrl_t scktCtrl;                     // handle for socket operations
static uint8_t receiveBuffer[SCKTTEST_RXBUFSZ]; // appl creates a rxBuffer for protocols, sized to your expected flows (will be incorporated into scktCtrl)

void setup() {
    #ifdef DIAGPRINT_SERIAL
        Serial.begin(115200);
        delay(5000);            // just give it some time
    #endif

    DPRINT(PRNT_RED, "\rLTEmC Test:7 Sockets\r\n");
    //lqDiag_setNotifyCallback(appEvntNotify);                        // configure ASSERTS to callback into application

    ltem_create(ltem_pinConfig, NULL, appEvntNotify);               // create LTEmC modem, no yield req'd for testing
    ltem_start(resetAction_swReset);                                // ... and start it
    DPRINT(PRNT_DEFAULT, "BGx %s\r", mdminfo_ltem()->fwver);

    DPRINT(PRNT_DEFAULT, "Waiting on network...\r");
    providerInfo_t* provider = ntwk_awaitProvider(PERIOD_FROM_SECONDS(15));
    while (strlen(provider->name) == 0)
    {
        DPRINT(PRNT_dYELLOW, ">");
    }
    DPRINT(PRNT_INFO, "Network type is %s on %s\r", provider->iotMode, provider->name);

    // create a socket control and open it
    sckt_initControl(&scktCtrl, dataCntxt_0, SCKTTEST_PROTOCOL, scktRecvCB);
    sckt_setConnection(&scktCtrl, PDP_DATA_CONTEXT, SCKTTEST_HOST, SCKTTEST_PORT, 0);
    resultCode_t scktResult = sckt_open(&scktCtrl,  true);

    if (scktResult == resultCode__previouslyOpened)
    {
        DPRINT(PRNT_WARN, "Socket 0 found already open!\r");
    }
    else if (scktResult != resultCode__success)
    {
        DPRINT(PRNT_ERROR, "Socket 0 open failed, resultCode=%d\r", scktResult);
        while(true){}
    }
}

char sendBffr[SCKTTEST_TXBUFSZ];

void loop() 
{
    if (pMillis() - lastCycle >= CYCLE_INTERVAL)
    {
        lastCycle = pMillis();
        showStats();

        #define SEND_TEST 0
        uint8_t ctrlZ = 0x1a;
        memset(sendBffr, 0, sizeof(sendBffr));

        #if SEND_TEST == 1
        /* test for short-send, fits is 1 TX chunk */
        snDPRINT(sendBffr, SCKTTEST_TXBUFSZ, "%d-%lu drops=%d  ABCDEFGHIJKLMNOPQRSTUVWXYZ%c", loopCnt, pMillis(), drops, ctrlZ);

        #elif SEND_TEST == 2
        /* test for longer, 2+ tx chunks */
        snDPRINT(sendBffr, SCKTTEST_TXBUFSZ, "#%d-0123456789-ABCDEFGHIJKLMNOPQRSTUVWXYZ-abcdefghijklmnopqrstuvwxyz--0123456789-ABCDEFGHIJKLMNOPQRSTUVWXYZ-abcdefghijklmnopqrstuvwxyz%c", loopCnt, ctrlZ);

        #else
        /* test for transparent data, sockets should allow binary and ignore embedded \0 */
        strcpy(sendBffr, "01234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ0abcdefghijklmnopqrstuvwxyz");
        sendBffr[strlen(sendBffr)] = ctrlZ;
        uint16_t sendSz = strlen(sendBffr);                                 // get length before inserting \0
        sendBffr[25] = 0;                                                   // test for data transparency, embedded NULL
        #endif

        resultCode_t sendResult = sckt_send(&scktCtrl, sendBffr, sendSz);
        DPRINT(PRNT_INFO, "Send result=%d\r", sendResult);
        
        loopCnt++;
    }
    /* NOTE: ltem1_eventMgr() background pipeline processor is required for async RECEIVE operations; like UDP/TCP receive.
     *       Event manager is light weight and has no side effects other than taking time, it should be invoked liberally. 
     */
    ltem_eventMgr();
}


uint16_t scktRecover()
{
    DPRINT(PRNT_WARN, "sgnl=%d, scktState=%d\r", mdminfo_signalRSSI(), sckt_getState(&scktCtrl));
    sckt_close(&scktCtrl);
    return sckt_open(&scktCtrl, true);
}


/**
 *  \brief Application provides a receiver function for incoming (recv'd) data, template given here
 * 
 *  \param [in] dataCntxt  Numeric identifier for the socket receiving the data
 *  \param [in] data  Pointer to character received data ()
*/
void scktRecvCB(dataCntxt_t dataCntxt, char* dataPtr, uint16_t dataSz, bool isFinal)
{
    // char temp[dataSz + 1];

    // sckt_fetchRecv(&scktCtrl, temp, sizeof(temp));
    // temp[dataSz] = '\0';

    // DPRINT(PRNT_INFO, "appRcvd %d chars: %s\r", dataSz, temp);
}



/* test helpers
========================================================================================================================= */

void appEvntNotify(appEvent_t eventType, const char *notifyMsg)
{
    if (eventType == appEvent_fault_assertFailed)
        DPRINT(PRNT_ERROR, "LTEmC Fault: %s\r", notifyMsg);
    else 
        DPRINT(PRNT_WHITE, "LTEmC Info: %s\r", notifyMsg);
    return;
}


void showStats() 
{
    static uint16_t lastTx = 0;
    static uint16_t lastRx = 0;
    static uint16_t lastDrops = 0;

    // uint16_t newTxCnt = txCnt - lastTx;
    // uint16_t newRxCnt = rxCnt - lastRx;
    // if (newRxCnt < newTxCnt)
    //     drops++;

    // DPRINT((lastDrops == drops) ? PRNT_MAGENTA : PRNT_WARN, "\rTX=%d, RX=%d \r", scktCtrl.statsRxCnt, scktCtrl.statsRxCnt);

    DPRINT(PRNT_MAGENTA, "\rTX=%d, RX=%d \r", scktCtrl.statsRxCnt, scktCtrl.statsRxCnt);
    DPRINT(PRNT_MAGENTA, "FreeMem=%u  Loop=%d\r", getFreeMemory(), loopCnt);

    // lastTx = txCnt;
    // lastRx = rxCnt;
    // lastDrops = drops;
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

