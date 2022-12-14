/******************************************************************************
 *  \file ltemc-iop.c
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

/*** Known BGx Header Patterns Supported by LTEmC IOP ***
/**
 * @details

 Area/Msg Prefix
    // -- BG96 init
    // \r\nAPP RDY\r\n      -- BG96 completed firmware initialization
    // 
    // -- Commands
    // +QPING:              -- PING response (instance and summary header)
    // +QIURC: "dnsgip"     -- DNS lookup reply
    //
    // -- Protocols
    // +QIURC: "recv",      -- "unsolicited response" proto tcp/udp
    // +QIRD: #             -- "read data" response 
    // +QSSLURC: "recv"     -- "unsolicited response" proto ssl tunnel
    // +QHTTPGET:           -- GET response, HTTP-READ 
    // CONNECT<cr><lf>      -- HTTP Read
    // +QMTSTAT:            -- MQTT state change message recv'd
    // +QMTRECV:            -- MQTT subscription data message recv'd
    // 
    // -- Async Status Change Messaging
    // +QIURC: "pdpdeact"   -- network pdp context timed out and deactivated

    // default content type is command response
 * 
 */


#pragma region Header

#define _DEBUG 0                        // set to non-zero value for PRINTF debugging output, 
// debugging output options             // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if _DEBUG > 0
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG == 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #define PRINTF(c_,f_,__VA_ARGS__...) do { rtt_printf(c_, (f_), ## __VA_ARGS__); } while(0)
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#endif


#include "ltemc-iop.h"
#include "ltemc-internal.h"
#include "ltemc-sckt.h"                                 // protocols that are tightly coupled to I/O processing
#include "ltemc-mqtt.h"


//#define NULL 0

#define QBG_APPREADY_MILLISMAX 10000

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define IOP_RXCTRLBLK_ADVINDEX(INDX) INDX = (++INDX == IOP_RXCTRLBLK_COUNT) ? 0 : INDX


extern ltemDevice_t g_lqLTEM;


#pragma region Private Static Function Declarations
/* ------------------------------------------------------------------------------------------------ */

static cbuf_t *S_createTxBuffer();
static rxCoreBufferCtrl_t *S_createRxCoreBuffer();
static uint16_t S_putTx(const char *data, uint16_t dataSz);
static uint16_t S_takeTx(char *data, uint16_t dataSz);
static inline rxDataBufferCtrl_t *S_isrRxBufferSync();
static void S_interruptCallbackISR();
static inline uint8_t S_convertCharToContextId(const char cntxtChar);

#pragma endregion // Header


#pragma region Public Functions
/*-----------------------------------------------------------------------------------------------*/
#pragma endregion // Public Functions


#pragma region LTEm Internal Functions
/*-----------------------------------------------------------------------------------------------*/

/**
 *	@brief Initialize the Input/Output Process subsystem.
 */
void IOP_create()
{
    g_lqLTEM.iop = calloc(1, sizeof(iop_t));
    ASSERT(g_lqLTEM.iop != NULL, srcfile_ltemc_iop_c);

    g_lqLTEM.iop->txBuffer = S_createTxBuffer();
    g_lqLTEM.iop->rxCBuffer = S_createRxCoreBuffer(bufferSz__coreRx);       // create cmd/default RX buffer
}


/**
 *	@brief Complete initialization and start running IOP processes.
 */
void IOP_attachIrq()
{
    spi_usingInterrupt(g_lqLTEM.spi, g_lqLTEM.pinConfig.irqPin);
    platform_attachIsr(g_lqLTEM.pinConfig.irqPin, true, gpioIrqTriggerOn_falling, S_interruptCallbackISR);
}


/**
 *	@brief Stop IOP services.
 */
void IOP_detachIrq()
{
    platform_detachIsr(g_lqLTEM.pinConfig.irqPin);
}


/**
 *	@brief register a stream peer with IOP to control communications. Typically performed by protocol open.
 */
void IOP_registerStream(streamPeer_t streamIndx, iopStreamCtrl_t *streamCtrl)
{
    g_lqLTEM.iop->streamPeers[streamIndx] = streamCtrl;
}


/**
 *	@brief Verify LTEm firmware has started and is ready for driver operations.
 */
bool IOP_awaitAppReady()
{
    char buf[120] = {0};
    char *head = &buf;
    uint8_t rxLevel = 0;
    uint32_t waitStart = pMillis();

    while (pMillis() - waitStart < QBG_APPREADY_MILLISMAX)                  // typical wait: 700-1450 mS
    {
        pYield();                                                           // give application time for non-comm startup activities or watchdog
        rxLevel = SC16IS7xx_readReg(SC16IS7xx_RXLVL_regAddr);
        if (rxLevel > 0)
        {
            if ((head - buf) + rxLevel < 120)
            {
                SC16IS7xx_read(head, rxLevel);
                head += rxLevel;
                char *foundAt = strstr(buf, "APP RDY");
                if (foundAt)
                {
                    PRINTF(dbgColor__white, "AppRdy @ %lums\r", pMillis() - waitStart);
                    g_lqLTEM.deviceState = deviceState_appReady;
                    return true;
                }
            }
        }
    }
    PRINTF(dbgColor__warn, "AppRdy Timeout");
    return false;
}



/**
 *	@brief Start a (raw) send operation.
 */
uint16_t IOP_sendTx(const char *sendData, uint16_t sendSz, bool sendImmediate)
{
    uint16_t queuedSz = S_putTx(sendData, sendSz);                          // put sendData into global send buffer
    if (sendImmediate)                                                      // if sender done adding, send initial block of data
    {                                                                       // IOP ISR will continue sending chunks until global buffer empty
        char txData[SC16IS7xx__FIFO_bufferSz] = {0};                        // max size of the NXP TX FIFO
        do
        {
            uint8_t bufAvailable = SC16IS7xx_readReg(SC16IS7xx_TXLVL_regAddr);
            uint16_t dataToSendSz = S_takeTx(txData, bufAvailable);
            if (dataToSendSz == 0) 
                break;
            else
            {
                g_lqLTEM.iop->txSendStartTck = pMillis();
                PRINTF(dbgColor__dCyan, "txChunk=%s\r", txData);
                SC16IS7xx_write(txData, dataToSendSz);
                break;
            }
        } while (true);                                                     // loop until send at least 1 char
    }
    return queuedSz;                                                        // return number of chars queued, not char send cnt
}


/**
 *	@brief Get the idle time in milliseconds since last RX I/O.
 */
uint32_t IOP_getRxIdleDuration()
{
    return pMillis() - g_lqLTEM.iop->rxLastFillChgTck;
}


/**
 *	@brief Finds a string in the last X characters of the IOP RX stream.
 */
char *IOP_findInRxReverse(rxDataBufferCtrl_t *pBuf, uint8_t rewindCnt, const char *pNeedle, const char *pTerm)
{
    uint8_t needleLen = strlen(pNeedle);
    uint8_t termLen = strlen(pTerm);

    ASSERT(needleLen + termLen < rewindCnt, srcfile_ltemc_iop_c);
    uint16_t iopAvail = pBuf->pages[pBuf->iopPg].head - pBuf->pages[pBuf->iopPg].tail;

    if (needleLen + termLen > iopAvail + (pBuf->pages[!pBuf->iopPg].head - pBuf->pages[!pBuf->iopPg].tail))
        return NULL;

    // find start of needle
        //    uint16_t rewindRemaining = rewindCnt;
        //    uint8_t searchPg;
    // bool foundNeedle = false;
    char *pNeedleAt = NULL;
    uint8_t termSrchState = (termLen == 0) ? 2 : 0;                         // termSrchState: 0=nothing yet, 1=start found, 2=full match found

    uint8_t searchPg = pBuf->iopPg;
    char *pSearch = pBuf->pages[pBuf->iopPg].head - rewindCnt;
    if (rewindCnt > iopAvail)
    {
        searchPg = !pBuf->iopPg;
        pSearch = pBuf->pages[!pBuf->iopPg].head - iopAvail;
    }
    
    // find start of needle, looking for match
    while (pSearch < pBuf->pages[searchPg].head)
    {
        if (pNeedle[0] == *pSearch)
        {
            pNeedleAt = pSearch;
            break;
        }

        pSearch++;
        if (pSearch == pBuf->pages[!pBuf->iopPg].head)                      // end of RCV page, continue search in IOP page
        {
            searchPg = pBuf->iopPg;
            pSearch = pBuf->pages[pBuf->iopPg]._buffer;
        }
    }

    // find rest of needle, exit on miss
    for (size_t i = 0; i < needleLen; i++)
    {
        if (pNeedle[i] != *pSearch)
            return NULL;

        pSearch++;
        if (pSearch == pBuf->pages[!pBuf->iopPg].head)                      // end of RCV page, continue search in IOP page
        {
            searchPg = pBuf->iopPg;
            pSearch = pBuf->pages[pBuf->iopPg]._buffer;
        }
    }
    
    // find start of term, looking for match
    while (pSearch < pBuf->pages[searchPg].head)
    {
        if (pTerm[0] == *pSearch)
        {
            termSrchState = 1;
            break;
        }

        pSearch++;
        if (pSearch == pBuf->pages[!pBuf->iopPg].head)                      // end of RCV page, continue search in IOP page
        {
            searchPg = pBuf->iopPg;
            pSearch = pBuf->pages[pBuf->iopPg]._buffer;
        }
    }
    if (!termSrchState)
        return NULL;

    // find rest of term, exit on miss
    if (termSrchState < 2)
    {
        for (size_t i = 0; i < termLen; i++)
        {
            if (pTerm[i] != *pSearch)
                return NULL;

            pSearch++;
            if (pSearch == pBuf->pages[!pBuf->iopPg].head)                  // end of RCV page, continue search in IOP page
            {
                searchPg = pBuf->iopPg;
                pSearch = pBuf->pages[pBuf->iopPg]._buffer;
            }
        }
    }
    return pNeedleAt;
}


/**
 *	@brief Get a contiguous block of characters from the RX data buffer pages.
 */
uint16_t IOP_fetchFromRx(rxDataBufferCtrl_t *pBuf, char *pStart, uint16_t takeCnt, char *pChars)
{
    ASSERT(pBuf->_buffer <= pStart && pStart <= pBuf->_bufferEnd, srcfile_ltemc_iop_c);
    memset(pChars, 0, takeCnt + 1);

//    uint16_t iopAvail =  pBuf->pages[pBuf->iopPg].head -  pBuf->pages[pBuf->iopPg]._buffer;
    uint8_t startPg = pStart > pBuf->pages[1]._buffer;
    uint16_t takenCnt = 0;

    if (startPg != pBuf->iopPg) {
        uint16_t pgTake = pBuf->pages[!pBuf->iopPg].head - pStart;
        memcpy(pChars, pStart, pgTake);
        takeCnt -= pgTake;
        takenCnt += pgTake;

        if (takeCnt > 0)
        {
            uint16_t iopAvail = pBuf->pages[pBuf->iopPg].head - pBuf->pages[pBuf->iopPg]._buffer;
            takeCnt = MIN(takeCnt, iopAvail);
            memcpy(pChars + takenCnt, pBuf->pages[pBuf->iopPg]._buffer, takeCnt);
            takenCnt += takeCnt;
        }
    }
    else
    {
        uint16_t iopAvail =  pBuf->pages[pBuf->iopPg].head -  pStart;
        takeCnt = MIN(takeCnt, iopAvail);
        memcpy(pChars, pStart, takeCnt);
        takenCnt = takeCnt;
    }
    return takenCnt;
}


/**
 *	@brief Clear receive COMMAND/CORE response buffer.
 */
void IOP_resetCoreRxBuffer()
{
    rxCoreBufferCtrl_t *rxCBuf = g_lqLTEM.iop->rxCBuffer;

    memset(rxCBuf->_buffer, 0, (rxCBuf->head - rxCBuf->_buffer));
    rxCBuf->head = rxCBuf->prevHead = rxCBuf->tail = rxCBuf->_buffer;
}


/**
 *	@brief Initializes a RX data buffer control.
 */
uint16_t IOP_initRxBufferCtrl(rxDataBufferCtrl_t *bufCtrl, uint8_t *rxBuf, uint16_t rxBufSz)
{
    uint16_t pgSz = (rxBufSz / 128) * 64;           // determine page size on 64B boundary

    bufCtrl->_buffer = rxBuf;
    bufCtrl->_bufferSz = pgSz * 2;
    bufCtrl->_bufferEnd = rxBuf + bufCtrl->_bufferSz;
    bufCtrl->_pageSz = pgSz;

    bufCtrl->bufferSync = false;
    bufCtrl->iopPg = 0;
    bufCtrl->pages[0].head = bufCtrl->pages[0].prevHead = bufCtrl->pages[0].tail = bufCtrl->pages[0]._buffer = rxBuf;
    bufCtrl->pages[1].head = bufCtrl->pages[1].prevHead = bufCtrl->pages[1].tail = bufCtrl->pages[1]._buffer = rxBuf + pgSz;
    return pgSz * 2;
}


/**
 *	@brief Syncs RX consumers with ISR for buffer swap.
 */
void IOP_swapRxBufferPage(rxDataBufferCtrl_t *bufCtrl)
{
    bufCtrl->_nextIopPg = !bufCtrl->iopPg;                                  // set intent, used to check state if swap interrupted
    bufCtrl->bufferSync = true;                                             // set bufferSync, IOP ISR will finish swap if interrupted

    if (bufCtrl->iopPg != bufCtrl->_nextIopPg)
        bufCtrl->iopPg = bufCtrl->_nextIopPg;

    bufCtrl->bufferSync = false;
}


/**
 *	@brief Syncs RX consumers with ISR for buffer swap.
 */
static inline rxDataBufferCtrl_t *S_isrRxBufferSync()
{
    rxDataBufferCtrl_t* bufCtrl = &(g_lqLTEM.iop->rxStreamCtrl->recvBufCtrl);

    if (bufCtrl->bufferSync && bufCtrl->iopPg != bufCtrl->_nextIopPg)       // caught the swap underway and interrupted prior to swap change
            bufCtrl->iopPg = bufCtrl->_nextIopPg;                           // so finish swap and use it in ISR

    return bufCtrl;
}


/**
 *	@brief Reset a receive buffer for reuse.
 */
void IOP_resetRxDataBufferPage(rxDataBufferCtrl_t *bufPtr, uint8_t page)
{
    g_lqLTEM.iop->rxLastFillChgTck = 0;
    //memset(bufPtr->pages[page]._buffer, 0, (bufPtr->pages[page].head - bufPtr->pages[page]._buffer));
    bufPtr->pages[page].head = bufPtr->pages[page].prevHead = bufPtr->pages[page].tail = bufPtr->pages[page]._buffer;
}


#pragma endregion


#pragma region Static Function Definions
/*-----------------------------------------------------------------------------------------------*/

/**
 *	@brief Rapid fixed case conversion of context value returned from BGx to number.
 *  @param cntxChar [in] RX data buffer to sync.
 */
static inline uint8_t S_convertCharToContextId(const char cntxtChar)
{
    return (uint8_t)cntxtChar - 0x30;
}


/**
 *	@brief Create the IOP transmit buffer.
 */
static cbuf_t *S_createTxBuffer()
{
    cbuf_t *txBuf = calloc(1, sizeof(cbuf_t));
    if (txBuf == NULL)
        return NULL;

    txBuf->buffer = calloc(1, bufferSz__txData);
    if (txBuf->buffer == NULL)
    {
        free(txBuf);
        return NULL;
    }
    txBuf->maxlen = bufferSz__txData;
    return txBuf;
}


/**
 *	@brief Create the core/command receive buffer.
 *  @param bufSz [in] Size of the buffer to create.
 */
static rxCoreBufferCtrl_t *S_createRxCoreBuffer(size_t bufSz)
{
    rxCoreBufferCtrl_t *rxBuf = calloc(1, sizeof(rxCoreBufferCtrl_t));
    if (rxBuf == NULL)
        return NULL;

    rxBuf->_buffer = calloc(1, bufSz);
    if (rxBuf->_buffer == NULL)
    {
        free(rxBuf);
        return NULL;
    }
    rxBuf->head = rxBuf->prevHead = rxBuf->tail = rxBuf->_buffer;
    rxBuf->_bufferSz = bufSz;
    rxBuf->_bufferEnd = rxBuf->_buffer + bufSz;
    return rxBuf;
}


/**
 *  @brief Puts data into the TX buffer control structure. NOTE: this operation performs a copy.
 *  @param data [in] - Pointer to where source data is now. 
 *  @param dataSz [in] - How much data to put in the TX struct.
 *  @return The number of bytes of actual queued in the TX struct, compare to dataSz to determine if all data queued. 
*/
static uint16_t S_putTx(const char *data, uint16_t dataSz)
{
    uint16_t putCnt = 0;

    for (size_t i = 0; i < dataSz; i++)
    {
        if(cbuf_push(g_lqLTEM.iop->txBuffer, data[i]))
        {
            putCnt++;
            continue;
        }
        break;
    }
    g_lqLTEM.iop->txPend += putCnt;
    return putCnt;
}



/**
 *  @brief Gets (dequeues) data from the TX buffer control structure.
 *  @param data [in] - Pointer to where taken data will be placed.
 *  @param dataSz [in] - How much data to take, if possible.
 *  @return The number of bytes of data being returned. 
 */
static uint16_t S_takeTx(char *data, uint16_t dataSz)
{
    uint16_t takeCnt = 0;

    for (size_t i = 0; i < dataSz; i++)
    {
        if (cbuf_pop(g_lqLTEM.iop->txBuffer, data + i))
        {
            takeCnt++;
            continue;
        }
        break;
    }
    g_lqLTEM.iop->txPend -= takeCnt;
    return takeCnt;
}

#pragma endregion


#pragma region ISR

/**
 *  @brief Parse recv'd data (in command RX buffer) for async event preambles that need to be handled immediately.
 *  @details AT cmd uses this to examine new buffer contents for +URC events that may arrive in command response
 *           Declared in ltemc-internal.h
 */
void IOP_rxParseForUrcEvents()
{
    char *foundAt;
    char *urcBffr = g_lqLTEM.iop->urcDetectBuffer;

    /* SSL/TLS data received
    */
    if (g_lqLTEM.iop->scktMap > 0 && (foundAt = strstr(urcBffr, "+QSSLURC: \"recv\",")))         // shortcircuit if no sockets
    {
        PRINTF(dbgColor__cyan, "-p=sslURC");
        uint8_t urcLen = strlen("+QSSLURC: \"recv\",");
        char *cntxtIdPtr = urcBffr + urcLen;
        char *endPtr = NULL;
        uint8_t cntxtId = (uint8_t)strtol(cntxtIdPtr, &endPtr, 10);
        // action
        ((scktCtrl_t *)g_lqLTEM.iop->streamPeers[cntxtId])->dataPending = true;
        // clean up
        g_lqLTEM.iop->rxCBuffer->head - (endPtr - urcBffr);                                     // remove URC from rxBuffer
        memset(g_lqLTEM.iop->urcDetectBuffer, 0, IOP__urcDetectBufferSz);
    }

    // preserve temporarily Nov29-2022
    // else if (g_lqLTEM.iop->scktMap && memcmp("+QIURC: \"recv", urcStartPtr, strlen("+QIURC: \"recv")) == 0)         // shortcircuit if no sockets
    // {
    //     PRINTF(dbgColor__cyan, "-p=ipURC");
    //     char *cntxIdPtr = g_lqLTEM.iop->rxCBuffer->prevHead + strlen("+QIURC: \"recv");
    //     char *endPtr = NULL;
    //     uint8_t cntxId = (uint8_t)strtol(cntxIdPtr, &endPtr, 10);
    //     ((scktCtrl_t *)g_lqLTEM.iop->streamPeers[cntxId])->dataPending = true;
    //     // discard this chunk, processed here
    //     g_lqLTEM.iop->rxCBuffer->head = g_lqLTEM.iop->rxCBuffer->prevHead;
    // }

    /* TCP/UDP data received
    */
    else if (g_lqLTEM.iop->scktMap && (foundAt = strstr(urcBffr, "+QIURC: \"recv\",")))         // shortcircuit if no sockets
    {
        PRINTF(dbgColor__cyan, "-p=ipURC");
        uint8_t urcLen = strlen("+QIURC: \"recv\",");
        char *cntxtIdPtr = urcBffr + urcLen;
        char *endPtr = NULL;
        uint8_t cntxtId = (uint8_t)strtol(cntxtIdPtr, &endPtr, 10);
        // action
        ((scktCtrl_t *)g_lqLTEM.iop->streamPeers[cntxtId])->dataPending = true;
        // clean up
        g_lqLTEM.iop->rxCBuffer->head - (endPtr - urcBffr);                                     // remove URC from rxBuffer
        memset(g_lqLTEM.iop->urcDetectBuffer, 0, IOP__urcDetectBufferSz);
    }

    /* MQTT message receive
    */
    else if (g_lqLTEM.iop->mqttMap && (foundAt = strstr(urcBffr, "+QMTRECV: ")))
    {
        PRINTF(dbgColor__cyan, "-p=mqttR");
        uint8_t urcLen = strlen("+QMTRECV: ");
        char *cntxtIdPtr = urcBffr + urcLen;
        char *endPtr = NULL;
        uint8_t cntxtId = (uint8_t)strtol(cntxtIdPtr, &endPtr, 10);

        // action
        ASSERT(g_lqLTEM.iop->rxStreamCtrl == NULL, srcfile_ltemc_iop_c);                                // ASSERT: not inside another stream recv

        /* this chunk, contains both meta data for receive followed by actual data, need to copy the data chunk to start of rxDataBuffer for this context */

        g_lqLTEM.iop->rxStreamCtrl = g_lqLTEM.iop->streamPeers[cntxtId];                                // put IOP in datamode for context 
        rxDataBufferCtrl_t *dBufPtr = &g_lqLTEM.iop->rxStreamCtrl->recvBufCtrl;                         // get reference to context specific data RX buffer
        
        /* need to fixup core/cmd and data buffers for mixed content in receive
         * moving post prefix received from core/cmd buffer to context data buffer
         * preserving prefix text for overflow detection (prefix & trailer text must be in same buffer)
         */
        char *urcStartPtr = memchr(g_lqLTEM.iop->rxCBuffer->prevHead, '+', g_lqLTEM.iop->rxCBuffer->head - g_lqLTEM.iop->rxCBuffer->prevHead);  
        memcpy(dBufPtr->pages[dBufPtr->iopPg]._buffer, urcStartPtr, g_lqLTEM.iop->rxCBuffer->head - urcStartPtr);
        dBufPtr->pages[dBufPtr->iopPg].head += g_lqLTEM.iop->rxCBuffer->head - urcStartPtr;

        // clean-up
        g_lqLTEM.iop->rxCBuffer->head = urcStartPtr;                                                    // drop recv'd from cmd\core buffer, processed here
        memset(g_lqLTEM.iop->urcDetectBuffer, 0, IOP__urcDetectBufferSz);
    }

    /* MQTT connection reset by server
    */
    else if (g_lqLTEM.iop->mqttMap &&
             (foundAt = MAX(strstr(urcBffr, "+QMTSTAT: "), strstr(urcBffr, "+QMTDISC: "))))
    {
        PRINTF(dbgColor__cyan, "-p=mqttS");
        uint8_t urcLen = MAX(strlen("+QMTSTAT: "), strlen("+QMTDISC: "));
        char *cntxtIdPtr = urcBffr + urcLen;
        char *endPtr = NULL;
        uint8_t cntxId = (uint8_t)strtol(cntxtIdPtr, &endPtr, 10);
        // action
        g_lqLTEM.iop->mqttMap &= ~(0x01 << cntxId);
        ((mqttCtrl_t *)g_lqLTEM.iop->streamPeers[cntxId])->state = mqttState_closed;
        // clean up
        g_lqLTEM.iop->rxCBuffer->head - (endPtr - urcBffr);                                     // remove URC from rxBuffer
        memset(g_lqLTEM.iop->urcDetectBuffer, 0, IOP__urcDetectBufferSz);
    }

    /* PDP context closed by network
    */
    else if ((foundAt = strstr(urcBffr, "+QIURC: \"pdpdeact\",")))
    {
        PRINTF(dbgColor__cyan, "-p=pdpD");
        uint8_t urcLen = strlen("+QIURC: \"pdpdeact\",");
        char *pdpCntxtIdPtr = urcBffr + urcLen;
        char *endPtr = NULL;
        uint8_t contextId = (uint8_t)strtol(pdpCntxtIdPtr, &endPtr, 10);
        // action
        for (size_t i = 0; i <  sizeof(g_lqLTEM.providerInfo->networks) / sizeof(providerInfo_t); i++)
        {
            if (g_lqLTEM.providerInfo->networks[i].pdpContextId == contextId)
            {
                g_lqLTEM.providerInfo->networks[i].pdpContextId = 0;
                g_lqLTEM.providerInfo->networks[i].ipAddress[0] = 0;
                break;
            }
        }
        // clean-up
        g_lqLTEM.iop->rxCBuffer->head - (endPtr - urcBffr);                                     // remove URC from rxBuffer
        memset(g_lqLTEM.iop->urcDetectBuffer, 0, IOP__urcDetectBufferSz);

    }
}



/**
 *	@brief ISR for NXP UART interrupt events, the NXP UART performs all serial I/O with BGx.
 */
static void S_interruptCallbackISR()
{
    /* ----------------------------------------------------------------------------------------------------------------
     * NOTE: The IIR, TXLVL and RXLVL are read seemingly redundantly, this is required to ensure NXP SC16IS741
     * IRQ line is reset (belt AND suspenders).  During initial testing it was determined that without this 
     * duplication of register reads IRQ would latch in active IRQ state randomly.
     * ------------------------------------------------------------------------------------------------------------- */
    /*
    * IIR servicing:
    *   read (RHR) : buffer full (need to empty), timeout (chars recv'd, buffer not full but no more coming)
    *   write (THR): buffer emptied sufficiently to send more chars
    */

    SC16IS7xx_IIR iirVal;
    uint8_t rxLevel;
    uint8_t txAvailable;

    iirVal.reg = SC16IS7xx_readReg(SC16IS7xx_IIR_regAddr);
    retryIsr:
    PRINTF(dbgColor__white, "\rISR(%d:%d)[", iirVal.IRQ_nPENDING, iirVal.IRQ_SOURCE);

    do
    {
        uint8_t regReads = 0;
        while(iirVal.IRQ_nPENDING == 1 && regReads < 120)                               // wait for register, IRQ was signaled; safety limit at 120 in case of error gpio
        {
            iirVal.reg = SC16IS7xx_readReg(SC16IS7xx_IIR_regAddr);
            PRINTF(dbgColor__dRed, "*");
            regReads++;
        }


        if (iirVal.IRQ_SOURCE == 3)                                                     // priority 1 -- receiver line status error : clear fifo of bad char
        {
            PRINTF(dbgColor__error, "RXErr ");
            SC16IS7xx_flushRxFifo();
        }

        
        if (iirVal.IRQ_SOURCE == 2 || iirVal.IRQ_SOURCE == 6)                           // priority 2 -- receiver RHR full (src=2), receiver time-out (src=6)
        {                                                                               // Service Action: read RXLVL, read FIFO to empty
            rxLevel = SC16IS7xx_readReg(SC16IS7xx_RXLVL_regAddr);
            PRINTF(dbgColor__gray, "RX-sz=%d ", rxLevel);

            if (rxLevel > 0)
            {
                iop_t *iopPtr = g_lqLTEM.iop;
                if (iopPtr->rxStreamCtrl == NULL)                                       // in COMMAND\EVENT mode, use core buffer
                {
                    PRINTF(dbgColor__white, "-cmd ");
                    g_lqLTEM.iop->rxCBuffer->prevHead = g_lqLTEM.iop->rxCBuffer->head;  // save last head, to allow for recv'd data locally moved/discarded
                    SC16IS7xx_read(g_lqLTEM.iop->rxCBuffer->head, rxLevel);
                    g_lqLTEM.iop->rxCBuffer->head += rxLevel;

                    // look for potential URC event in last receive, URC start with '+' EX: "+QIURC: "pdpdeact",1"
                    char *pUrcPrefix;
                    if (g_lqLTEM.iop->urcDetectBuffer[0] == '\0')                       // urcDetectBuffer empty: look for new URC
                    {
                        pUrcPrefix = memchr(g_lqLTEM.iop->rxCBuffer->prevHead, '+', g_lqLTEM.iop->rxCBuffer->head - g_lqLTEM.iop->rxCBuffer->prevHead);
                        if (pUrcPrefix)
                        {
                            memset(g_lqLTEM.iop->urcDetectBuffer, 0, IOP__urcDetectBufferSz);
                            memcpy(g_lqLTEM.iop->urcDetectBuffer, pUrcPrefix, g_lqLTEM.iop->rxCBuffer->head - pUrcPrefix);
                            IOP_rxParseForUrcEvents();                                  // parse for URC events
                        }
                    }
                    else
                    {                                                                   // add new recv to previously started urcDetectBuffer
                        uint8_t urcFillSz = strlen(g_lqLTEM.iop->urcDetectBuffer);
                        uint8_t cpySz = MIN(rxLevel, IOP__urcDetectBufferSz - urcFillSz);
                        memcpy(g_lqLTEM.iop->urcDetectBuffer + urcFillSz, g_lqLTEM.iop->rxCBuffer->prevHead, cpySz);
                        IOP_rxParseForUrcEvents();                                      // parse for URC events
                    }
                }
                else                                                                    // in DATA mode, use stream's data buffers
                {
                    rxDataBufferCtrl_t *dBufPtr = S_isrRxBufferSync();                  // get rxStream's data buffer control, sync pending page swap
                    uint8_t iopPg = dBufPtr->iopPg;
                    SC16IS7xx_read(dBufPtr->pages[iopPg].head, rxLevel);
                    dBufPtr->pages[iopPg].head += rxLevel;
                    uint16_t fillLevel = dBufPtr->pages[iopPg].head - dBufPtr->pages[iopPg]._buffer;
                    iopPtr->rxLastFillChgTck = pMillis();

                    PRINTF(dbgColor__cyan, "-data(%d:%d) ", iopPg, fillLevel);
                    if (fillLevel > dBufPtr->_pageSz - IOP__uartFIFOBufferSz)           // if less than 1 UART_buffer of recv buffer available, swap buffer page
                    {
                        PRINTF(dbgColor__cyan, "-BSw>%d ", !iopPg);
                        IOP_swapRxBufferPage(dBufPtr);
                        // check buffer page swapped in for head past page start: OVERFLOW
                        dBufPtr->_overflow =  dBufPtr->pages[dBufPtr->_nextIopPg].head != dBufPtr->pages[dBufPtr->_nextIopPg]._buffer;
                    }
                }
            }
        }

        
        if (iirVal.IRQ_SOURCE == 1)                                                     // priority 3 -- transmit THR (threshold) : TX ready for more data
        {
            uint8_t buf[SC16IS7xx__FIFO_bufferSz] = {0};
            uint8_t thisTxSz;

            txAvailable = SC16IS7xx_readReg(SC16IS7xx_TXLVL_regAddr);
            PRINTF(dbgColor__gray, "TX-sz=%d ", txAvailable);

            if ((thisTxSz = S_takeTx(buf, txAvailable)) > 0)
            {
                PRINTF(dbgColor__dCyan, "txChunk=%s", buf);
                SC16IS7xx_write(buf, thisTxSz);
            }
        }

        /* -- NOT USED --
        // priority 4 -- modem interrupt
        // priority 6 -- receive XOFF/SpecChar
        // priority 7 -- nCTS, nRTS state change:
        */

        iirVal.reg = SC16IS7xx_readReg(SC16IS7xx_IIR_regAddr);

    } while (iirVal.IRQ_nPENDING == 0);

    PRINTF(dbgColor__white, "]\r");

    gpioPinValue_t irqPin = platform_readPin(g_lqLTEM.pinConfig.irqPin);
    if (irqPin == gpioValue_low)
    {
        iirVal.reg = SC16IS7xx_readReg(SC16IS7xx_IIR_regAddr);
        txAvailable = SC16IS7xx_readReg(SC16IS7xx_TXLVL_regAddr);
        rxLevel = SC16IS7xx_readReg(SC16IS7xx_RXLVL_regAddr);

        PRINTF(dbgColor__warn, "IRQ failed to reset!!! nIRQ=%d, iir=%d, txLvl=%d, rxLvl=%d \r", iirVal.IRQ_nPENDING, iirVal.reg, txAvailable, rxLevel);
        goto retryIsr;
    }
}


#pragma endregion

