/** ***************************************************************************
  @file 
  @brief Modem GNSS location features/services.

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


#define SRCFILE "GNS"                       // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT
//#define ENABLE_DIAGPRINT                    // expand DPRINT into debug output
//#define ENABLE_DIAGPRINT_VERBOSE            // expand DPRINT and DPRINT_V into debug output
#define ENABLE_ASSERT
#include <lqdiag.h>

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
        return atcmd_awaitResultWithOptions(SEC_TO_MS(2), NULL);
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
    gnssLocation_t gnssResult = {0};
    gnssResult.statusCode = resultCode__internalError;

    //atcmd_t *gnssCmd = atcmd_build("AT+QGPSLOC=2", GNSS_CMD_RESULTBUF_SZ, 500, gnssLocCompleteParser);
    // result sz=86 >> +QGPSLOC: 121003.0,44.74769,-85.56535,1.1,189.0,2,95.45,0.0,0.0,250420,08  + \r\nOK\r\n

    if (atcmd_tryInvoke("AT+QGPSLOC=2"))
    {
        resultCode_t rslt = atcmd_awaitResultWithOptions(2000, gnssLocCompleteParser);
        gnssResult.statusCode = (rslt == 516) ? resultCode__gone : rslt;                    // translate "No fix" to gone
        if (rslt != resultCode__success)
            return gnssResult;                                                              // return on failure, continue to parse on success

        DPRINT_V(PRNT_WARN, "<gnss_getLocation()> parse starting...\r\n");
        char *cmdResponse = atcmd_getResponse();
        char *delimAt;
        DPRINT_V(PRNT_WHITE, "<gnss_getLocation()> response=%s\r\n", cmdResponse);

        if ((delimAt = strchr(cmdResponse, (int)',')) != NULL)
            strncpy(gnssResult.utc, cmdResponse, delimAt - cmdResponse);
        cmdResponse = delimAt + 1;
        DPRINT_V(PRNT_WHITE, "<gnss_getLocation()> result.utc=%s\r\n", gnssResult.utc);

        gnssResult.lat.val = strtod(cmdResponse, &cmdResponse);
        cmdResponse++;
        gnssResult.lat.dir = ' ';

        gnssResult.lon.val = strtod(cmdResponse, &cmdResponse);
        cmdResponse++;
        gnssResult.lon.dir = ' ';

        DPRINT(PRNT_WHITE, "[gnss_getLocation()] location is lat=%f, lon=%f\r\n", gnssResult.lat.val, gnssResult.lon.val);

        gnssResult.hdop = strtod(cmdResponse + 1, &cmdResponse);
        gnssResult.altitude = strtod(cmdResponse + 1, &cmdResponse);
        gnssResult.fixType = strtol(cmdResponse + 1, &cmdResponse, 10);
        gnssResult.course = strtod(cmdResponse + 1, &cmdResponse);
        gnssResult.speedkm = strtod(cmdResponse + 1, &cmdResponse);
        gnssResult.speedkn = strtod(cmdResponse + 1, &cmdResponse);

        if ((delimAt = strchr(cmdResponse, (int)',')) != NULL)
            strncpy(gnssResult.date, tokenBuf, 7);

        gnssResult.nsat = strtol(cmdResponse, NULL, 10);
        gnssResult.statusCode = resultCode__success;
        atcmd_close();
        DPRINT_V(PRNT_WARN, "<gnss_getLocation()> parse completed\r\n");
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
    cmdParseRslt_t parseRslt = atcmd_stdResponseParser("+QGPSLOC: ", true, ",", GNSS_LOC_EXPECTED_TOKENCOUNT, 0, "OK\r\n", 0);
    DPRINT_V(PRNT_DEFAULT, "<gnssLocCompleteParser()> result=%d\r\n", parseRslt);
    return parseRslt;
}

#pragma endregion
