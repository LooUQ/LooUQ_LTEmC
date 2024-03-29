/** ****************************************************************************
  \file 
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


#ifndef __LTEMC_GNSS_H__
#define __LTEMC_GNSS_H__


/** 
 *  \brief Enum describing the output format for location data.
*/
typedef enum
{
    gnss_format_dms = 0,            ///< Output as degrees, minutes; default resolution. Format: ddmm.mmmm N/S,dddmm.mmmm E/W
    gnss_format_dmsPrecise = 1,     ///< Output as degrees, minutes; precise. Format: ddmm.mmmmmm N/S,dddmm.mmmmmm E/W
    gnss_format_dcmDegrees = 2      ///< Output as decimal latitude, longitude. Format: (-)dd.ddddd,(-)ddd.ddddd
} gnss_format_t;


/** 
 *  \brief Struct containing both the location value (latitude or longitude) and a char indicating direction (char only for DMS formats).
*/
typedef struct gnss_latlon_tag
{
    double val;                     ///< The decimal number indicating the value for the lat/lon
    char dir;                       ///< Char indicating direction, values are N/S (lat) or E/W (lon). Optional based on format.
} gnss_latlon_t;


/** 
 *  \brief Struct containing a GNSS location fix.
*/
typedef struct gnssLocation_tag
{
    char utc[11];           ///< Universal time value when fixing position.
    gnss_latlon_t lat;      ///< Latitude value as a struct gnss_latlon_tag (Quoted from GPGGA sentence).
    gnss_latlon_t lon;      ///< Longitude value as a struct gnss_latlon_tag (Quoted from GPGGA sentence).
    double hdop;            ///< Horizontal precision: 0.5-99.9 (Quoted from GPGGA sentence).
    double altitude;        ///< The altitude of the antenna away from the sea level (unit: m), accurate to one decimal place (Quoted from GPGGA sentence).
    uint16_t fixType;       ///< GNSS positioning mode (Quoted from GNGSA/GPGSA sentence). Values: 2=2D positioning. 3=3D positioning
    double course;          ///< Course Over Ground based on true north. Format: ddd.mm (Quoted from GPVTG sentence). Values: ddd=000-359 (degree), mm 00-59 (minute).
    double speedkm;         ///< Speed over ground (metric). Format: xxxx.x; unit: Km/h; accurate to one decimal place (Quoted from GPVTG sentence).
    double speedkn;         ///< Speed over ground (nautical). Format: xxxx.x; unit: Knots/h; accurate to one decimal place (Quoted from GPVTG sentence).
    char date[7];           ///< UTC time when fixing position. Format: ddmmyy (Quoted from GPRMC sentence).
    uint16_t nsat;          ///< Number of satellites, from 00 (The first 0 should be retained) to 12 (Quoted from GPGGA sentence).
    uint16_t statusCode;    ///< Result code indicating get location status. 200 = success, otherwise error condition.
} gnssLocation_t;


#ifdef __cplusplus
extern "C" {
#endif

/**
 *	@brief Turn GNSS/GPS subsystem on. 
 *
 *  @return Result code representing status of operation, OK = 200.
 */
resultCode_t gnss_on();          // AT+QGPS=1

/**
 *	@brief Turn GNSS/GPS subsystem off. 
 *  @return Result code representing status of operation, OK = 200.
 */
resultCode_t gnss_off();

/**
 *	@brief Query BGx for current location/positioning information. 
 *  @return GNSS location struct, see gnss.h for details.
 */
gnssLocation_t gnss_getLocation();


#ifdef __cplusplus
}
#endif

#endif  // !__LTEMC_GNSS_H__
