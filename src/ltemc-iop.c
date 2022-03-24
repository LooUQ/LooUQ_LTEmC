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
#include "ltemc-nxp-sc16is.h"
#include "ltemc-internal.h"
#include "ltemc-sckt.h"
#include "ltemc-mqtt.h"

#include <lq-str.h>

#define QBG_APPREADY_MILLISMAX 5000

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define IOP_RXCTRLBLK_ADVINDEX(INDX) INDX = (++INDX == IOP_RXCTRLBLK_COUNT) ? 0 : INDX
//#define IOP_TX_ACTIVE()  do { g_ltem1->iop->txCtrl->remainSz > 0; } while(0);


extern ltemDevice_t g_ltem;


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
    g_ltem.iop = calloc(1, sizeof(iop_t));
    ASSERT(g_ltem.iop != NULL, srcfile_ltemc_iop_c);

    ((iop_t*)g_ltem.iop)->txBuf = S_createTxBuffer();
    ((iop_t*)g_ltem.iop)->rxCBuffer = S_createRxCoreBuffer(IOP__rxCoreBufferSize);        // create cmd/default RX buffer
    ((iop_t*)g_ltem.iop)->scktMap = 0;
    ((iop_t*)g_ltem.iop)->scktLstWrk = 0;
    ((iop_t*)g_ltem.iop)->mqttMap = 0;
    // ((iop_t*)g_ltem.iop)->httpMap = 0;
    ((iop_t*)g_ltem.iop)->rxStreamCtrl = NULL;                                          // != NULL, data mode on stream peer pointed
    memset(((iop_t*)g_ltem.iop)->streamPeers, 0, sizeof(void *) * streamPeer_cnt);
}


/**
 *	@brief Complete initialization and start running IOP processes.
 */
void IOP_start()
{
    spi_usingInterrupt(((spi_t*)g_ltem.spi), g_ltem.pinConfig.irqPin);
    gpio_attachIsr(g_ltem.pinConfig.irqPin, true, gpioIrqTriggerOn_falling, S_interruptCallbackISR);
}


/**
 *	@brief Stop IOP services.
 */
void IOP_stopIrq()
{
    gpio_detachIsr(g_ltem.pinConfig.irqPin);
}


/**
 *	@brief register a stream peer with IOP to control communications. Typically performed by protocol open.
 */
void IOP_registerStream(streamPeer_t streamIndx, iopStreamCtrl_t *streamCtrl)
{
    ((iop_t*)g_ltem.iop)->streamPeers[streamIndx] = streamCtrl;
}


/**
 *	@brief Verify LTEm firmware has started and is ready for driver operations.
 */
void IOP_awaitAppReady()
{
    char buf[120] = {0};
    char *head = &buf;
    uint8_t rxLevel = 0;
    uint32_t waitStart = pMillis();

    while (pMillis() - waitStart < QBG_APPREADY_MILLISMAX)      // typical wait: 700-1450 mS
    {
        pYield();                                               // give application time for non-comm startup activities or watchdog
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
                    g_ltem.qbgDeviceState = qbgDeviceState_appReady;
                    return;
                }
            }
        }
    }
}



/**
 *	@brief Start a (raw) send operation.
 */
uint16_t IOP_sendTx(const char *sendData, uint16_t sendSz, bool sendImmediate)
{
    uint16_t queuedSz = S_putTx(sendData, sendSz);    // put sendData into global send buffer
    // if (queuedSz < sendSz)
    //     ltem1_notifyApp(ltem1NotifType_bufferOverflow, "iop-tx buffer overflow");

    if (sendImmediate)                                  // if sender done adding, send initial block of data
    {                                                       // IOP ISR will continue sending chunks until global buffer empty
        char txData[SC16IS7xx__FIFO_bufferSz] = {0};       // max size of the NXP TX FIFO
        do
        {
            uint8_t bufAvailable = SC16IS7xx_readReg(SC16IS7xx_TXLVL_regAddr);
            uint16_t dataToSendSz = S_takeTx(txData, bufAvailable);
            if (dataToSendSz == 0) 
                break;
            else
            {
                ((iop_t*)g_ltem.iop)->txSendStartTck = pMillis();
                PRINTF(dbgColor__dCyan, "txChunk=%s\r", txData);
                SC16IS7xx_write(txData, dataToSendSz);
                break;
            }
        } while (true);                                     // loop until send at least 1 char
    }
    return queuedSz;                                     // return number of chars queued, not char send cnt
}


/**
 *	@brief Check for RX progress/idle.
 */
uint32_t IOP_getRxIdleDuration()
{
    ASSERT(((iop_t*)g_ltem.iop)->rxStreamCtrl != NULL, srcfile_ltemc_iop_c);

    rxDataBufferCtrl_t rxBuf = ((rxDataBufferCtrl_t)((baseCtrl_t*)((iop_t*)g_ltem.iop)->rxStreamCtrl)->recvBufCtrl);
    uint16_t fillNow = rxBuf.pages[rxBuf.iopPg].head - rxBuf.pages[rxBuf.iopPg]._buffer;

    uint32_t retVal = 0;

    if (((iop_t*)g_ltem.iop)->rxLastFillLevel == fillNow)           // idle now
    {
        retVal = pMillis() - ((iop_t*)g_ltem.iop)->rxLastFillChgTck;
    }
    else
    {
        ((iop_t*)g_ltem.iop)->rxLastFillChgTck = pMillis();
        ((iop_t*)g_ltem.iop)->rxLastFillLevel = fillNow;
    }
    return retVal;
}


/**
 *	@brief Finds a string in the last X characters of the IOP RX stream.
 */
char *IOP_findRxAhead(rxDataBufferCtrl_t *pBuf, uint8_t rewindCnt, const char *pNeedle, const char *pTerm)
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
    uint8_t termSrchState = (termLen == 0) ? 2 : 0;                     // termSrchState: 0=nothing yet, 1=start found, 2=full match found

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
        if (pSearch == pBuf->pages[!pBuf->iopPg].head)              // end of RCV page, continue search in IOP page
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
        if (pSearch == pBuf->pages[!pBuf->iopPg].head)              // end of RCV page, continue search in IOP page
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
        if (pSearch == pBuf->pages[!pBuf->iopPg].head)              // end of RCV page, continue search in IOP page
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
            if (pSearch == pBuf->pages[!pBuf->iopPg].head)          // end of RCV page, continue search in IOP page
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
uint16_t IOP_fetchRxAhead(rxDataBufferCtrl_t *pBuf, char *pStart, uint16_t takeCnt, char *pChars)
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
    rxCoreBufferCtrl_t *rxCBuf = ((iop_t*)g_ltem.iop)->rxCBuffer;

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
    ((iop_t*)g_ltem.iop)->rxLastFillLevel = 0;
    // ((iop_t*)g_ltem.iop)->rxLastFillChgTck = 0;

    bufCtrl->_nextIopPg = !bufCtrl->iopPg;     // set intent, used to check state if swap interrupted
    bufCtrl->bufferSync = true;                // set bufferSync, IOP ISR will finish swap if interrupted

    if (bufCtrl->iopPg != bufCtrl->_nextIopPg)
        bufCtrl->iopPg = bufCtrl->_nextIopPg;

    bufCtrl->bufferSync = false;
}


/**
 *	@brief Syncs RX consumers with ISR for buffer swap.
 */
static inline rxDataBufferCtrl_t *S_isrRxBufferSync()
{
    rxDataBufferCtrl_t* bufCtrl = &(((baseCtrl_t*)((iop_t*)g_ltem.iop)->rxStreamCtrl)->recvBufCtrl);

    if (bufCtrl->bufferSync && bufCtrl->iopPg != bufCtrl->_nextIopPg)       // caught the swap underway and interrupted prior to swap change
            bufCtrl->iopPg = bufCtrl->_nextIopPg;                           // so finish swap and use it in ISR

    return bufCtrl;
}


/**
 *	@brief Reset a receive buffer for reuse.
 */
void IOP_resetRxDataBufferPage(rxDataBufferCtrl_t *bufPtr, uint8_t page)
{
    ((iop_t*)g_ltem.iop)->rxLastFillLevel = 0;
    ((iop_t*)g_ltem.iop)->rxLastFillChgTck = 0;

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

    txBuf->buffer = calloc(1, IOP__txBufferSize);
    if (txBuf->buffer == NULL)
    {
        free(txBuf);
        return NULL;
    }
    txBuf->maxlen = IOP__txBufferSize;
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
        if(cbuf_push(((iop_t*)g_ltem.iop)->txBuf, data[i]))
        {
            putCnt++;
            continue;
        }
        break;
    }
    ((iop_t*)g_ltem.iop)->txPend += putCnt;
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
        if (cbuf_pop(((iop_t*)g_ltem.iop)->txBuf, data + i))
        {
            takeCnt++;
            continue;
        }
        break;
    }
    ((iop_t*)g_ltem.iop)->txPend -= takeCnt;
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
    char *urcStartPtr = memchr(((iop_t*)g_ltem.iop)->rxCBuffer->prevHead, '+', 6);             // all URC start with '+', skip leading \r\n 
    if (urcStartPtr)
    {
        if (((iop_t*)g_ltem.iop)->scktMap > 0 && memcmp("+QSSLURC: \"recv", urcStartPtr, strlen("+QSSLURC: \"recv")) == 0)    // shortcircuit if no sockets
        {
            PRINTF(dbgColor__cyan, "-p=sslURC");
            char *connIdPtr = ((iop_t*)g_ltem.iop)->rxCBuffer->prevHead + strlen("+QSSLURC: \"recv");
            char *endPtr = NULL;
            uint8_t socketId = (uint8_t)strtol(connIdPtr, &endPtr, 10);
            ((scktCtrl_t *)((iop_t*)g_ltem.iop)->streamPeers[socketId])->dataPending = true;
            // discard this chunk, processed here
            ((iop_t*)g_ltem.iop)->rxCBuffer->head = ((iop_t*)g_ltem.iop)->rxCBuffer->prevHead;
        }

        else if (((iop_t*)g_ltem.iop)->scktMap && memcmp("+QIURC: \"recv", urcStartPtr, strlen("+QIURC: \"recv")) == 0)       // shortcircuit if no sockets
        {
            PRINTF(dbgColor__cyan, "-p=ipURC");
            char *cntxIdPtr = ((iop_t*)g_ltem.iop)->rxCBuffer->prevHead + strlen("+QIURC: \"recv");
            char *endPtr = NULL;
            uint8_t cntxId = (uint8_t)strtol(cntxIdPtr, &endPtr, 10);
            ((scktCtrl_t *)((iop_t*)g_ltem.iop)->streamPeers[cntxId])->dataPending = true;
            // discard this chunk, processed here
            ((iop_t*)g_ltem.iop)->rxCBuffer->head = ((iop_t*)g_ltem.iop)->rxCBuffer->prevHead;
        }

        else if (((iop_t*)g_ltem.iop)->mqttMap && memcmp("+QMTRECV:", urcStartPtr, strlen("+QMTRECV")) == 0)
        {
            PRINTF(dbgColor__cyan, "-p=mqttR");
            //char *endPtr = NULL;
            uint8_t cntxtId = S_convertCharToContextId(*(urcStartPtr + strlen("+QMTRECV: ")));                  // parse for context #

            ASSERT(((iop_t*)g_ltem.iop)->rxStreamCtrl == NULL, srcfile_ltemc_iop_c);                            // ASSERT: not inside another stream recv

            // this chunk, contains both meta data for receive followed by actual data, need to copy the data chunk to start of rxDataBuffer for this context
            ((iop_t*)g_ltem.iop)->rxStreamCtrl = ((mqttCtrl_t *)((iop_t*)g_ltem.iop)->streamPeers[cntxtId]);    // put IOP in datamode for context 
            rxDataBufferCtrl_t *dBufPtr = &(((baseCtrl_t*)((iop_t*)g_ltem.iop)->rxStreamCtrl)->recvBufCtrl);    // get reference to context specific data RX buffer

            // move received from core\cmd buffer to context data buffer, preserving prefix text for overflow detection (prefix & trailer text must be in same buffer)
            memcpy(dBufPtr->pages[dBufPtr->iopPg]._buffer, urcStartPtr, ((iop_t*)g_ltem.iop)->rxCBuffer->head - urcStartPtr);
            dBufPtr->pages[dBufPtr->iopPg].head += ((iop_t*)g_ltem.iop)->rxCBuffer->head - urcStartPtr;

            ((iop_t*)g_ltem.iop)->rxCBuffer->head = ((iop_t*)g_ltem.iop)->rxCBuffer->prevHead;                  // drop recv'd from cmd\core buffer, processed here
        }

        else if (((iop_t*)g_ltem.iop)->mqttMap &&
                 (memcmp("+QMTSTAT:", urcStartPtr, 8) == 0 || 
                 memcmp("+QMTDISC", urcStartPtr, 8) == 0))
        {
            PRINTF(dbgColor__cyan, "-p=mqttS");
            char *cntxIdPtr = ((iop_t*)g_ltem.iop)->rxCBuffer->prevHead + 10;               // look past "+QMTSTAT: " or "+QMTDISC: "
            char *endPtr = NULL;
            uint8_t cntxId = (uint8_t)strtol(cntxIdPtr, &endPtr, 10);
            ((iop_t*)g_ltem.iop)->mqttMap &= ~(0x01 << cntxId);
            ((mqttCtrl_t *)((iop_t*)g_ltem.iop)->streamPeers[cntxId])->state = mqttState_closed;
        }

        else if (memcmp("+QIURC: \"pdpdeact", urcStartPtr, strlen("+QIURC: \"pdpdeact")) == 0)
        {
            PRINTF(dbgColor__cyan, "-p=pdpD");

            char *connIdPtr = ((iop_t*)g_ltem.iop)->rxCBuffer->prevHead + strlen("+QIURC: \"pdpdeact");
            char *endPtr = NULL;
            uint8_t contextId = (uint8_t)strtol(connIdPtr, &endPtr, 10);
            for (size_t i = 0; i <  sizeof(((network_t*)g_ltem.network)->pdpCntxts) / sizeof(pdpCntxt_t); i++)
            {
                if (((network_t*)g_ltem.network)->pdpCntxts[i].contextId == contextId)
                {
                    ((network_t*)g_ltem.network)->pdpCntxts[i].contextId = 0;
                    ((network_t*)g_ltem.network)->pdpCntxts[i].ipAddress[0] = 0;
                    break;
                }
            }
            // discard this chunk, processed here
            ((iop_t*)g_ltem.iop)->rxCBuffer->head = ((iop_t*)g_ltem.iop)->rxCBuffer->prevHead;
        }
    }

    // else if (g_ltem.qbgDeviceState != qbgDeviceState_appReady && memcmp("\r\nAPP RDY", ((iop_t*)g_ltem.iop)->rxCBuffer->prevHead, strlen("\r\nAPP RDY")) == 0)
    // {
    //     PRINTF(dbgColor__cyan, "-p=aRdy");
    //     g_ltem.qbgDeviceState = qbgDeviceState_appReady;
    //     // discard this chunk, processed here
    //     ((iop_t*)g_ltem.iop)->rxCBuffer->head = ((iop_t*)g_ltem.iop)->rxCBuffer->prevHead;
    // }
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
        while(iirVal.IRQ_nPENDING == 1 && regReads < 120)           // wait for register, IRQ was signaled; safety limit at 120 in case of error gpio
        {
            iirVal.reg = SC16IS7xx_readReg(SC16IS7xx_IIR_regAddr);
            PRINTF(dbgColor__dRed, "*");
            regReads++;
        }


        if (iirVal.IRQ_SOURCE == 3)                                 // priority 1 -- receiver line status error : clear fifo of bad char
        {
            PRINTF(dbgColor__error, "RXErr ");
            SC16IS7xx_flushRxFifo();
        }

        
        if (iirVal.IRQ_SOURCE == 2 || iirVal.IRQ_SOURCE == 6)       // priority 2 -- receiver RHR full (src=2), receiver time-out (src=6)
        {                                                           // Service Action: read RXLVL, read FIFO to empty
            PRINTF(dbgColor__gray, "RX(%d)", iirVal.IRQ_SOURCE);
            rxLevel = SC16IS7xx_readReg(SC16IS7xx_RXLVL_regAddr);
            PRINTF(dbgColor__gray, "-sz=%d ", rxLevel);

            if (rxLevel > 0)
            {
                iop_t *iopPtr = ((iop_t*)g_ltem.iop);
                if (iopPtr->rxStreamCtrl != NULL)                                               // in DATA mode
                {
                    rxDataBufferCtrl_t *dBufPtr = S_isrRxBufferSync();                          // get rxStream's data buffer control, sync pending page swap
                    uint8_t iopPg = dBufPtr->iopPg;
                    SC16IS7xx_read(dBufPtr->pages[iopPg].head, rxLevel);
                    dBufPtr->pages[iopPg].head += rxLevel;
                    uint16_t fillLevel = dBufPtr->pages[iopPg].head - dBufPtr->pages[iopPg]._buffer;

                    PRINTF(dbgColor__cyan, "-data(%d:%d) ", iopPg, fillLevel);
                    if (fillLevel > dBufPtr->_pageSz - IOP__uartFIFOBufferSz)
                    {
                        PRINTF(dbgColor__cyan, "-BSw>%d ", !iopPg);
                        IOP_swapRxBufferPage(dBufPtr);
                        // check buffer page swapped in for head past page start: OVERFLOW
                        dBufPtr->_overflow =  dBufPtr->pages[dBufPtr->_nextIopPg].head != dBufPtr->pages[dBufPtr->_nextIopPg]._buffer;
                    }
                }
                else                                                                            // in COMMAND\EVENT mode (aka not data mode), use core buffer
                {
                    PRINTF(dbgColor__white, "-cmd ");
                    SC16IS7xx_read(((iop_t*)g_ltem.iop)->rxCBuffer->head, rxLevel);
                    ((iop_t*)g_ltem.iop)->rxCBuffer->prevHead = ((iop_t*)g_ltem.iop)->rxCBuffer->head;  // save last head if RX moved/discarded
                    ((iop_t*)g_ltem.iop)->rxCBuffer->head += rxLevel;
                    IOP_rxParseForUrcEvents();                                                          // parse recv'd for events or immediate processing and discard
                }
            }
        }

        
        if (iirVal.IRQ_SOURCE == 1)                                 // priority 3 -- transmit THR (threshold) : TX ready for more data
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

    gpioPinValue_t irqPin = gpio_readPin(g_ltem.pinConfig.irqPin);
    if (irqPin == gpioValue_low)
    {
        iirVal.reg = SC16IS7xx_readReg(SC16IS7xx_IIR_regAddr);
        txAvailable = SC16IS7xx_readReg(SC16IS7xx_TXLVL_regAddr);
        rxLevel = SC16IS7xx_readReg(SC16IS7xx_RXLVL_regAddr);

        PRINTF(dbgColor__warn, "IRQ failed to reset!!! nIRQ=%d, iir=%d, txLvl=%d, rxLvl=%d \r", iirVal.IRQ_nPENDING, iirVal.reg, txAvailable, rxLevel);
        //ltem1_notifyApp(ltem1NotifType_resetFailed, "IRQ failed to reset.");
        goto retryIsr;
    }
}


#pragma endregion

