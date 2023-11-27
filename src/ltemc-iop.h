/** ***************************************************************************
  @file 
  @brief LTEmC low-level I/O and interrupt processing.

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


#ifndef __LTEMC_IOP_H__
#define __LTEMC_IOP_H__

// #include "ltemc-types.h"
// #include "ltemc-nxp-sc16is.h"
#include <stdint.h>

#include <lq-bBuffer.h>                                         // RX buffer is a block-buffer type


/**
 *	\brief Typed numeric Input/Output Processor subsystem contants.
 */
enum IOP__Constants
{
    IOP__uartBaudRate = 115200,                             // baud rate between BGx and NXP UART
    IOP__uartFIFOBufferSz = 64,
    IOP__uartFIFOFillPeriod = (int)(1 / (double)IOP__uartBaudRate * 10 * IOP__uartFIFOBufferSz * 1000) + 1,

    IOP__rxDefaultTimeout = IOP__uartFIFOFillPeriod * 2,
    IOP__urcDetectBufferSz = 40
};


/** 
 *  \brief Struct for the IOP subsystem state. During initialization a pointer to this structure is reference in g_ltem1.
 * 
 *  Each of the protocols (sockets, MQTT, HTTP) have unique behaviors: sockets receive asynchronous alerts but the data
 *  receive process is fully synchronous, HTTP is synchronous on receive tied to page read, MQTT is fully asynchronous 
 *  with msg event receive signaling and data transfer taking place async behind the scenes initiated on interrupt. 
 */
typedef struct iop_tag
{
    const uint8_t volatile * txSrc;                    // source pointer to TX pending data, composed data is not altered by IOP
    volatile uint16_t txPendingCnt;         // outstanding char count for TX
    volatile bool dmActive;                 // interaction with BGx is now in data mode
    volatile uint16_t dmTxEvents;           // number of TX blocks sent during data mode
    volatile bool isrEnabled;               // flag to signal ISR to run normally (true), or return immediately

    uint8_t irqAttached;
    bBuffer_t *rxBffr;                              // receive buffer
    char txEot;                                     // if not NULL, char to output on empty TX FIFO; clears automatically on use.
 
    uint32_t isrInvokeCnt;                          // number of times the ISR function has been invoked
    uint32_t lastTxAt;                              // tick count when TX send started, used for response timeout detection
    uint32_t lastRxAt;                              // tick count when RX buffer fill level was known to have change
} iop_t;


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


// /**
//  *	@brief Stop IOP services.
//  */
// void IOP_detachIrq();


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

uint8_t IOP_getRxOccupied();

uint8_t IOP_getTxPending();


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

// ISR (accessible to ltemc_ module)
void IOP_interruptCallbackISR();

#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__LTEMC_IOP_H__ */
