/******************************************************************************
 *  \file LTEmC-9-http.ino
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2021 LooUQ Incorporated.
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
 * Test HTTPS protocol client request to github. 
 * 
 * The sketch is designed for debug output to observe results.
 *****************************************************************************/

#define _DEBUG 2                        // set to non-zero value for PRINTF debugging output, 
// debugging output options             // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if _DEBUG > 0
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG == 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #define PRINTF(c_,f_,__VA_ARGS__...) do { rtt_printf(c_, (f_), ## __VA_ARGS__); } while(0)
    #endif
#else
#define PRINTF(c_, f_, ...) 
#endif


// define options for how to assemble this build
#define HOST_FEATHER_UXPLOR             // specify the pin configuration

#include <ltemc.h>
#include <ltemc-tls.h>
#include <ltemc-http.h>

#define DEFAULT_NETWORK_CONTEXT 1
// #define ASSERT(expected_true, failMsg)  if(!(expected_true))  appNotifyCB(255, failMsg)
// #define ASSERT_NOTEMPTY(string, failMsg)  if(string[0] == '\0') appNotifyCB(255, failMsg)

// test setup
#define CYCLE_INTERVAL 30000
uint16_t loopCnt = 1;
uint32_t lastCycle;

// ltem1 variables
static httpCtrl_t httpCtrl;
static httpCtrl_t *httpPtr = &httpCtrl;
static char webPageBuf[1024];
//char cstmHdrs[256];                           // if you use custom HTTP headers then create a buffer to hold them

void setup() {
    #ifdef SERIAL_OPT
        Serial.begin(115200);
        #if (SERIAL_OPT > 0)
        while (!Serial) {}      // force wait for serial ready
        #else
        delay(5000);            // just give it some time
        #endif
    #endif

    PRINTF(dbgColor__red, "\rLTEm1c test9-HTTP\r\n");
    gpio_openPin(LED_BUILTIN, gpioMode_output);

    ltem_create(ltem_pinConfig, appNotifyCB);
    ltem_start();

    PRINTF(dbgColor__none, "Waiting on network...\r");
    networkOperator_t networkOp = ntwk_awaitOperator(30000);
    if (strlen(networkOp.operName) == 0)
        appNotifyCB(255, "Timout (30s) waiting for cellular network.");
    PRINTF(dbgColor__info, "Network type is %s on %s\r", networkOp.ntwkMode, networkOp.operName);

    uint8_t cntxtCnt = ntwk_getActivePdpCntxtCnt();
    if (cntxtCnt == 0)
    {
        ntwk_activatePdpContext(DEFAULT_NETWORK_CONTEXT);
    }

    /* Basic connectivity established, moving on to HTTPS setup */

    // most sites use tls, 
    // NOTE: the TLS context MUST == the HTTP context if using SSL\TLS; dataContext_0 is this example
    tls_configure(dataContext_0, tlsVersion_any, tlsCipher_default, tlsCertExpiration_default, tlsSecurityLevel_default);

    // alternate test sites...
    // https://api.weather.gov/points/44.7582,-85.6022

    // RECOMMENDED LOOK
    // httpbin.org is a public test site for HTTP and HTTPS maintained by Ken Reitz with support for several test patterns.
    // http://httpbin.org/anything

    /* Not recommeded to use LTEmC HTTP against human targeted web pages, even the simplest are 10K-15K. You can scrape
     * the stream and toss everything on page you don't need, but must be quick about it. The HTTP read stream cannot
     * be suspended or paused. 
     * 
     * -- https://web.ics.purdue.edu/~gchopra/class/public/pages/webdesign/05_simple.html
     * -- https://forecast.weather.gov/obslocal.php?warnzone=MIZ026&local_place=Traverse%20City%20MI&zoneid=EDT&offset=14400
     */

    // // create a control for talking to the website
    // http_initControl(httpPtr, dataContext_0, "https://api.weather.gov", webPageBuf, sizeof(webPageBuf), httpRecvCB);
    // PRINTF(dbgColor__dGreen, "URL Host=%s\r", httpCtrl.urlHost);

    http_initControl(httpPtr, dataContext_0, "http://httpbin.org", webPageBuf, sizeof(webPageBuf), httpRecvCB);
    PRINTF(dbgColor__dGreen, "URL Host=%s\r", httpCtrl.urlHost);
}

resultCode_t rslt;
char pageBuffer[4096] = {0};
uint16_t pageChars = 0;


void loop() 
{
    // if (pMillis() - lastCycle >= CYCLE_INTERVAL)
    if (pMillis() - lastCycle >= 5000)
    {
        lastCycle = pMillis();
        pageChars = 0;

        // // resultCode_t http_get(httpCtrl_t *httpCtrl, const char* url, uint8_t timeoutSeconds);
        // rslt = http_get(httpPtr, "/points/44.7582,-85.6022", http__noResponseHeaders, http__useDefaultTimeout);
        // if (rslt == resultCode__success)
        // {
        //     PRINTF(dbgColor__info, "GET invoked successfully\r");
        // }
        // else
        //     PRINTF(dbgColor__warn, "HTTP request failed\r");


        char postData[] = "{ \"field1\": 1, \"field2\": \"field2\" }";

        // resultCode_t http_post(httpCtrl_t *httpCtrl, const char* url, const char* postData, uint16_t dataSz, uint8_t timeoutSeconds);
        rslt = http_post(httpPtr, "/anything", http__noResponseHeaders, postData, strlen(postData), http__useDefaultTimeout);
        if (rslt == resultCode__success)
        {
            PRINTF(dbgColor__info, "POST invoked successfully\r");
        }
        else
            PRINTF(dbgColor__warn, "HTTP request failed\r");


        /* The page request (GET\POST) has completed at this point and all or most of the web content has be retrieved
         * to the LTEm modem device. If all you care about is the status (200, 400, 404 etc.), your done.
         *
         * If you want to read the web content returned use the http_readPage() function to initiate a background
         * stream to your HTTP callback function. The http_readPage() returns when the background processing has
         * finished. You can cancel the read page stream while it is underway (if your only need some content near
         * the start of the page and you are parsing form it as it arrives). 
         * 
         * Note: The http_readPage() process cannot be paused or suspended, only cancelled. This process streams very
         * quickly so any processing in your callback method should be limited and return control quickly. Otherwise
         * buffer your page and analyze the page content later.
         *-----------------------------------------------------------------------------------------------------------*/

        if (rslt == resultCode__success)
        {
            if (rslt = http_readPage(httpPtr, 20))
            {
                switch (rslt)
                {
                case resultCode__success:
                    PRINTF(dbgColor__warn, "Read page complete, %d chars received.", pageChars);
                    break;

                case resultCode__cancelled:
                    PRINTF(dbgColor__warn, "Read page cancelled after %d chars.", pageChars);
                    break;
                
                default:
                    break;
                }
            }
        }


        // rslt = http_awaitResult(httpPtr, 5);                         // block here (there a yields() behind the scenes)

        // -OR-

        // uint32_t ckCnt = 1;
        // rslt = http_getFinalResult(httpPtr);
        // while (rslt == 0)              // perform other application duties in a loop
        // {
        //     PRINTF(dbgColor__gray, ".", ckCnt);
        //     lDelay(1000);
        //     ckCnt++;
        //     /* do something else */
        //     rslt = http_getFinalResult(httpPtr);
        // }

        // PRINTF(dbgColor__info, "Web request completed with status=%d\r", rslt);

        // -OR-

        // do nothing to retrieve status, if you don't care. Callback func() will receive data until page cancelled or complete
    }

    /* NOTE: Advance data pipeline, ltem_doWork(). DoWork has no side effects other than taking a small amount of
     * time to check and forward receive buffers. The ltem_doWork() function SHOULD BE invoked frequently. */
    ltem_doWork();
}


// typedef void (*httpRecvFunc_t)(dataContext_t dataCntxt, char *data, uint16_t dataSz);

void httpRecvCB(dataContext_t cntxt, uint16_t httpStatus, char *recvData, uint16_t dataSz)
{
    // recvData[dataSz-1] = '\0';
    // PRINTF(dbgColor__info, "(%d) %s", httpStatus, recvData);
    memcpy(pageBuffer + pageChars, recvData, dataSz);
    pageChars += dataSz;

    if (loopCnt % 2 == 1 && pageChars >= 1000)
    {
        http_cancelPage(httpPtr);
    }
}



/* test helpers
========================================================================================================================= */

void appNotifyCB(uint8_t notifType, const char *notifMsg)
{
	PRINTF(dbgColor__error, "\r\n** %s \r\n", notifMsg);
    PRINTF(dbgColor__error, "** Test Assertion Failed. \r\n");

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

