/** ****************************************************************************
  \file 
  \author Greg Terrell, LooUQ Incorporated

  \loouq

--------------------------------------------------------------------------------

    This project is released under the GPL-3.0 License.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 
***************************************************************************** */


#ifndef __LTEMC_IOP_H__
#define __LTEMC_IOP_H__

// #include "ltemc-types.h"
// #include "ltemc-nxp-sc16is.h"
#include <stdint.h>

#define USR_PG() (!pRxBffr->iopPg)
#define IOP_PG() (pRxBffr->iopPg)


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


/**
 *	@brief Initialize the Input/Output Process subsystem.
 */
void IOP_create();


/**
 *	@brief Complete initialization and start running IOP processes.
 */
void IOP_attachIrq();


/**
 *	@brief Stop IOP services.
 */
void IOP_detachIrq();


/**
 *	@brief Verify LTEm firmware has started and is ready for driver operations.
 */
bool IOP_awaitAppReady();


/**
 *	@brief Perform a TX send operation. 
    @details This call will block until TX is fully sent out to UART. The BGx module is synchronous, so no buffering is implemented (may change).
 *  @param sendData [in] Pointer to char data to send out, input buffer can be discarded following call.
 *  @param sendSz [in] The number of characters to send.
 */
void IOP_startTx(const char *sendData, uint16_t sendSz);


/**
 *	@brief Perform a forced TX send immediate operation. Intended for sending break type events to device.
 *  @details sendData must be less than 64 chars. This function aborts any TX and immediately posts data to UART.
 *  @param sendData [in] Pointer to char data to send out, input buffer can be discarded following call.
 *  @param sendSz [in] The number of characters to send.
 */
void IOP_forceTx(const char *sendData, uint16_t sendSz);


/**
 *	@brief Check for RX progress/idle.
 *
 *  @return Duration since last fill level change detected; returns 0 if a change is detected since last call
 */
uint32_t IOP_getRxIdleDuration();


// /**
//  *	@brief Finds a string in the last X characters of the IOP RX stream.
//  *  @param pBuf [in] Pointer data receive buffer.
//  *  @param rewindCnt [in] The number of chars to search backward.
//  *	@param pNeedle [in] The string to find.
//  *  @param pTerm [in] Optional phrase that must be found after needle phrase.
//  *  @return Pointer to the character after the needle match (if found), otherwise return NULL.
//  */
// char *IOP_findInRxReverse(rxDataBufferCtrl_t *pBuf, uint8_t rewindCnt, const char *pNeedle, const char *pTerm);


// /**
//  *	@brief Get a contiguous block of characters from the RX data buffer pages.
//  *  @param pBuf [in] Pointer data receive buffer.
//  *	@param pStart [in] Address within the RX buffer to start retrieving chars.
//  *  @param takeCnt [in] The number of chars to return.
//  *  @param pChars [out] Buffer to hold retrieved characters, must be takeCnt + 1. Buffer will be null terminated.
//  *  @return Pointer to the character after the needle match (if found), otherwise return NULL.
//  */
// uint16_t IOP_fetchFromRx(rxDataBufferCtrl_t *pBuf, char *pStart, uint16_t takeCnt, char *pChars);


/**
 *	@brief Clear receive COMMAND/CORE response buffer.
 */
void IOP_resetCoreRxBuffer();


// /**
//  *	@brief Initializes a RX data buffer control.
//  *  @param bufCtrl [in] Pointer to RX data buffer control structure to initialize.
//  *  @param rxBuf [in] Pointer to raw byte buffer to integrate into RxBufferCtrl.
//  *  @param rxBufSz [in] The size of the rxBuf passed in.
//  */
// uint16_t IOP_initRxBufferCtrl(rxDataBufferCtrl_t *bufCtrl, uint8_t *rxBuf, uint16_t rxBufSz);


// /**
//  *	@brief Syncs RX consumers with ISR for buffer swap.
//  *  @param bufCtrl [in] RX data buffer to sync.
//  *  @return True if overflow detected: buffer being assigned to IOP for filling is not empty
//  */
// void IOP_swapRxBufferPage(rxDataBufferCtrl_t *bufPtr);


// /**
//  *	@brief Reset a receive buffer for reuse.
//  *  @param bufPtr [in] Pointer to the buffer struct.
//  *  @param page [in] Index to buffer page to be reset.
//  */
// void IOP_resetRxDataBufferPage(rxDataBufferCtrl_t *bufPtr, uint8_t page);


// IOP Internal 
//void IOP_rxParseForEvents();            // atcmd dependency


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__LTEMC_IOP_H__ */
