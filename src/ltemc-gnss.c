/******************************************************************************
 *  \file ltemc-gnss.c
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020,2021 LooUQ Incorporated.
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
 * GNSS\GPS support for BGx family (geo-fence is a separate optional module)
 *****************************************************************************/

#define _DEBUG 0                        // set to non-zero value for PRINTF debugging output, 
// debugging output options             // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if defined(_DEBUG) && _DEBUG > 0
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG == 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#endif


#include "ltemc.h"
#include "ltemc-gnss.h"

#define GNSS_CMD_RESULTBUF_SZ 90
#define GNSS_LOC_DATAOFFSET 12
#define GNSS_LOC_EXPECTED_TOKENCOUNT 11
#define GNSS_TIMEOUTml 800

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


/**
 *	\brief Turn GNSS\GPS subsystem on. 
 *
 *  \return Result code representing status of operation, OK = 200.
 */
resultCode_t gnss_on()
{
    if (atcmd_tryInvokeAdv("AT+QGPS=1", GNSS_TIMEOUTml, NULL))
    {
        return atcmd_awaitResult(true).statusCode;
    }
    return RESULT_CODE_CONFLICT;
}



/**
 *	\brief Turn GNSS\GPS subsystem off. 
 *
 *  \return Result code representing status of operation, OK = 200.
 */
resultCode_t gnss_off()
{
    if (atcmd_tryInvokeAdv("AT+QGPSEND", GNSS_TIMEOUTml, NULL))
    {
        return atcmd_awaitResult(true).statusCode;
    }
    return RESULT_CODE_CONFLICT;
}


/**
 *	\brief Query BGx for current location\positioning information. 
 *
 *  \return GNSS location struct, see gnss.h for details.
 */
gnssLocation_t gnss_getLocation()
{
    #define TOKEN_BUF_SZ 12

    char tokenBuf[TOKEN_BUF_SZ] = {0};
    char *continueAt;
    gnssLocation_t gnssResult;

    //atcmd_t *gnssCmd = atcmd_build("AT+QGPSLOC=2", GNSS_CMD_RESULTBUF_SZ, 500, gnssLocCompleteParser);
    
    // result sz=86 >> +QGPSLOC: 121003.0,44.74769,-85.56535,1.1,189.0,2,95.45,0.0,0.0,250420,08  + lineEnds and OK

    if (atcmd_tryInvokeAdv("AT+QGPSLOC=2", ACTION_TIMEOUTml, gnssLocCompleteParser))
    {
        atcmdResult_t atResult = atcmd_awaitResult(false);

        gnssResult.statusCode = atResult.statusCode;
        if (atResult.statusCode != RESULT_CODE_SUCCESS)
        {
            atcmd_close();
            return gnssResult;
        }

        PRINTF(DBGCOLOR_warn, "getLocation(): parse starting...\r");

        continueAt = atResult.response + GNSS_LOC_DATAOFFSET;                           // skip past +QGPSLOC: 
        continueAt = atcmd_strToken(continueAt, ASCII_cCOMMA, tokenBuf, TOKEN_BUF_SZ);        // grab 1st element as a string
        if (continueAt != NULL)
            strncpy(gnssResult.utc, tokenBuf, 11);
        gnssResult.lat.val = strtof(continueAt, &continueAt);                           // grab a float
        gnssResult.lat.dir = ASCII_cSPACE;
        gnssResult.lon.val = strtof(++continueAt, &continueAt);                         // ++continueAt, pre-incr to skip previous comma
        gnssResult.lon.dir = ASCII_cSPACE;
        gnssResult.hdop = strtof(++continueAt, &continueAt);
        gnssResult.altitude = strtof(++continueAt, &continueAt);
        gnssResult.fixType = strtol(++continueAt, &continueAt, 10);                     // grab an integer
        gnssResult.course = strtof(++continueAt, &continueAt);
        gnssResult.speedkm = strtof(++continueAt, &continueAt);
        gnssResult.speedkn = strtof(++continueAt, &continueAt);
        continueAt = atcmd_strToken(continueAt + 1, ASCII_cCOMMA, tokenBuf, TOKEN_BUF_SZ);
        if (continueAt != NULL)
            strncpy(gnssResult.date, tokenBuf, 7);
        gnssResult.nsat = strtol(continueAt, &continueAt, 10);
        atcmd_close();
    }
    PRINTF(DBGCOLOR_warn, "getLocation(): parse completed\r");


    return gnssResult;
}


#pragma endregion

/* private (static) functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions


/**
 *	\brief Action response parser for GNSS location request. 
 */
static resultCode_t gnssLocCompleteParser(const char *response, char **endptr)
{
    //const char *response, const char *landmark, char delim, uint8_t minTokens, const char *terminator, char** endptr
    resultCode_t result = atcmd_tokenResultParser(response, "+QGPSLOC:", ASCII_cCOMMA, GNSS_LOC_EXPECTED_TOKENCOUNT, ASCII_sOK, endptr);

    PRINTF(0, "gnssParser(): result=%i\r", result);
    return result;
}

#pragma endregion
