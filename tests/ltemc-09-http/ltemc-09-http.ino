/** ***************************************************************************
  @file 
  @brief LTEm example/test for HTTP(S) client (GET/POST) communications.

  @author Greg Terrell, LooUQ Incorporated

  \loouq
-------------------------------------------------------------------------------

LooUQ-LTEmC // Software driver for the LooUQ LTEm series cellular modems.
Copyright (C) 2017-2023 LooUQ Incorporated

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
Also add information on how to contact you by electronic and paper mail.

**************************************************************************** */

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

#include <lq-SAMDutil.h>                // allows read of reset cause

#include <ltemc.h>
#include <ltemc-tls.h>
#include <ltemc-http.h>
#include <ltemc-files.h>


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


httpCtrl_t httpCtrl_BMS;




static char webPageBuf[1024];
//char cstmHdrs[256];                           // if you use custom HTTP headers then create a buffer to hold them

void setup() {
    #ifdef DIAGPRINT_SERIAL
        Serial.begin(115200);
        #if (SERIAL_OPT > 0)
        while (!Serial) {}      // force wait for serial ready
        #else
        delay(5000);            // just give it some time
        #endif
    #endif

    DPRINT(PRNT_RED, "\rLTEmC test9-HTTP\r");
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
    if (strlen(ntwkOperator->name) > 0)
    {
        DPRINT(PRNT_INFO, "Connected to %s using %s, %d networks available.\r", ntwkOperator->name, ntwkOperator->iotMode, ntwkOperator->pdpCntxtCnt);
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



http_initControl(&httpCtrl_BMS, dataCntxt_2, httpRecvCB);
http_setConnection(&httpCtrl_BMS, "https://ota-cdn.memfault.com/3916/952/11412810952?token=bVDX5Ed95OV5EjBILIFKHYCKjy-qcEqoKov9Ojky02c&expires=1699754400", 80);
DPRINT(PRNT_dGREEN, "URL Host2=%s\r", httpCtrlP.hostUrl);

resultCode_t rslt = http_get(&httpCtrl_BMS, "", false);
rslt = http_readPageToFile(&httpCtrl_BMS, "ota_test.bin");

file_delete("ota_test.bin");

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
            rslt = http_get(&httpCtrlG, "/points/44.7582,-85.6022", http__noResponseHeaders);
            if (rslt == resultCode__success)
            {
                httpCtrl = &httpCtrlG;
                DPRINT(PRNT_INFO, "GET invoked successfully\r");
            }
            else
                DPRINT(PRNT_WARN, "HTTP GET failed, status=%d\r", rslt);
        }
        else
        {
            char postData[] = "{ \"field1\": 1, \"field2\": \"field2\" }";

            // resultCode_t http_post(httpCtrl_t *httpCtrl, const char* url, const char* postData, uint16_t dataSz);
            rslt = http_post(&httpCtrlP, "/anything", http__noResponseHeaders, postData, strlen(postData));
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

