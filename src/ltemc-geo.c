/** ****************************************************************************
  \file 
  \brief Public API geo-fence support (requires GNSS included in project)
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
    #include <jlinkRtt.h>                       // PRINTF debug macro output to J-Link RTT channel
    // #define PRINTF(c_,f_,__VA_ARGS__...) do { rtt_printf(c_, (f_), ## __VA_ARGS__); } while(0)
    #endif
#else
#define PRINTF(c_, f_, ...) 
#endif

#define SRCFILE "GEO"                           // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT
#include <stdio.h>
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
