/** ***************************************************************************
  @file 
  @brief LTEm timing abstraction declarations.

  @author Greg Terrell/Jensen Miller, LooUQ Incorporated

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


#ifndef __PLATFORM_TIMING_H__
#define __PLATFORM_TIMING_H__



// yield callback allows host application to be signalled when the LTEm1 is awaiting network events

typedef void (*platform_yieldCB_func)();

// extern platform_yieldCB_func platform_yieldCB_func;


#ifdef __cplusplus
extern "C"
{
#include <cstdint>
#else
#include <stdint.h>
#endif // __cplusplus

/**
 * @brief Get millisecond timer current state.
 * @details LTEmC is designed for portability by minimizing hardware interactions and utilizing common framework facilities. It uses
 * a milliseconds timer count for all timing functions.
 * 
 * @return uint32_t The current "tick" count of the system
 */
uint32_t pMillis();


/**
 * @brief Invoked by LTEmC long-running functions to allow for host processing while waiting for a LTEmC result.
 */
void pYield();


/**
 * @brief LTEmC sparingly uses a platform implementation of delay or it can be implemented in the timing abstraction.
 * 
 * @param delay_ms Number of milliseconds to pause program flow
 */
void pDelay(uint32_t delay_ms);


/**
 * @brief Simple get timespan function based on millisecond timer (counter)
 * @param timerStart 
 * @param timerTimeout 
 * @return True indicates that the timeout has occured, false the period has NOT elapsed.
 */
bool pElapsed(uint32_t timerStart, uint32_t timerTimeout);


#ifdef __cplusplus
}
#endif // __cplusplus



#endif  /* !__PLATFORM_TIMING_H__ */