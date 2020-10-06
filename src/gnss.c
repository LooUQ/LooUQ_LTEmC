// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "ltem1c.h"

#define GNSS_CMD_RESULTBUF_SZ 90
#define GNSS_LOC_DATAOFFSET 12
#define GNSS_LOC_EXPECTED_TOKENCOUNT 11

#define MIN(x, y) (((x) < (y)) ? (x) : (y))


// private local declarations
static resultCode_t gnssLocCompleteParser(const char *response, char **endptr);


/*
 *  AT+QGPSLOC=2 (format=2)
 *  +QGPSLOC: 113355.0,44.74770,-85.56527,1.2,192.0,2,277.11,0.0,0.0,250420,10
 * --------------------------------------------------------------------------------------------- */


/* public functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions


resultCode_t gnss_on()
{
    if (action_tryInvokeAdv("AT+QGPS=1", ACTION_RETRIES_DEFAULT, 800, NULL))
    {
        return action_awaitResult(true).statusCode;
    }
    return RESULT_CODE_CONFLICT;
}



resultCode_t gnss_off()
{
    if (action_tryInvokeAdv("AT+QGPSEND", ACTION_RETRIES_DEFAULT, 800, NULL))
    {
        return action_awaitResult(true).statusCode;
    }
    return RESULT_CODE_CONFLICT;
}



gnssLocation_t gnss_getLocation()
{
    // result sz=86 >> +QGPSLOC: 121003.0,44.74769,-85.56535,1.1,189.0,2,95.45,0.0,0.0,250420,08  + lineEnds and OK

    #define TOKEN_BUF_SZ 12

    char tokenBuf[TOKEN_BUF_SZ] = {0};
    char *continueAt;
    gnssLocation_t gnssResult;

    //action_t *gnssCmd = action_build("AT+QGPSLOC=2", GNSS_CMD_RESULTBUF_SZ, 500, gnssLocCompleteParser);

    if (action_tryInvokeAdv("AT+QGPSLOC=2", ACTION_RETRIES_DEFAULT, ACTION_TIMEOUT_DEFAULTmillis, gnssLocCompleteParser))
    {
        actionResult_t atResult = action_awaitResult(false);

        gnssResult.statusCode = atResult.statusCode;
        if (atResult.statusCode != RESULT_CODE_SUCCESS)
        {
            action_close();
            return gnssResult;
        }
        continueAt = atResult.response + GNSS_LOC_DATAOFFSET;
        continueAt = strToken(continueAt, ASCII_cCOMMA, tokenBuf, TOKEN_BUF_SZ);
        if (continueAt != NULL)
            strncpy(gnssResult.utc, tokenBuf, 11);
        gnssResult.lat.val = strtof(continueAt, &continueAt);
        gnssResult.lat.dir = ASCII_cSPACE;
        gnssResult.lon.val = strtof(++continueAt, &continueAt);
        gnssResult.lon.dir = ASCII_cSPACE;
        gnssResult.hdop = strtof(++continueAt, &continueAt);
        gnssResult.altitude = strtof(++continueAt, &continueAt);
        gnssResult.fixType = strtol(++continueAt, &continueAt, 10);
        gnssResult.course = strtof(++continueAt, &continueAt);
        gnssResult.speedkm = strtof(++continueAt, &continueAt);
        gnssResult.speedkn = strtof(++continueAt, &continueAt);
        continueAt = strToken(continueAt + 1, ASCII_cCOMMA, tokenBuf, TOKEN_BUF_SZ);
        if (continueAt != NULL)
            strncpy(gnssResult.date, tokenBuf, 7);
        gnssResult.nsat = strtol(continueAt, &continueAt, 10);
        action_close();
    }

    return gnssResult;
}


#pragma endregion

/* private (static) functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions


static resultCode_t gnssLocCompleteParser(const char *response, char **endptr)
{
    //const char *response, const char *landmark, char delim, uint8_t minTokens, const char *terminator, char** endptr
    return action_tokenResultParser(response, "+QGPSLOC:", ASCII_cCOMMA, GNSS_LOC_EXPECTED_TOKENCOUNT, ASCII_sOK, endptr);
}


#pragma endregion
