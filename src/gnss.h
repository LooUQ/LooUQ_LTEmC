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


typedef struct gnss_latlon_tag
{
    float val;
    char dir;
} gnss_latlon_t;


typedef struct gnssLocation_tag
{
    char utc[11];
    gnss_latlon_t lat;
    gnss_latlon_t lon;
    float hdop;
    float altitude;
    uint16_t fixType;
    float course;
    float speedkm;
    float speedkn;
    char date[7];
    uint16_t nsat;
    uint16_t statusCode;
} gnssLocation_t;


#ifdef __cplusplus
extern "C" {
#endif

resultCode_t gnss_on();          // AT+QGPS=1
resultCode_t gnss_off();

gnssLocation_t gnss_getLocation();

// future geo-fence
void gnss_geoAdd();
void gnss_geoDelete();

/* future geo-fence, likely separate .h/.c fileset */
// void gnss_geoAdd();
// void gnss_geoDelete();


#ifdef __cplusplus
}
#endif

#endif  //!_GNSS_H_
