
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