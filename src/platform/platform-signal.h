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
