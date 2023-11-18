/** ***************************************************************************
  @file 
  @brief Platform signaling abstraction for Arduino SAMD (Cortex+).

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


#ifdef ARDUINO_ARCH_SAMD

#include "platform-signal.h"

#include <stdint.h>


static uint8_t _mutexContainer(mutexTableIndex_t indx, int newValue);

#define SAMD_SIGNAL_TAKE (-1)
#define SAMD_SIGNAL_QUERY (0)
#define SAMD_SIGNAL_GIVE (1)

/* Public platform functions
 ----------------------------------------------------------------------------------------------- */

uint8_t pMutexCount(mutexTableIndex_t indx)
{
    return _mutexContainer(indx, SAMD_SIGNAL_QUERY);
}


bool pMutexTake(mutexTableIndex_t indx, uint16_t timeout)           // SAMD is single-threaded, no alternate task to release mutex; API parameter timeout is ignored
{
    uint8_t prevState = _mutexContainer(indx, SAMD_SIGNAL_QUERY);
    return _mutexContainer(indx, SAMD_SIGNAL_TAKE) != prevState;    // true = mutex changed (take successful)
}


void pMutexGive(mutexTableIndex_t indx)
{
    _mutexContainer(indx, SAMD_SIGNAL_GIVE);
}


/* Static local functions
 ----------------------------------------------------------------------------------------------- */
static uint8_t _mutexContainer(mutexTableIndex_t indx, int newVal)
{
    //static bool initialized = false;
    // static SemaphoreHandle_t _handles[mutexTableSz];
    // static StaticSemaphore_t _mutexTable[mutexTableSz];


    static uint8_t _mutexTable[mutexTableSz] = {1, 1, 1, 1, 1, 1, 1, 1, 1};

    if (newVal > 0 && _mutexTable[indx] < 1)
        _mutexTable[indx]++;

    else if (newVal < 0 && _mutexTable[indx] > 0)
        _mutexTable[indx]--;

    return _mutexTable[indx];
}

#endif  // ARDUINO_ARCH_SAMD