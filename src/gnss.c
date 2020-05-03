// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "ltem1c.h"

#define GNSS_CMD_RESULTBUF_SZ 90
#define GNSS_LOC_DATAOFFSET 12
#define GNSS_LOC_EXPECTED_TOKENCOUNT 11

#define MIN(x, y) (((x) < (y)) ? (x) : (y))


/*
AT+QGPSLOC=2

+QGPSLOC: 113355.0,44.74770,-85.56527,1.2,192.0,2,277.11,0.0,0.0,250420,10

OK
*/

static action_result_t gnssLocCompleteParser(const char *response)
{
    action_result_t result = action_tokenResultParser(response, "+QGPSLOC:", ASCII_cCOMMA, GNSS_LOC_EXPECTED_TOKENCOUNT);

    if (result != ACTION_RESULT_PENDING)
        return result;
    return action_cmeResultParser(response);
}


/* --------------------------------------------------------------------------------------------- */


action_result_t gnss_on()
{
    action_invoke("AT+QGPS=1\r");
    return action_awaitResult(g_ltem1->dAction);
}



action_result_t gnss_off()
{
    action_invoke("AT+QGPSEND\r");
    return action_awaitResult(g_ltem1->dAction);
}



gnss_location_t gnss_getLocation()
{
    // result sz=86 >> +QGPSLOC: 121003.0,44.74769,-85.56535,1.1,189.0,2,95.45,0.0,0.0,250420,08
    gnss_location_t gnssResult;

    #define TOKEN_BUF_SZ 12
    char tokenBuf[TOKEN_BUF_SZ];
    char *continueAt;

    action_t *gnssCmd = action_build("AT+QGPSLOC=2", GNSS_CMD_RESULTBUF_SZ, 500, gnssLocCompleteParser);
    action_invokeCustom(gnssCmd);
    action_result_t cmdResult = action_awaitResult(gnssCmd);

    if (cmdResult != ACTION_RESULT_SUCCESS)
    {
        gnssResult.errcode = (uint16_t)cmdResult;
        return gnssResult;
    }

    continueAt = gnssCmd->resultHead + GNSS_LOC_DATAOFFSET;

    continueAt = strToken(continueAt, ASCII_cCOMMA, tokenBuf, TOKEN_BUF_SZ);
    if (continueAt != NULL)
        strncpy(gnssResult.utc, tokenBuf, TOKEN_BUF_SZ);
        
    gnssResult.lat.val = strtof(continueAt, &continueAt);
    gnssResult.lat.dir = ASCII_cSPACE;
    gnssResult.lon.val = strtof(continueAt++, &continueAt);
    gnssResult.lon.dir = ASCII_cSPACE;
    gnssResult.hdop = strtof(continueAt++, &continueAt);
    gnssResult.altitude = strtof(continueAt++, &continueAt);
    gnssResult.fixType = strtol(continueAt++, &continueAt, 10);
    gnssResult.course = strtof(continueAt++, &continueAt);
    gnssResult.speedkm = strtof(continueAt++, &continueAt);
    gnssResult.speedkn = strtof(continueAt++, &continueAt);

    continueAt = strToken(continueAt, ASCII_cCOMMA, tokenBuf, TOKEN_BUF_SZ);
    if (continueAt != NULL)
        strncpy(gnssResult.date, tokenBuf, TOKEN_BUF_SZ);

    gnssResult.nsat = strtol(continueAt, &continueAt, 10);

    action_destroy(gnssCmd);
    return gnssResult;
}



/* future geo-fence, likely separate fileset */
// void gnss_geoAdd();
// void gnss_geoDelete();
