/** ***************************************************************************
  @file 
  @brief Platform signaling (mutex/semaphore) abstraction declarations.

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


#ifndef __LTEMC_PLATFORM_LOCK_H__
#define __LTEMC_PLATFORM_LOCK_H__

#include <stdint.h>
#include <stdbool.h>

typedef enum mutexTableIndex_tag
{
    mutexTableIndex_cntxt0 = 0,
    mutexTableIndex_cntxt1 = 1,
    mutexTableIndex_cntxt2 = 2,
    mutexTableIndex_cntxt3 = 3,
    mutexTableIndex_cntxt4 = 4,
    mutexTableIndex_cntxt5 = 5,
    mutexTableIndex_files = 6,
    mutexTableIndex_ltem = 7,
    mutexTableIndex_atcmd = 8,
    mutexTableSz 
} mutexTableIndex_t;


#ifdef __cplusplus
extern "C" {
#endif


uint8_t pMutexCount(mutexTableIndex_t indx);

bool pMutexTake(mutexTableIndex_t indx, uint16_t timeout);

void pMutexGive(mutexTableIndex_t indx);


#ifdef __cplusplus
}
#endif
#endif  // !__LTEMC_PLATFORM_LOCK_H__
