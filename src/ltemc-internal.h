/******************************************************************************
 *  \file ltemc.h
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020-2022 LooUQ Incorporated.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED
 * "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
 * LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *****************************************************************************/

#ifndef __LTEMC_INTERNAL_H__
#define __LTEMC_INTERNAL_H__


// Internal static buffers you may need to change for your application. Contact LooUQ for details.
// #define IOP_RX_COREBUF_SZ 256
// #define IOP_TX_BUFFER_SZ 1460

#include "ltemc.h"
#include <lq-str.h>                         /// most LTEmC modules use LooUQ string functions

#include "ltemc-quectel-bg.h"
#include "ltemc-nxp-sc16is.h"
#include "ltemc-iop.h"


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


// LTEM Internal
void LTEM_initIo();
void LTEM_registerDoWorker(doWork_func *doWorker);


#pragma region ATCMD LTEmC Internal Functions
/* LTEmC internal, not intended for user application consumption.
 * --------------------------------------------------------------------------------------------- */

/**
 *	\brief Checks recv buffer for command response and sets atcmd structure data with result.
 */
resultCode_t ATCMD_readResult();

/**
 *  \brief Default AT command result parser.
*/
cmdParseRslt_t ATCMD_okResponseParser();

/**
 *  \brief Awaits exclusive access to QBG module command interface.
 *  \param timeoutMS [in] - Number of milliseconds to wait for a lock.
 *  @return true if lock aquired prior to the timeout period.
*/
bool ATCMD_awaitLock(uint16_t timeoutMS);

/**
 *	\brief Returns the current atCmd lock state
 *  \return True if atcmd lock is active (command underway)
 */
bool ATCMD_isLockActive();

/* LTEmC INTERNAL prompt parsers 
 * ------------------------------------------------------------------------- */

/**
 *	\brief Base parser looking for a simple prompt string.
 */
cmdParseRslt_t ATCMD_readyPromptParser(const char *rdyPrompt);

/**
 *	\brief Parser looking for BGx transmit data prompt "> "
 */
cmdParseRslt_t ATCMD_txDataPromptParser();

/**
 *	\brief Parser looking for BGx CONNECT data prompt.
 */
cmdParseRslt_t ATCMD_connectPromptParser();

#pragma endregion
/* ------------------------------------------------------------------------------------------------
 * End ATCMD LTEmC Internal Functions */


#pragma region NTWK LTEmC Internal Functions
/* LTEmC internal, not intended for user application consumption.
 * --------------------------------------------------------------------------------------------- */


/**
 *	\brief Initialize BGx Radio Access Technology (RAT) options.
 */
void NTWK_initRatOptions();


/**
 *	\brief Apply the default PDP context config to BGx.
 */
void NTWK_applyDefaulNetwork();


#pragma endregion
/* ------------------------------------------------------------------------------------------------
 * End NTWK LTEmC Internal Functions */


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__LTEMC_INTERNAL_H__ */