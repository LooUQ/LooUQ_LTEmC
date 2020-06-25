// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "ltem1c.h"

#define GNSS_CMD_RESULTBUF_SZ 90
#define GNSS_LOC_DATAOFFSET 12
#define GNSS_LOC_EXPECTED_TOKENCOUNT 11

#define MIN(x, y) (((x) < (y)) ? (x) : (y))


// private local declarations
static actionResult_t gnssLocCompleteParser(const char *response);


/*
 *  AT+QGPSLOC=2 (format=2)
 *  +QGPSLOC: 113355.0,44.74770,-85.56527,1.2,192.0,2,277.11,0.0,0.0,250420,10
 * --------------------------------------------------------------------------------------------- */


/* public functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions


actionResult_t gnss_on()
{
    char response[ACTION_DEFAULT_RESPONSE_SZ] = {0};
    action_tryInvoke("AT+QGPS=1", true);
    return action_awaitResult(response, ACTION_DEFAULT_RESPONSE_SZ, 800, NULL);
}



actionResult_t gnss_off()
{
    char response[ACTION_DEFAULT_RESPONSE_SZ] = {0};
    action_tryInvoke("AT+QGPSEND", true);
    return action_awaitResult(response, ACTION_DEFAULT_RESPONSE_SZ, 800, NULL);
}



gnssLocation_t gnss_getLocation()
{
    // result sz=86 >> +QGPSLOC: 121003.0,44.74769,-85.56535,1.1,189.0,2,95.45,0.0,0.0,250420,08  + lineEnds and OK

    #define TOKEN_BUF_SZ 12

    char response[GNSS_CMD_RESULTBUF_SZ] = {0};
    char tokenBuf[TOKEN_BUF_SZ] = {0};
    char *continueAt;
    gnssLocation_t gnssResult;

    //action_t *gnssCmd = action_build("AT+QGPSLOC=2", GNSS_CMD_RESULTBUF_SZ, 500, gnssLocCompleteParser);

    action_tryInvoke("AT+QGPSLOC=2", true);
    actionResult_t cmdResult = action_awaitResult(response, GNSS_CMD_RESULTBUF_SZ, 0, gnssLocCompleteParser);

    gnssResult.statusCode = (uint16_t)cmdResult;
    if (cmdResult != ACTION_RESULT_SUCCESS)
    {
        return gnssResult;
    }

    continueAt = response + GNSS_LOC_DATAOFFSET;
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

    return gnssResult;
}


#pragma endregion

/* private (static) functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions


static actionResult_t gnssLocCompleteParser(const char *response)
{
    actionResult_t result = action_tokenResultParser(response, "+QGPSLOC:", ASCII_cCOMMA, GNSS_LOC_EXPECTED_TOKENCOUNT);

    // if (result != ACTION_RESULT_PENDING)
    //     return result;
    // return action_cmeResultParser(response);
}


#pragma endregion
