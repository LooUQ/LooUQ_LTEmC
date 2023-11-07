
#ifdef ARDUINO_ARCH_SAMD

#include "platform-signal.h"

#include <stdint.h>
#include <FreeRTOS.h>
#include <task.h>
#include "freertos/semphr.h"


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
    return _mutexContainer(indx, SAMD_SIGNAL_TAKE);
}


void pMutexGive(mutexTableIndex_t indx)
{
    return _mutexContainer(indx, SAMD_SIGNAL_GIVE);
}


/* Static local functions
 ----------------------------------------------------------------------------------------------- */
static uint8_t _mutexContainer(mutexTableIndex_t indx, int newValue)
{
    //static bool initialized = false;
    // static SemaphoreHandle_t _handles[mutexTableSz];
    // static StaticSemaphore_t _mutexTable[mutexTableSz];


    static uint8_t _mutexTable[mutexTableSz] = {0};

    if (newValue > 0 && _mutexTable[indx] < 1)
        _mutexTable[indx]++;
    else if (newValue < 0 && _mutexTable[indx] > 0)
        _mutexTable[indx]--;


    // if (!initialized)
    // {
    //     for (size_t i = 0; i < mutexTableSz; i++)
    //     {
    //         _handles[indx] = xSemaphoreCreateMutexStatic(&_mutexTable[indx]);
    //     }
    //     initialized = true;
    // }

    return _mutexTable[indx];
}

#endif  // ARDUINO_ARCH_SAMD