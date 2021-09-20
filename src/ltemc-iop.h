/******************************************************************************
 *  \file ltemc-iop.h
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020, 2021 LooUQ Incorporated.
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
 ******************************************************************************
 * IOP is the Input/Output processor for the LTEm line of LooUQ IoT modems. It
 * manages the IO via SPI-UART bridge and multiplexes cmd\data streams.
 *****************************************************************************/

#ifndef __LTEMC_IOP_H__
#define __LTEMC_IOP_H__

//#include "ltemc-internal.h"
#include "ltemc-cbuf.h"
#include "ltemc-streams.h"
#include "ltemc-nxp-sc16is.h"

enum
{
                                    /* can be reduced based on you protocol selections and your data segment sizes */
    IOP__txBufferSize = 1800,       // size should be equal or greater than length of largest data transmission
    IOP__rxCoreBufferSize = 192,

    IOP__uartBaudRate = 115200,     // baud rate between BGx and NXP UART
    IOP__uartFIFOBufferSz = 64,
    IOP__uartFIFOFillPeriod = (int)(1 / (double)IOP__uartBaudRate * 10 * IOP__uartFIFOBufferSz * 1000) + 1,

    IOP__rxDefaultTimeout = IOP__uartFIFOFillPeriod * 2 + 192
};


/**
 *   \brief Brief inline static function to support doWork() readability
*/
static inline uint16_t IOP_rxPageDataAvailable(rxDataBufferCtrl_t *buf, uint8_t page)
{
    return buf->pages[page].head - buf->pages[page].tail;
}


/** 
 *  \brief Struct for the IOP subsystem state. During initialization a pointer to this structure is reference in g_ltem1.
 * 
 *  Each of the protocols (sockets, MQTT, HTTP) have unique behaviors: sockets receive asynchronous alerts but the data
 *  receive process is fully synchronous, HTTP is synchronous on receive tied to page read, MQTT is fully asynchronous 
 *  with msg event receive signaling and data transfer taking place async behind the scenes initiated on interrupt. 
 */
typedef volatile struct iop_tag
{
    cbuf_t *txBuf;                                      ///< transmit buffer (there is just one)
    uint16_t txPend;                                    ///< outstanding TX char pending
    rxCoreBufferCtrl_t *rxCBuffer;                      ///< URC and command response receive buffer, this is the default RX buffer
    iopStreamCtrl_t streamPeers[streamPeer_cnt];        ///< array of iopStream ctrl pointers, cast to a specific stream type: protocol or file stream
    iopStreamCtrl_t rxStreamCtrl;                       ///< stream data source, if not null
    uint32_t txSendStartTck;                            ///< tick count when TX send started, used for response timeout detection
    uint32_t rxLastFillChgTck;                          ///< tick count when RX buffer fill level was known to have change
    uint16_t rxLastFillLevel;                           ///< last known fill level (updated only during check)

    // protocol specific properties
    uint8_t scktMap;                                    ///< bitmap indicating open sockets (TCP/UDP/SSL), bit position is the dataContext (IOP event detect shortcut)
    uint8_t scktLstWrk;                                 ///< bit mask of last sckt do work IRD inquiry (fairness)
    uint8_t mqttMap;                                    ///< bitmap indicating open mqtt(s) connections, bit position is the dataContext (IOP event detect shortcut)
} iop_t;


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

void IOP_create();
void IOP_start();
void IOP_stopIrq();
void IOP_awaitAppReady();

void IOP_resetCoreRxBuffer();

uint16_t IOP_initRxBufferCtrl(rxDataBufferCtrl_t *bufCtrl, uint8_t *rxBuf, uint16_t rxBufSz);
void IOP_swapRxBufferPage(rxDataBufferCtrl_t *bufPtr);
void IOP_resetRxDataBufferPage(rxDataBufferCtrl_t *bufPtr, uint8_t page);
uint32_t IOP_getRxIdleDuration();
char *IOP_findRxAhead(rxDataBufferCtrl_t *pBuf, uint8_t rewindCnt, const char *pNeedle, const char *pTerm);
uint16_t IOP_fetchRxAhead(rxDataBufferCtrl_t *pBuf, char *pStart, uint16_t takeCnt, char *pChars);

void IOP_registerStream(streamPeer_t stream, iopStreamCtrl_t *streamCtrl);

uint16_t IOP_sendTx(const char *sendData, uint16_t sendSz, bool sendImmediate);

#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__LTEMC_IOP_H__ */
