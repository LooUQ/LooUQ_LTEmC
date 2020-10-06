// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef _GEO_H_
#define _GEO_H_
#include <stdint.h>

typedef enum geo_position_tag
{
    geo_position_unknown = 0,
    geo_position_inside = 1,
    geo_position_outside = 2
} geo_position_t;


typedef struct geo_result_tag
{
    uint8_t geoId;
    geo_position_t position;
} geo_result_t;


typedef enum
{
    geo_mode_noUrc = 0,
    geo_mode_enterUrc = 1,
    geo_mode_exitUrc = 2,
    geo_mode_bothUrc = 3

} geo_mode_t;


typedef enum
{
    geo_shape_circlerad = 0,
    geo_shape_circlept = 1,
    geo_shape_triangle = 2,
    geo_shape_quadrangle = 3
} geo_shape_t;


#ifdef __cplusplus
extern "C" {
#endif


resultCode_t geo_add(uint8_t geoId, geo_mode_t mode, geo_shape_t shape, double lat1, double lon1, double lat2, double lon2, double lat3, double lon3, double lat4, double lon4);
resultCode_t geo_delete(uint8_t geoId);

geo_position_t geo_query(uint8_t geoId);


#ifdef __cplusplus
}
#endif

#endif  //!_GEO_H_
