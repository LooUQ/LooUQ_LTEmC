// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef _GEO_H_
#define _GEO_H_
#include <stdint.h>

/** 
 *  \brief Enum indicating the device's relationship to a geo-fence.
*/
typedef enum geo_position_tag
{
    geo_position_unknown = 0,       ///< The device's relationship to the geo-fence is not known.
    geo_position_inside = 1,        ///< The device is inside the boundary of the geo-fence.
    geo_position_outside = 2        ///< The device is located out the geo-fence boundary.
} geo_position_t;

/** 
 *  \brief Structure containing a geo-fence inquiry result.
*/
typedef struct geo_result_tag
{
    uint8_t geoId;                  ///< The geo-fence the result applies to.
    geo_position_t position;        ///< The relationship to the geo-fence boundary.
} geo_result_t;

/** 
 *  \brief Enum indicating geo-fence boundary events of interest.
*/
typedef enum
{
    geo_mode_noUrc = 0,             ///< Do not generate an URC (event) for geo-fence boundary crossing.
    geo_mode_enterUrc = 1,          ///< Generate an URC (event) for the device entering the geo-fenced area.
    geo_mode_exitUrc = 2,           ///< Generate an URC (event) for the device leaving the geo-fenced area.
    geo_mode_bothUrc = 3            ///< Generate an URC (event) for the device entering or exiting the geo-fenced area.
} geo_mode_t;


/** 
 *  \brief Enum indicating the shape of the geo-fence boundary.
*/
typedef enum
{
    geo_shape_circlerad = 0,        ///< The geo-fence is a circle and is described with a center point and a radius.
    geo_shape_circlept = 1,         ///< The geo-fence is a circle and is described with a center point and a point on the perimeter.
    geo_shape_triangle = 2,         ///< The geo-fence is a triangle and is described with the points of the 3 corners.
    geo_shape_quadrangle = 3        ///< The geo-fence is a quadrangle and is described with the points of the 4 corners.
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
