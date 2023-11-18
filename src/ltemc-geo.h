/** ***************************************************************************
  @file 
  @brief Modem geo-fence features/services (requires inclusion of GNSS module).

  @author Greg Terrell, LooUQ Incorporated

  \loouq

  @warning Internal dependencies, changes only as directed by LooUQ staff.

-------------------------------------------------------------------------------

LooUQ-LTEmC // Software driver for the LooUQ LTEm series cellular modems.
Copyright (C) 2017-2023 LooUQ Incorporated

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
Also add information on how to contact you by electronic and paper mail.

**************************************************************************** */


#ifndef __LTEMC_GEO_H__
#define __LTEMC_GEO_H__

#include <stdint.h>

/** 
 *  \brief Enum indicating the device's relationship to a geo-fence.
*/
typedef enum geoPosition_tag
{
    geoPosition_unknown = 0,       ///< The device's relationship to the geo-fence is not known.
    geoPosition_inside = 1,        ///< The device is inside the boundary of the geo-fence.
    geoPosition_outside = 2        ///< The device is located out the geo-fence boundary.
} geoPosition_t;

/** 
 *  \brief Structure containing a geo-fence inquiry result.
*/
typedef struct geoResult_tag
{
    uint8_t geoId;                  ///< The geo-fence the result applies to.
    geoPosition_t position;        ///< The relationship to the geo-fence boundary.
} geoResult_t;

/** 
 *  \brief Enum indicating geo-fence boundary events of interest.
*/
typedef enum geoMode_tag
{
    geoMode_noUrc = 0,             ///< Do not generate an URC (event) for geo-fence boundary crossing.
    geoMode_enterUrc = 1,          ///< Generate an URC (event) for the device entering the geo-fenced area.
    geoMode_exitUrc = 2,           ///< Generate an URC (event) for the device leaving the geo-fenced area.
    geoMode_bothUrc = 3            ///< Generate an URC (event) for the device entering or exiting the geo-fenced area.
} geoMode_t;


/** 
 *  \brief Enum indicating the shape of the geo-fence boundary.
*/
typedef enum geoShape_tag
{
    geoShape_circlerad = 0,        ///< The geo-fence is a circle and is described with a center point and a radius.
    geoShape_circlept = 1,         ///< The geo-fence is a circle and is described with a center point and a point on the perimeter.
    geoShape_triangle = 2,         ///< The geo-fence is a triangle and is described with the points of the 3 corners.
    geoShape_quadrangle = 3        ///< The geo-fence is a quadrangle and is described with the points of the 4 corners.
} geoShape_t;


#ifdef __cplusplus
extern "C" {
#endif


/**
 *	@brief Create a geo-fence for future position evaluations.
 */
resultCode_t geo_add(uint8_t geoId, geoMode_t mode, geoShape_t shape, double lat1, double lon1, double lat2, double lon2, double lat3, double lon3, double lat4, double lon4);


/**
 *	@brief Delete a geo-fence for future position evaluations.
 */
resultCode_t geo_delete(uint8_t geoId);


/**
 *	@brief Determine the current location relation to a geo-fence, aka are you inside or outside the fence.
 */
geoPosition_t geo_query(uint8_t geoId);


#ifdef __cplusplus
}
#endif

#endif  // !__LTEMC_GEO_H__
