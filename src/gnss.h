// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef _GNSS_H_
#define _GNSS_H_

typedef enum
{
    gnss_format_dms = 0,
    gnss_format_dmsPrecise = 1,
    gnss_format_dcmDegrees = 2
} gnss_format_t;

typedef enum
{
    gnss_geo_position_unknown = 0,
    gnss_geo_position_inside = 1,
    gnss_geo_position_outside = 2
} gnss_geo_position_t;


typedef struct gnss_latlon_tag
{
    float val;
    char dir;
} gnss_latlon_t;

typedef struct gnss_geo_result_tag
{
    uint8_t geoId;
    gnss_geo_position_t position;
};


typedef struct gnss_location_tag
{
    char utc[10];
    gnss_latlon_t lat;
    gnss_latlon_t lon;
    float hdop;
    float altitude;
    uint16_t fixType;
    float course;
    float speedkm;
    float speedkn;
    char date[6];
    uint16_t nsat;
    uint16_t errcode;
} gnss_location_t;


#ifdef __cplusplus
extern "C" {
#endif

action_result_t gnss_on();          // AT+QGPS=1
action_result_t gnss_off();

gnss_location_t gnss_getLocation();

// future geo-fence
void gnss_geoAdd();
void gnss_geoDelete();



#ifdef __cplusplus
}
#endif

#endif  //!_GNSS_H_
