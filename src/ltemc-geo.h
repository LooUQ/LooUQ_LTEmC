/******************************************************************************
 *  \file ltemc-geo.h
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

#ifndef __LTEMC_GEO_H__
#define __LTEMC_GEO_H__

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

#endif  // !__LTEMC_GEO_H__
