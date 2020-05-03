// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "ltem1c.h"

#define GNSS_CMD_RESULTBUF_SZ 90
#define GNSS_LOC_DATAOFFSET 11
#define GNSS_LOC_EXPECTED_TOKENCOUNT 11

/*
AT+QGPSLOC=2

+QGPSLOC: 113355.0,44.74770,-85.56527,1.2,192.0,2,277.11,0.0,0.0,250420,10

OK
*/

static uint16_t gnssLocCompleteParser(const char *response)
{
    return atcmd_tokenCompletedHelper(response, "+QGPSLOC:", ASCII_cCOMMA, GNSS_LOC_EXPECTED_TOKENCOUNT);
}


/* --------------------------------------------------------------------------------------------- */


atcmd_result_t gnss_on()
{
    atcmd_invoke("AT+QGPS=1\r");
    return atcmd_awaitResult(g_ltem1->atcmd);
}



atcmd_result_t gnss_off()
{
    atcmd_invoke("AT+QGPSEND\r");
    return atcmd_awaitResult(g_ltem1->atcmd);
}



gnss_location_t gnss_getLocation()
{
    gnss_location_t gnssResult;
    // +QGPSLOC: 121003.0,44.74769,-85.56535,1.1,189.0,2,95.45,0.0,0.0,250420,08

    char cmdResultBuf[GNSS_CMD_RESULTBUF_SZ + 1] = {0};
    char *tokPtr;

    atcmd_t *gnssCmd = atcmd_build("AT+QGPSLOC=2\r", cmdResultBuf, GNSS_CMD_RESULTBUF_SZ, 500, gnssLocCompleteParser);
    atcmd_invokeAdv(gnssCmd);
    atcmd_result_t cmdResult = atcmd_awaitResult(gnssCmd);

    if (cmdResult != ATCMD_RESULT_SUCCESS)
    {
        gnssResult.errcode = (uint16_t)cmdResult;
        return gnssResult;
    }

    tokPtr = cmdResult + GNSS_LOC_DATAOFFSET;

    *gnssResult.utc = strtok_r(tokPtr, ASCII_cCOMMA, &tokPtr);
    gnssResult.lat.val = strtof(tokPtr, &tokPtr);
    gnssResult.lon.val = strtof(tokPtr + 1, &tokPtr);
    gnssResult.hdop = strtof(tokPtr + 1, &tokPtr);
    gnssResult.altitude = strtof(tokPtr + 1, &tokPtr);
    gnssResult.fixType = strtol(tokPtr + 1, &tokPtr, 10);
    gnssResult.course = strtof(tokPtr + 1, &tokPtr);
    gnssResult.speedkm = strtof(tokPtr + 1, &tokPtr);
    gnssResult.speedkn = strtof(tokPtr + 1, &tokPtr);
    *gnssResult.date = strtok_r(tokPtr, ASCII_cCOMMA, &tokPtr);
    gnssResult.nsat = strtol(tokPtr, &tokPtr, 10);

    return gnssResult;
}



/* future geo-fence */
// void gnss_geoAdd();
// void gnss_geoDelete();
