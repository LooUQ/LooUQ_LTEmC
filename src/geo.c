// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "ltem1c.h"
#include "gnss.h"
#include "geo.h"

// private local declarations
static actionResult_t geoQueryResponseParser(const char *response);


/* public functions
 * --------------------------------------------------------------------------------------------- */
#pragma region public functions


actionResult_t geo_add(uint8_t geoId, geo_mode_t mode, geo_shape_t shape, double lat1, double lon1, double lat2, double lon2, double lat3, double lon3, double lat4, double lon4)
{
    #define CMDSZ 32+8*12
    #define COORD_PRECISION 6
    #define COORD_OFFSET 11

    char response[ACTION_DEFAULT_RESPONSE_SZ] = {0};
    char cmdStr[CMDSZ] = {0};

    if (mode != geo_mode_noUrc)                                 // currently only supporting mode 0 (no event reporting)
        return ACTION_RESULT_BADREQUEST;

    //void floatToString(float fVal, char *buf, uint8_t bufSz, uint8_t precision)
    snprintf(cmdStr, 40, "AT+QCFGEXT=\"addgeo\",%d,0,%d", geoId, shape);

    if (shape == geo_shape_circlerad && (lon2 != 0 || lat3 != 0 || lon3 != 0 || lat4 != 0 || lon4 != 0) ||
        shape == geo_shape_circlept &&  (lat3 != 0 || lon3 != 0 || lat4 != 0 || lon4 != 0) ||
        shape == geo_shape_triangle &&  (lat4 != 0 || lon4 != 0))
    {
        return ACTION_RESULT_BADREQUEST;
    }

    floatToString(lat1, cmdStr, CMDSZ, COORD_PRECISION);
    strcat(cmdStr, ",");
    floatToString(lon1, cmdStr + COORD_OFFSET, CMDSZ, COORD_PRECISION);
    strcat(cmdStr, ",");
    floatToString(lat2, cmdStr + COORD_OFFSET * 2, CMDSZ, COORD_PRECISION);

    if (shape >= geo_shape_circlept)
    {
        strcat(cmdStr, ",");
        floatToString(lon2, cmdStr, CMDSZ, COORD_PRECISION);
    }
    if (shape >= geo_shape_triangle)
    {
        strcat(cmdStr, ",");
        floatToString(lat3, cmdStr, CMDSZ, COORD_PRECISION);
        strcat(cmdStr, ",");
        floatToString(lon3, cmdStr, CMDSZ, COORD_PRECISION);
    }
    if (shape == geo_shape_quadrangle)
    {
        strcat(cmdStr, ",");
        floatToString(lat4, cmdStr, CMDSZ, COORD_PRECISION);
        strcat(cmdStr, ",");
        floatToString(lat4, cmdStr, CMDSZ, COORD_PRECISION);
    }

    action_tryInvoke(cmdStr, true);
    return action_awaitResult(response, ACTION_DEFAULT_RESPONSE_SZ, 0, NULL);
}



actionResult_t geo_delete(uint8_t geoId)
{
    char response[ACTION_DEFAULT_RESPONSE_SZ] = {0};
    char cmdStr[28] = {0};
    snprintf(cmdStr, 80, "AT+QCFGEXT=\"deletegeo\",%d", geoId);
    action_tryInvoke(cmdStr, true);
    return action_awaitResult(response, ACTION_DEFAULT_RESPONSE_SZ, 0, NULL);
}



geo_position_t geo_query(uint8_t geoId)
{
    actionResult_t actionResult;

    char response[ACTION_DEFAULT_RESPONSE_SZ] = {0};
    char cmdStr[28] = {0};
    snprintf(cmdStr, 80, "AT+QCFGEXT=\"querygeo\",%d", geoId);
    action_tryInvoke(cmdStr, true);
    actionResult = action_awaitResult(response, ACTION_DEFAULT_RESPONSE_SZ, 0, NULL);

    if (actionResult == ACTION_RESULT_SUCCESS)
        return geo_position_unknown;
    return actionResult;
};



#pragma endregion

/* private (static) functions
 * --------------------------------------------------------------------------------------------- */
#pragma region private functions


static actionResult_t geoQueryResponseParser(const char *response)
{
    return serviceResponseParser(response, "+QCFGEXT: \"querygeo\",");
}


#pragma endregion
