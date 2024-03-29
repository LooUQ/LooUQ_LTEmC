/** ****************************************************************************
  \file 
  \brief Public API GNSS positioning support
  \author Greg Terrell, LooUQ Incorporated

  \loouq

--------------------------------------------------------------------------------

    This project is released under the GPL-3.0 License.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 
***************************************************************************** */


#define _DEBUG 0                                // set to non-zero value for PRINTF debugging output, 
// debugging output options                     // LTEmC will satisfy PRINTF references with empty definition if not already resolved
#if _DEBUG > 0
    asm(".global _printf_float");               // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                        // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG == 2
    #include <jlinkRtt.h>                       // output debug PRINTF macros to J-Link RTT channel
    #define PRINTF(c_,f_,__VA_ARGS__...) do { rtt_printf(c_, (f_), ## __VA_ARGS__); } while(0)
    #endif
#else
#define PRINTF(c_, f_, ...) 
#endif

#define SRCFILE "GNS"                           // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT
#include "ltemc-internal.h"
#include "ltemc-gnss.h"


#define GNSS_CMD_RESULTBUF_SZ 90
#define GNSS_LOC_DATAOFFSET 12
#define GNSS_LOC_EXPECTED_TOKENCOUNT 11
#define GNSS_TIMEOUTml 800

#define MIN(x, y) (((x) < (y)) ? (x) : (y))


// private local declarations
static cmdParseRslt_t gnssLocCompleteParser(const char *response, char **endptr);


/*
 *  AT+QGPSLOC=2 (format=2)
 *  +QGPSLOC: 113355.0,44.74770,-85.56527,1.2,192.0,2,277.11,0.0,0.0,250420,10
 * --------------------------------------------------------------------------------------------- */


/* public functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions


/**
 *	@brief Turn GNSS/GPS subsystem on. 
 */
resultCode_t gnss_on()
{
    if (atcmd_tryInvoke("AT+QGPS=1"))
    {
        return atcmd_awaitResult();
    }
    return resultCode__conflict;
}


/**
 *	@brief Turn GNSS/GPS subsystem off. 
 */
resultCode_t gnss_off()
{
    if (atcmd_tryInvoke("AT+QGPSEND"))
    {
        return atcmd_awaitResult();
    }
    return resultCode__conflict;
}


/**
 *	@brief Query BGx for current location/positioning information. 
 */
gnssLocation_t gnss_getLocation()
{
    char tokenBuf[12] = {0};
    gnssLocation_t gnssResult;

    //atcmd_t *gnssCmd = atcmd_build("AT+QGPSLOC=2", GNSS_CMD_RESULTBUF_SZ, 500, gnssLocCompleteParser);
    // result sz=86 >> +QGPSLOC: 121003.0,44.74769,-85.56535,1.1,189.0,2,95.45,0.0,0.0,250420,08  + \r\nOK\r\n

    if (ATCMD_awaitLock(atcmd__defaultTimeout))
    {
        atcmd_invokeReuseLock("AT+QGPSLOC=2");
        resultCode_t atResult = atcmd_awaitResultWithOptions(atcmd__defaultTimeout, gnssLocCompleteParser);

        gnssResult.statusCode = atResult;
        if (atResult != resultCode__success)                                            // return on failure, continue on success
            return gnssResult;

        PRINTF(dbgColor__warn, "getLocation(): parse starting...\r");

        char *parsedResponse = atcmd_getResponse();
        char *delimAt;


        if ((delimAt = strchr(parsedResponse, (int)',')) != NULL)
            strncpy(gnssResult.utc, parsedResponse, delimAt - parsedResponse);

        gnssResult.lat.val = strtof(parsedResponse, &parsedResponse);                         // grab a float
        gnssResult.lat.dir = ' ';
        gnssResult.lon.val = strtof(++parsedResponse, &parsedResponse);                       // ++parsedResponse, pre-incr to skip previous comma
        gnssResult.lon.dir = ' ';
        gnssResult.hdop = strtof(++parsedResponse, &parsedResponse);
        gnssResult.altitude = strtof(++parsedResponse, &parsedResponse);
        gnssResult.fixType = strtol(++parsedResponse, &parsedResponse, 10);                   // grab an integer
        gnssResult.course = strtof(++parsedResponse, &parsedResponse);
        gnssResult.speedkm = strtof(++parsedResponse, &parsedResponse);
        gnssResult.speedkn = strtof(++parsedResponse, &parsedResponse);

        if ((delimAt = strchr(parsedResponse, (int)',')) != NULL)
            strncpy(gnssResult.date, tokenBuf, 7);

        gnssResult.nsat = strtol(parsedResponse, &parsedResponse, 10);
        atcmd_close();

        PRINTF(dbgColor__warn, "getLocation(): parse completed\r");
    }
    return gnssResult;
}


#pragma endregion

/* private (static) functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions


/**
 *	@brief Action response parser for GNSS location request. 
 */
static cmdParseRslt_t gnssLocCompleteParser(const char *response, char **endptr)
{
    //const char *response, const char *landmark, char delim, uint8_t minTokens, const char *terminator, char** endptr
    cmdParseRslt_t parseRslt = atcmd_stdResponseParser("+QGPSLOC: ", true, ",", GNSS_LOC_EXPECTED_TOKENCOUNT, 0, "", 0);
    PRINTF(0, "gnssParser(): result=%i\r", parseRslt);
    return parseRslt;
}

#pragma endregion
