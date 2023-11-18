/** ***************************************************************************
  @file 
  @brief Platform signaling abstraction for Arduino ESP32.

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


#ifdef ARDUINO_ARCH_ESP32

#include "platform-signal.h"

#include <stdint.h>
#include <FreeRTOS.h>
#include <task.h>
#include "freertos/semphr.h"

SemaphoreHandle_t _ltemCmd_mutexHandle;
static StaticSemaphore_t _ltemCmd_mutexStruct;

typedef struct esp32_mutex_tag
{
    SemaphoreHandle_t handle;
    StaticSemaphore_t mutex;
} esp32_mutex_t;

static SemaphoreHandle_t _mutexContainer(mutexTableIndex_t indx);



/* Public platform functions
 ----------------------------------------------------------------------------------------------- */

uint8_t pMutexCount(mutexTableIndex_t indx)
{
    return uxSemaphoreGetCount(_mutexContainer(indx));
}


bool pMutexTake(mutexTableIndex_t indx, uint16_t timeout)
{

    return xSemaphoreTake(_mutexContainer(indx), timeout);
}


void pMutexGive(mutexTableIndex_t indx)
{
    xSemaphoreGive(_mutexContainer(indx));
}


/* Static local functions
 ----------------------------------------------------------------------------------------------- */
static SemaphoreHandle_t _mutexContainer(mutexTableIndex_t indx)
{
    static bool initialized = false;
    static SemaphoreHandle_t _handles[mutexTableSz];
    static StaticSemaphore_t _mutexTable[mutexTableSz];

    if (!initialized)
    {
        for (size_t i = 0; i < mutexTableSz; i++)
        {
            _handles[indx] = xSemaphoreCreateMutexStatic(&_mutexTable[indx]);
        }
        initialized = true;
    }
    return _handles[indx];
}

#endif  // ARDUINO_ARCH_ESP32
