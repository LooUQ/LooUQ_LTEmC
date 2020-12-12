// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.


#include "ltem1c.h"


#ifndef _MODEMINFO_H_
#define _MODEMINFO_H_

// #include <stddef.h>
// #include <stdint.h>

#define MDMINFO_IMEI_SZ = 16
#define MDMINFO_ICCID_SZ = 21
#define MDMINFO_FWVER_SZ = 41
#define MDMINFO_MFGINFO_SZ 41

/** 
 *  \brief Struct holding information about the physical BGx module.
*/
typedef struct modemInfo_tag
{
	char imei[16];          ///< IMEI (15 digits) International Mobile Equipment Identity, set in the BGx itself at manufacture.
	char iccid [21];        ///< ICCID (20 digits) Integrated Circuit Card ID. Set in the SIM card at manufacture.
	char mfgmodel [21];     ///< The Quectel model number of the BGx device.
	char fwver [41];        ///< Firmware version of the BGx device.
} modemInfo_t;


#ifdef __cplusplus
extern "C" {
#endif


modemInfo_t mdminfo_ltem1();
int16_t mdminfo_rssi();
uint8_t mdminfo_rssiBars(uint8_t numberOfBars);


#ifdef __cplusplus
}
#endif

#endif  //!_MODEMINFO_H_
