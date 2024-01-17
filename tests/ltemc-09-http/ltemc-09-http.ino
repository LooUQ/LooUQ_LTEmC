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
 * Test HTTP(S) protocol client. 
 * 
 * The sketch is designed for debug output to observe results.
 *****************************************************************************/

#include <lq-embed.h>
#define lqLOG_LEVEL lqLOGLEVEL_DBG
//#define DISABLE_ASSERT                                   // ASSERT/_W enabled by default, can be disabled 
#define ASSERT_ACTION_STOP 


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


// two service/helper functions for callback
static void httpRecvCB(dataCntxt_t dataCntxt, char *recvData, uint16_t dataSz, bool isFinal);
static void appEvntNotify(appEvent_t eventType, const char *notifyMsg);


// test setup
#define CYCLE_INTERVAL 30000
uint16_t loopCnt = 1;
uint32_t lastCycle;

// LTEmC variables
/* To avoid having to prefix httpCtrl1 with & in calls below, you can create a pointer
 * variable with: "httpCtrl_t *httpCtrl1 = &httpCtrl;"   */
httpCtrl_t httpCtrlG;
httpCtrl_t httpCtrlP;
httpCtrl_t *httpCtrl;                           // used for common READ 
httpRequest_t noaaReqst;

static char webPageBuf[1024];
resultCode_t rslt;

char pageBffr[2048] = {0};
uint16_t pageChars = 0;


void setup() {
    #if defined(DIAGPRINT_SERIAL) || defined(lqLOG_SERIAL)
        Serial.begin(115200);
        delay(5000);            // just give it some time
    #endif

    lqLOG_NOTICE("\r\nLTEmC-09 HTTP Examples\r\n");

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
            lqLOG_INFO("Searching for provider...");
        else
            break;
    }
    if (ntwk_isReady())
    {
        lqLOG_INFO("Connected to %s using %s\r\n", ntwkOperator->name, ntwkOperator->iotMode);
    }

    /* Basic connectivity established, moving on to HTTPS setup */

    // most sites use tls, 
    // NOTE: the TLS context MUST == the HTTP context if using SSL\TLS; dataContext_0 is this example
    tls_configure(dataCntxt_0, tlsVersion_any, tlsCipher_default, tlsCertExpiration_default, tlsSecurityLevel_default);

    // test sites...
    // https://api.weather.gov/points/44.7582,-85.6022          (returns approx total=1.45KB; headers=811 bytes, body=676 bytes)

    // httpbin.org is a public test site for HTTP and HTTPS maintained by Ken Reitz with support for several test patterns.
    // http://httpbin.org/anything                              (returns approx 850 bytes headers & body)

    /* Not recommeded to use LTEmC HTTP against human targeted web pages, even the simplest are 10K-15K. You can scrape
     * the stream and toss everything on page you don't need, but must be quick about it. The HTTP read stream generally
     * cannot tolerate suspention or being paused. 
     */

    // // create a control for talking to the website
    http_initControl(&httpCtrlG, dataCntxt_0, httpRecvCB);                              // initialize local (internal) structures
    http_setConnection(&httpCtrlG, "https://api.weather.gov", 443);                     // set remote web host
    lqLOG_DBG(lqDARKGREEN, "URL Host1=%s\r", httpCtrlG.hostUrl);

    // you can optionally setup a httpCtrl, EXAMPLE: httpCtrl *httpCtrl = &httpCtrl2
    // Below the &httpCtrl2 style is required since there is no "ptr" variable created (around line 65) to use here

    http_initControl(&httpCtrlP, dataCntxt_1, httpRecvCB);
    http_setConnection(&httpCtrlP, "http://httpbin.org", 80);
    lqLOG_DBG(lqDARKGREEN, "URL Host2=%s\r", httpCtrlP.hostUrl);

    char noaaReqstBffr[256] = {0};
    noaaReqst = http_createRequest(httpRequestType_GET, "https://api.weather.gov", "/points/44.7582,-85.6022", noaaReqstBffr, sizeof(noaaReqstBffr));

    lqLOG_INFO("\r\n  Ntwk Info = %s \r\n", ntwk_getNetworkInfo());

    // lqLOG_INFO("\r\nMemfault OTA Check-In\r\n");
    // memfaultSetup();
    // memfaultCheckIn();

    lqLOG_INFO("\r\nopMetrics Reporting\r\n");
    opMetricsSetup();
    opMetricsPublish();

    lastCycle = 0;
}


void loop()
{
    if (IS_ELAPSED(lastCycle, SEC_TO_MS(5)))
    {
        lastCycle = lqMillis();
        pageChars = 0;
        lqLOG_INFO("\r\n\r\nStarting test loop %d\r\n", loopCnt);

        if (loopCnt % 2 == 1)
        {
            rslt = http_get(&httpCtrlG, "/points/44.7582,-85.6022");                                // default HTTP timeout is 60 seconds
            rslt = http_getCustomRequest(&httpCtrlG, &noaaReqst);

            // rslt = http_get(&httpCtrlG, "/points/44.7582,-85.6022");
            if (rslt == resultCode__success)
            {
                httpCtrl = &httpCtrlG;
                lqLOG_INFO("GET invoked successfully\r");
            }
            else
            {
                lqLOG_INFO("HTTP GET failed, raw-result=%d, std-result=%d\r", rslt, http_translateExtended(rslt));
            }
        }
        else
        {
            char postData[] = "{ \"field1\": 1, \"field2\": \"field2\" }";

            // resultCode_t http_post(httpCtrl_t *httpCtrl, const char* url, const char* postData, uint16_t dataSz);
            rslt = http_post(&httpCtrlP, "/anything", postData, strlen(postData));
            if (rslt == resultCode__success)
            {
                httpCtrl = &httpCtrlP;
                lqLOG_INFO("POST invoked successfully\r");
            }
            else
                lqLOG_INFO("HTTP POST failed, status=%d\r", rslt);
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

            lqLOG_INFO("Request complete, expecting %d chars.\rHTTP Page\r", httpCtrl->pageSize);

            httpResult = http_readPage(httpCtrl);               // content is delivered via the registered page receive callback
            lqLOG_INFO("Read status=%d\r", httpResult);
        }
        loopCnt++;
    }
}   // loop()



/* Memfault check-in -- HTTP GET (with custom headers)
 * ============================================================================================= */

//Memfault check-in
#define OTA_CUSTOMREQUEST_SZ 384
#define OTA_CHECKIN_URL_SZ 256
#define OTA_FIRMWARE_URL_SZ 256
#define OTA_PAGE_SIZE 1024
#define OTA_QUERY_URL "https://device.memfault.com"
#define OTA_PROJECT_KEY "Memfault-Project-Key: 5Zul5eB3Qn1gkEtDFCEs9dn0IXexhr3x"
#define OTA_CUSTOMHDR_SZ (500)
#define OTA_RELEASE_QUERY_SZ (200)
#define OTA_RELEASE_QUERY_HW ("bms-gen2")
#define OTA_RELEASE_QUERY_SW ("application")
#define OTA_CONTENT_URL_SZ (200)
#define OTA_INTERVAL (MIN_TO_MS(5))
#define OTA_PAGE_SIZE (1024)

char otaCheckinReqstBffr[OTA_CUSTOMREQUEST_SZ];
char otaCheckinUrl[OTA_CHECKIN_URL_SZ];
char otaFirmwareUrl[OTA_FIRMWARE_URL_SZ];
char otaPage[OTA_PAGE_SIZE];
httpCtrl_t otaCheckinHttpCtrl;
httpRequest_t otaCheckinHttpReqst;

void memfaultSetup()
{
    // snprintf(otaCheckinUrl, sizeof(otaCheckinUrl), "/api/v0/releases/latest/url?device_serial=%s&hardware_version=%s&software_type=%s&current_version=%s",
    //          "863740069733589",
    //          OTA_RELEASE_QUERY_HW,
    //          OTA_RELEASE_QUERY_SW,
    //          "0.4.8");
    snprintf(otaCheckinUrl, sizeof(otaCheckinUrl), "/api/v0/releases/latest/url?device_serial=%s&hardware_version=%s&software_type=%s&current_version=%s",
             "869738069236557",
             OTA_RELEASE_QUERY_HW,
             OTA_RELEASE_QUERY_SW,
             "0.4.8");

    http_initControl(&otaCheckinHttpCtrl, dataCntxt_1, httpRecvCB);                      // setup HTTP for OTA functions
    http_setConnection(&otaCheckinHttpCtrl, OTA_QUERY_URL, 443);                         // set host target to query for releases

    otaCheckinHttpReqst = http_createRequest(httpRequestType_GET, OTA_QUERY_URL, otaCheckinUrl, otaCheckinReqstBffr, sizeof(otaCheckinReqstBffr));
    http_addCommonHdrs(&otaCheckinHttpReqst, httpHeaderMap_all);
    http_addHeader(&otaCheckinHttpReqst, OTA_PROJECT_KEY);

    lqLOG_INFO("[OTA Setup] Check-In URL=%s%s\r\n", otaCheckinHttpCtrl.hostUrl, otaCheckinUrl);
}


void memfaultCheckIn()
{
    memset(pageBffr, 0, sizeof(pageBffr));

    RSLT = http_getCustomRequest(&otaCheckinHttpCtrl, &otaCheckinHttpReqst);
    lqLOG_INFO("\r\nOTA Check-In: HTTP status=%d\r\n", rslt);

    if (IS_SUCCESS(rslt))
    {
        if (IS_SUCCESS(http_readPage(&otaCheckinHttpCtrl)))
        {
            lqLOG_INFO("OTA URL: %s\r\n", pageBffr);
        }
    }
    else
        lqLOG_INFO("No OTA update is available.\r\n");
}


/* HTTP POST FILE (with custom headers)
 * ============================================================================================= */

//setup for custom HTTP POST action
#define OPM_PROJECT_KEY "LQCloud-OrgKey: 5Zul5eB3Qn1gkEtDFCEs9dn0IXexhr3x\r\n"
#define OPM_HOST_URL "https://devices-dev-pelogical.azurewebsites.net"
#define OPM_BODY_SZ 250

httpCtrl_t httpCtrl_opMetrics;
httpRequest_t httpRqst_opMetrics;
char opmRequestBffr[http__typicalCustomRequestHeaders + OPM_BODY_SZ];
char relativeUrl[] = "/opmetrics/opmrpt/1234567890ABCDEF";

void opMetricsSetup()
{
    http_initControl(&httpCtrl_opMetrics, dataCntxt_1, httpRecvCB);
    http_setConnection(&httpCtrl_opMetrics, "https://devices-dev-pelogical.azurewebsites.net", 443);
    lqLOG_INFO("OpMetrics Reporting Host=%s\r\n", httpCtrl_opMetrics.hostUrl);

    // rslt = http_postFile(&httpCtrl_opMetrics, "/opmetrics/opmrpt/863740069733589", "bmsOpMRpt", false);
    // if (IS_SUCCESS(rslt))
    // {
    //     Serial.printf("[opMetrics] Send success to host: %s\r\n", httpCtrl_opMetrics.hostUrl);
    //     Serial.println("[opMetrics] Report file deleted from filesystem.\r\n");
    // }

    httpRqst_opMetrics = http_createRequest(httpRequestType_POST, OPM_HOST_URL, relativeUrl, opmRequestBffr, sizeof(opmRequestBffr));
    http_addCommonHdrs(&httpRqst_opMetrics, httpHeaderMap_all);
    http_addHeader(&httpRqst_opMetrics, "LQCloud-OrgKey: 5Zul5eB3Qn1gkEtDFCEs9dn0IXexhr3x");

    // create opMetrics file
    
}


void opMetricsPublish()
{
    char postData1[] = "Hello LQCloud, this is what I see...";
    http_addPostData(&httpRqst_opMetrics, postData1, strlen(postData1));

    char postData2[] = "Good things, but a little too much!";
    uint8_t dropped = http_addPostData(&httpRqst_opMetrics, postData2, strlen(postData2));
    // if (dropped)
    //     lqLOG_INFO("\"%s\" didn't all fit! Dropped=%d bytes\r\n", postData2, dropped);

    rslt = http_postCustomRequest(&httpCtrl_opMetrics, relativeUrl, &httpRqst_opMetrics);
    lqLOG_INFO(0,"*** opMetrics POST result=%d\r\n", rslt);
}

/* test helpers
========================================================================================================================= */

// typedef void (*httpRecvFunc_t)(dataCntxt_t dataCntxt, char *recvData, uint16_t dataSz, bool isFinal);

static void httpRecvCB(dataCntxt_t dataCntxt, char * recvData, uint16_t dataSz, bool isFinal)
{
    strncpy(pageBffr + pageChars, recvData, dataSz);
    pageChars += dataSz;

    lqLOG_INFO("AppRecv'd %d new chars, total page sz=%d\r", dataSz, pageChars);
    if (isFinal)
    {
        lqLOG_INFO("Read Complete!\r");
    }
}



static void appEvntNotify(appEvent_t eventType, const char *notifyMsg)
{
    if (eventType > appEvent__FAULTS)
    {
        lqLOG_ERROR("LTEmC Fault: %s\r", notifyMsg);
    }
    else if (eventType > appEvent__WARNINGS)
    {
        lqLOG_WARN("LTEmC Warning: %s\r", notifyMsg);
    }
    else 
    {
        lqLOG_INFO("LTEmC Info: %s\r", notifyMsg);
    }
    return;
}


