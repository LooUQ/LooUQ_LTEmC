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


#include <lq-embed.h>
#define lqLOG_LEVEL lqLOGLEVEL_DBG                                  ///< Logging detail level for this source file
//#define DISABLE_ASSERTS                                           ///< ASSERT/ASSERT_W enabled by default, can be disabled 
#define LQ_SRCFILE "GNS"                                            ///< create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT


#include "ltemc-internal.h"
#include "ltemc-gnss.h"


#define GNSS_CMD_RESULTBUF_SZ 90                                    ///< GNSS reponse buffer size (parse location from here)
#define GNSS_LOC_DATAOFFSET 12                                      ///< Buffer offset
#define GNSS_LOC_EXPECTED_TOKENCOUNT 11                             ///< Number of expected tokens in GNSS location response
#define GNSS_TIMEOUTms 800                                          ///< GNSS timeout in milliseconds

#define MIN(x, y) (((x) < (y)) ? (x) : (y))                         ///< Return the smaller of two numbers


// private local declarations
// static cmdParseRslt_t gnssLocCompleteParser(const char *response, char **endptr);


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
    atcmd_ovrrdDCmpltTimeout(SEC_TO_MS(2));
    return atcmd_dispatch("AT+QGPS=1");
}


/**
 *	@brief Turn GNSS/GPS subsystem off. 
 */
resultCode_t gnss_off()
{
    return atcmd_dispatch("AT+QGPSEND");
}


/**
 *	@brief Query BGx for current location/positioning information. 
 */
gnssLocation_t gnss_getLocation()
{
    char tokenBuf[12] = {0};
    gnssLocation_t gnssResult = {0};
    gnssResult.statusCode = resultCode__internalError;

    // atcmd_ovrrdParser(gnssLocCompleteParser);
    atcmd_ovrrdDCmpltTimeout(SEC_TO_MS(2));
    atcmd_configParser("+QGPSLOC: ", true, ",", GNSS_LOC_EXPECTED_TOKENCOUNT, "OK\r\n", 0);

    RSLT;
    if (IS_SUCCESS_RSLT(atcmd_dispatch("AT+QGPSLOC=2")))
    {
        gnssResult.statusCode = (rslt == 516) ? resultCode__gone : rslt;                    // translate "No fix" to gone
        if (rslt != resultCode__success)
            return gnssResult;                                                              // return on failure, continue to parse on success

        lqLOG_VRBS("<gnss_getLocation()> parse starting...\r\n");
        char *cmdResponse = atcmd_getResponse();
        char *delimAt;
        lqLOG_VRBS(PRNT_WHITE, "<gnss_getLocation()> response=%s\r\n", cmdResponse);

        if ((delimAt = strchr(cmdResponse, (int)',')) != NULL)
            strncpy(gnssResult.utc, cmdResponse, delimAt - cmdResponse);
        cmdResponse = delimAt + 1;
        lqLOG_VRBS("<gnss_getLocation()> result.utc=%s\r\n", gnssResult.utc);

        gnssResult.lat.val = strtod(cmdResponse, &cmdResponse);
        cmdResponse++;
        gnssResult.lat.dir = ' ';

        gnssResult.lon.val = strtod(cmdResponse, &cmdResponse);
        cmdResponse++;
        gnssResult.lon.dir = ' ';

        lqLOG_INFO("[gnss_getLocation()] location is lat=%f, lon=%f\r\n", gnssResult.lat.val, gnssResult.lon.val);

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
        lqLOG_VRBS("<gnss_getLocation()> parse completed\r\n");
    }
    return gnssResult;
}


#pragma endregion

/* private (static) functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions


// /**
//  *	@brief Action response parser for GNSS location request. 
//  */
// static cmdParseRslt_t gnssLocCompleteParser(const char *response, char **endptr)
// {
//     //const char *response, const char *landmark, char delim, uint8_t minTokens, const char *terminator, char** endptr
//     cmdParseRslt_t parseRslt = atcmd_stdResponseParser("+QGPSLOC: ", true, ",", GNSS_LOC_EXPECTED_TOKENCOUNT, 0, "OK\r\n", 0);
//     DPRINT_V(PRNT_DEFAULT, "<gnssLocCompleteParser()> result=%d\r\n", parseRslt);
//     return parseRslt;
// }

#pragma endregion
