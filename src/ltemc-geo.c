/******************************************************************************
 *  \file ltemc-geo.c
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
 * BGx Geo-Fence support (requires ltemc-gnss)
 *****************************************************************************/

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

#define SRCFILE "GEO"                           // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT
#include <stdlib.h>
#include "ltemc-internal.h"
#include "ltemc-geo.h"


// private local declarations
static resultCode_t geoQueryResponseParser(const char *response);


/* public functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions

/**
 *	@brief Create a geo-fence for future position evaluations.
 */
resultCode_t geo_add(uint8_t geoId, geoMode_t mode, geoShape_t shape, double lat1, double lon1, double lat2, double lon2, double lat3, double lon3, double lat4, double lon4)
{
    #define COORD_SZ 12
    #define COORD_P 6
    #define CMDSZ 32+8*COORD_SZ

    char cmdStr[CMDSZ] = {0};

    if (mode != geoMode_noUrc)                                 // currently only supporting mode 0 (no event reporting)
        return resultCode__badRequest;

    //void floatToString(float fVal, char *buf, uint8_t bufSz, uint8_t precision)
    snprintf(cmdStr, CMDSZ, "AT+QCFGEXT=\"addgeo\",%d,0,%d,%4.6f,%4.6f,%4.6f", geoId, shape, lat1, lon1, lat2);

    if (shape == geoShape_circlerad && (lon2 != 0 || lat3 != 0 || lon3 != 0 || lat4 != 0 || lon4 != 0) ||
        shape == geoShape_circlept &&  (lat3 != 0 || lon3 != 0 || lat4 != 0 || lon4 != 0) ||
        shape == geoShape_triangle &&  (lat4 != 0 || lon4 != 0))
    {
        return resultCode__badRequest;
    }

    if (shape >= geoShape_circlept)
    {
        char cmdChunk[COORD_SZ];
        snprintf(cmdChunk, COORD_SZ, ",%4.6f", lon2);
        strcat(cmdStr, cmdChunk);
    }
    if (shape >= geoShape_triangle)
    {
        char cmdChunk[2*COORD_SZ];
        snprintf(cmdChunk, 2*COORD_SZ, ",%4.6f,%4.6f", lat3, lon3);
        strcat(cmdStr, cmdChunk);
    }
    if (shape == geoShape_quadrangle)
    {
        char cmdChunk[2*COORD_SZ];
        snprintf(cmdChunk, 2*COORD_SZ, ",%4.6f,%4.6f", lat4, lon4);
        strcat(cmdStr, cmdChunk);
    }

    if (atcmd_tryInvokeDefaults(cmdStr))
    {
        return atcmd_awaitResult();
    }
    return resultCode__conflict;
}



/**
 *	@brief Delete a geo-fence for future position evaluations.
 */
resultCode_t geo_delete(uint8_t geoId)
{
    char cmdStr[28] = {0};
    snprintf(cmdStr, 80, "AT+QCFGEXT=\"deletegeo\",%d", geoId);
    if (atcmd_tryInvokeDefaults(cmdStr))
    {
        return atcmd_awaitResult();
    }
    return resultCode__conflict;
}



/**
 *	@brief Determine the current location relation to a geo-fence, aka are you inside or outside the fence.
 */
geoPosition_t geo_query(uint8_t geoId)
{
    if (atcmd_tryInvoke("AT+QCFGEXT=\"querygeo\",%d", geoId))
    {
        if (atcmd_awaitResult() != resultCode__success)
            return geoPosition_unknown;
        return resultCode__success;
    }
    return resultCode__conflict;
};



#pragma endregion

/* private (static) functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions

/**
 *	\brief Action response parser for a geo-fence query.
 */
static resultCode_t geoQueryResponseParser(const char *response)
{
    return serviceResponseParser(response, "+QCFGEXT: \"querygeo\",");
}


#pragma endregion
