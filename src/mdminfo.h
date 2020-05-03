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


#ifdef __cplusplus
extern "C" {
#endif


struct ltem1_modemInfo_tag mdminfo_ltem1();

int16_t mdminfo_rssi();


#ifdef __cplusplus
}
#endif

#endif  //!_MODEMINFO_H_
