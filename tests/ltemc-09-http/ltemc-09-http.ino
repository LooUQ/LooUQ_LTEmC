/******************************************************************************
 *  \file ltemc-9-http.ino
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

#include <lq-embed.h>
#define LOG_LEVEL LOGLEVEL_DBG
//#define DISABLE_ASSERTS                                   // ASSERT/ASSERT_W enabled by default, can be disabled 

#define ENABLE_DIAGPRINT                    // expand DIAGPRINT into debug output
//#define ENABLE_DIAGPRINT_VERBOSE            // expand DIAGPRINT and DIAGPRINT_V into debug output
#define ENABLE_ASSERT


/* specify the pin configuration 
 * --------------------------------------------------------------------------------------------- */
#ifdef ARDUINO_ARCH_ESP32
    #define HOST_ESP32_DEVMOD_BMS
#else
    #define HOST_FEATHER_UXPLOR_L
    // #define HOST_FEATHER_UXPLOR             
    // #define HOST_FEATHER_LTEM3F
#endif

#include <ltemc.h>
#include <ltemc-tls.h>
#include <ltemc-http.h>

#define PDP_DATA_CONTEXT 1
#define PDP_APN_NAME "hologram"

// #define ASSERT(expected_true, failMsg)  if(!(expected_true))  appNotifyCB(255, failMsg)
// #define ASSERT_NOTEMPTY(string, failMsg)  if(string[0] == '\0') appNotifyCB(255, failMsg)

// test setup
#define CYCLE_INTERVAL 30000
uint16_t loopCnt = 1;
uint32_t lastCycle;

// ltem1 variables
/* To avoid having to prefix httpCtrl1 with & in calls below, you can create a pointer
 * variable with: "httpCtrl_t *httpCtrl1 = &httpCtrl;"   */
httpCtrl_t httpCtrlG;
httpCtrl_t httpCtrlP;
httpCtrl_t *httpCtrl;                            // used for common READ 

static char webPageBuf[1024];
//char cstmHdrs[256];                           // if you use custom HTTP headers then create a buffer to hold them

void setup() {
    #if defined(DIAGPRINT_SERIAL) || defined(LOG_SERIAL)
        Serial.begin(115200);
        #if (SERIAL_OPT > 0)
        while (!Serial) {}      // force wait for serial ready
        #else
        delay(5000);            // just give it some time
        #endif
    #endif

    DPRINT(PRNT_RED, "\rLTEmC-09 HTTP Examples\r");
    //lqDiag_setNotifyCallback(appEvntNotify);

    ltem_create(ltem_pinConfig, NULL, appEvntNotify);                       // no yield req'd for testing
    ntwk_setOperatorScanMode(ntwkScanMode_lteonly);
    ntwk_setIotMode(ntwkIotMode_M1);
    ntwk_setDefaultNetwork(PDP_DATA_CONTEXT, pdpProtocol_IPV4, PDP_APN_NAME);
    ltem_start(resetAction_swReset);

    ntwkOperator_t *ntwkOperator;
    while(true)
    {
        ntwkOperator = ntwk_awaitOperator(PERIOD_FROM_SECONDS(15));
        if (STREMPTY(ntwkOperator->name))
            DPRINT(PRNT_WARN, "Searching for provider...");
        else
            break;
    }
    if (ntwk_isReady(false))
    {
        DPRINT(PRNT_INFO, "Connected to %s using %s\r\n", ntwkOperator->name, ntwkOperator->iotMode);
    }

    /* Basic connectivity established, moving on to HTTPS setup */

    // most sites use tls, 
    // NOTE: the TLS context MUST == the HTTP context if using SSL\TLS; dataContext_0 is this example
    tls_configure(dataCntxt_0, tlsVersion_any, tlsCipher_default, tlsCertExpiration_default, tlsSecurityLevel_default);

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
    http_initControl(&httpCtrlG, dataCntxt_0, httpRecvCB);                  // initialize local (internal) structures
    http_setConnection(&httpCtrlG, "https://api.weather.gov", 443);                                         // set remote web host
    DPRINT(PRNT_dGREEN, "URL Host1=%s\r", httpCtrlG.hostUrl);

    // you can optionally setup a httpCtrl, EXAMPLE: httpCtrl *httpCtrl = &httpCtrl2
    // Below the &httpCtrl2 style is required since there is no "ptr" variable created (around line 65) to use here

    http_initControl(&httpCtrlP, dataCntxt_1, httpRecvCB);
    http_setConnection(&httpCtrlP, "http://httpbin.org", 80);
    DPRINT(PRNT_dGREEN, "URL Host2=%s\r", httpCtrlP.hostUrl);

    #define OPM_PROJECT_KEY "LQCloud-OrgKey: 5Zul5eB3Qn1gkEtDFCEs9dn0IXexhr3x\r\n"
    #define OPM_HOST_URL "https://devices-dev-pelogical.azurewebsites.net"
    #define OPM_BODY_SZ 50

    httpCtrl_t httpCtrl_opMetrics;
    char opmRequestBffr[http__typicalCustomRequestHeaders + OPM_BODY_SZ];
    char relativeUrl[] = "/opmetrics/opmrpt/1234567890ABCDEF";
    resultCode_t rslt;
    
    //setup for custom HTTP POST action
    http_initControl(&httpCtrl_opMetrics, dataCntxt_1, httpRecvCB);
    http_setConnection(&httpCtrl_opMetrics, "https://devices-dev-pelogical.azurewebsites.net", 443);
    DPRINT(PRNT_dMAGENTA, "OpMetrics Reporting Host=%s\r\n", httpCtrl_opMetrics.hostUrl);

    httpRequest_t opmRequest = http_createRequest(httpRequestType_POST, OPM_HOST_URL, relativeUrl, opmRequestBffr, sizeof(opmRequestBffr));
    http_addCommonHdrs(&opmRequest, httpHeaderMap_all);
    http_addHeader(&opmRequest, "LQCloud-OrgKey: 5Zul5eB3Qn1gkEtDFCEs9dn0IXexhr3x");

    char postData1[] = "Hello LQCloud, this is what I see...";
    http_addPostData(&opmRequest, postData1, strlen(postData1));

    char postData2[] = "Good things, but a little too much!";
    uint8_t dropped = http_addPostData(&opmRequest, postData2, strlen(postData2));
    if (dropped)
        DPRINT(PRNT_WARN, "\"%s\" didn't all fit! Dropped=%d bytes\r\n", postData2, dropped);

    rslt = http_postCustomRequest(&httpCtrl_opMetrics, relativeUrl, &opmRequest, false);
    DPRINT(0,"*** opMetrics POST result=%d\r\n", rslt);
}

resultCode_t rslt;
char pageBffr[101] = {0};
uint16_t pageChars = 0;


void loop()
{
    // if (pMillis() - lastCycle >= CYCLE_INTERVAL)
    if (pMillis() - lastCycle >= 5000)
    {
        lastCycle = pMillis();
        pageChars = 0;
        DPRINT(PRNT_DEFAULT, "\r\r");

        if (loopCnt % 2 == 1)
        {
            // resultCode_t http_get(httpCtrl_t *httpCtrl, const char* url)   
            // default HTTP timeout is 60 seconds
            rslt = http_get(&httpCtrlG, "/points/44.7582,-85.6022", http__noReturnResponseHeaders);
            if (rslt == resultCode__success)
            {
                httpCtrl = &httpCtrlG;
                DPRINT(PRNT_INFO, "GET invoked successfully\r");
            }
            else
            {
                DPRINT(PRNT_WARN, "HTTP GET failed, raw-result=%d, std-result=%d\r", rslt, http_translateExtended(rslt));
            }
        }
        else
        {
            char postData[] = "{ \"field1\": 1, \"field2\": \"field2\" }";

            // resultCode_t http_post(httpCtrl_t *httpCtrl, const char* url, const char* postData, uint16_t dataSz);
            rslt = http_post(&httpCtrlP, "/anything", postData, strlen(postData), http__noReturnResponseHeaders);
            if (rslt == resultCode__success)
            {
                httpCtrl = &httpCtrlP;
                DPRINT(PRNT_INFO, "POST invoked successfully\r");
            }
            else
                DPRINT(PRNT_WARN, "HTTP POST failed, status=%d\r", rslt);
        }


        /* The page request (GET\POST) has completed at this point and all or most of the web content has been
         * retrieved to the LTEm modem device. If all you care about is the status (200, 400, 404 etc.), your done.
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
            bool morePage = false;
            uint16_t httpResult;

            DPRINT(PRNT_WHITE, "Request complete, expecting %d chars.\rHTTP Page\r", httpCtrl->pageSize);

            httpResult = http_readPage(httpCtrl);               // content is delivered via the registered page receive callback
            DPRINT(PRNT_MAGENTA, "Read status=%d\r", httpResult);
        }
        loopCnt++;
    }
}


// typedef void (*httpRecvFunc_t)(dataCntxt_t dataCntxt, char *recvData, uint16_t dataSz, bool isFinal);

void httpRecvCB(dataCntxt_t dataCntxt, char *recvData, uint16_t dataSz, bool isFinal)
{
    //strncpy(pageBffr + pageChars, recvData, dataSz);
    pageChars += dataSz;

    DPRINT(PRNT_MAGENTA, "AppRecv'd %d new chars, total page sz=%d\r", dataSz, pageChars);
    if (isFinal)
    {
        DPRINT(PRNT_MAGENTA, "Read Complete!\r");
    }
}



/* test helpers
========================================================================================================================= */

void appEvntNotify(appEvent_t eventType, const char *notifyMsg)
{
    if (eventType > appEvent__FAULTS)
    {
        DPRINT(PRNT_ERROR, "LTEmC Fault: %s\r", notifyMsg);
    }
    else if (eventType > appEvent__WARNINGS)
    {
        DPRINT(PRNT_WARN, "LTEmC Warning: %s\r", notifyMsg);
    }
    else 
    {
        DPRINT(PRNT_WHITE, "LTEmC Info: %s\r", notifyMsg);
    }
    return;
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

