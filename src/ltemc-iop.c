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

//  Area/Msg Prefix
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
*/

#pragma region Header

#define _DEBUG 2                        // set to non-zero value for PRINTF debugging output, 
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

#define QBG_APPREADY_MILLISMAX 5000

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define IOP_RXCTRLBLK_ADVINDEX(INDX) INDX = (++INDX == IOP_RXCTRLBLK_COUNT) ? 0 : INDX
//#define IOP_TX_ACTIVE()  do { g_ltem1->iop->txCtrl->remainSz > 0; } while(0);

extern ltemDevice_t g_ltem;

// debuggin temporary
uint8_t dbg_urcInCmdLock = 0;


/* Static Local Functions Declarations
------------------------------------------------------------------------------------------------ */
static cbuf_t *S_createTxBuffer();
static rxCoreBufferCtrl_t *S_createRxCoreBuffer();
static uint16_t S_putTx(const char *data, uint16_t dataSz);
static uint16_t S_takeTx(char *data, uint16_t dataSz);
// static void S_resetRxDataBuffer(rxDataBufferCtrl_t *rxBuf, uint8_t page, bool streamEot);
static inline rxDataBufferCtrl_t *S_isrCheckRxBufferSync();
static void S_interruptCallbackISR();
static inline uint8_t S_convertToContextId(const char cntxtChar);

#pragma endregion // Header


#pragma region Public Functions
/*-----------------------------------------------------------------------------------------------*/
#pragma endregion // Public Functions


#pragma region LTEm Internal Functions
/*-----------------------------------------------------------------------------------------------*/

/**
 *	\brief Initialize the Input/Output Process subsystem.
 */
void IOP_create()
{
    g_ltem.iop = calloc(1, sizeof(iop_t));
    ASSERT(g_ltem.iop != NULL, srcfile_iop_c);

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
 *	\brief register a stream peer with IOP to control communications. Typically performed by protocol open.
 */
void IOP_registerStream(streamPeer_t streamIndx, iopStreamCtrl_t *streamCtrl)
{
    ((iop_t*)g_ltem.iop)->streamPeers[streamIndx] = streamCtrl;
}


/**
 *	\brief Complete initialization and start running iop processes.
 */
void IOP_start()
{
    // attach ISR and enable NXP interrupt mode
    gpio_attachIsr(g_ltem.pinConfig.irqPin, true, gpioIrqTriggerOn_falling, S_interruptCallbackISR);
    spi_protectFromInterrupt(((spi_t*)g_ltem.spi), g_ltem.pinConfig.irqPin);
    SC16IS741A_enableIrqMode();
}



/**
 *	\brief Verify LTEm1 is ready for driver operations.
 */
void IOP_awaitAppReady()
{
    unsigned long apprdyWaitStart = pMillis();
    while (g_ltem.qbgReadyState < qbg_readyState_appReady)
    {
        pYield();
        if (apprdyWaitStart + QBG_APPREADY_MILLISMAX < pMillis())
        ltem_notifyApp(lqNotificationType_lqDevice_hwFault,  "qbg-BGx module failed to start in the allowed time");
    }
}



/**
 *	\brief Start a (raw) send operation.
 *
 *  \param sendData [in] - Pointer to char data to send out, input buffer can be discarded following call.
 *  \param sendSz [in] - The number of characters to send.
 *  \param sendReady [in] - If true, queue sendData then initiate the actual send process. If false continue queueing and wait to send.
 * 
 *  \return Number of characters queued for sending.
 */
uint16_t IOP_sendTx(const char *sendData, uint16_t sendSz, bool sendImmediate)
{
    uint16_t queuedSz = S_putTx(sendData, sendSz);    // put sendData into global send buffer
    // if (queuedSz < sendSz)
    //     ltem1_notifyApp(ltem1NotifType_bufferOverflow, "iop-tx buffer overflow");

    if (sendImmediate)                                  // if sender done adding, send initial block of data
    {                                                       // IOP ISR will continue sending chunks until global buffer empty
        char txData[sc16is741a__LTEM_FIFO_bufferSz] = {0};       // max size of the NXP TX FIFO
        do
        {
            uint8_t bufAvailable = SC16IS741A_readReg(SC16IS741A_TXLVL_ADDR);
            uint16_t dataToSendSz = S_takeTx(txData, bufAvailable);
            if (dataToSendSz == 0) 
                break;
            else
            {
                ((iop_t*)g_ltem.iop)->txSendStartTck = pMillis();
                PRINTF(dbgColor__dCyan, "txChunk=%s\r", txData);
                SC16IS741A_write(txData, dataToSendSz);
                break;
            }
        } while (true);                                     // loop until send at least 1 char
    }
    return queuedSz;                                     // return number of chars queued, not char send cnt
}


/**
 *	\brief Check for RX idle (no incoming receives from NXP in 2.5 buffer times).
 *
 *  \return True 
 */
bool IOP_detectRxIdle()
{
    ASSERT(((iop_t*)g_ltem.iop)->rxStreamCtrl != NULL, srcfile_iop_c);

    rxDataBufferCtrl_t rxBuf = ((rxDataBufferCtrl_t)((baseCtrl_t*)((iop_t*)g_ltem.iop)->rxStreamCtrl)->recvBufCtrl);
    uint16_t fillNow = rxBuf.pages[rxBuf.iopPg].head - rxBuf.pages[rxBuf.iopPg]._buffer;

    if (((iop_t*)g_ltem.iop)->rxLastRecvChkLvl < fillNow)           // recv'd bytes since last check
    {
        ((iop_t*)g_ltem.iop)->rxLastRecvChkLvl = fillNow;
        return false;
    }

    if (pElapsed(((iop_t*)g_ltem.iop)->rxLastRecvTck, IOP__uartFIFO_fillMS * 2 + 1))
    {
        return true;
    }
    return false;                               // insufficient time to determine idle
}


/**
 *	\brief Clear receive COMMAND\CORE response buffer.
 */
void IOP_resetCoreRxBuffer()
{
    rxCoreBufferCtrl_t *rxCBuf = ((iop_t*)g_ltem.iop)->rxCBuffer;

    memset(rxCBuf->_buffer, 0, (rxCBuf->head - rxCBuf->_buffer));
    rxCBuf->head = rxCBuf->prevHead = rxCBuf->tail = rxCBuf->_buffer;
}


/**
 *	\brief Initializes a RX buffer control.
 *
 *  \param bufCtrl [in] Pointer to RX data buffer control structure to initialize.
 *  \param rxBuf [in] Pointer to raw byte buffer to integrate into RxBufferCtrl.
 *  \param rxBufSz [in] The size of the rxBuf passed in.
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
 *	\brief Syncs RX consumers with ISR for buffer swap.
 *
 *  \param bufCtrl [in] RX data buffer to sync.
 * 
 *  \return True if overflow detected: buffer being assigned to IOP for filling is not empty
 */
void IOP_swapRxBufferPage(rxDataBufferCtrl_t *bufCtrl)
{
    ((iop_t*)g_ltem.iop)->rxLastRecvChkLvl = 0;
    ((iop_t*)g_ltem.iop)->rxLastRecvTck = 0;

    bufCtrl->_nextIopPg = !bufCtrl->iopPg;     // set intent, used to check state if swap interrupted
    bufCtrl->bufferSync = true;                // set bufferSync, IOP ISR will finish swap if interrupted

    if (bufCtrl->iopPg != bufCtrl->_nextIopPg)
        bufCtrl->iopPg = bufCtrl->_nextIopPg;

    bufCtrl->bufferSync = false;
}


/**
 *	\brief Syncs RX consumers with ISR for buffer swap.
 *
 *  \param bufCtrl [in] RX data buffer to sync.
 */
static inline rxDataBufferCtrl_t *S_isrCheckRxBufferSync()
{
    rxDataBufferCtrl_t* bufCtrl = &(((baseCtrl_t*)((iop_t*)g_ltem.iop)->rxStreamCtrl)->recvBufCtrl);

    if (bufCtrl->bufferSync && bufCtrl->iopPg != bufCtrl->_nextIopPg)       // caught the swap underway and interrupted prior to swap change
            bufCtrl->iopPg = bufCtrl->_nextIopPg;                           // so finish swap and use it in ISR

    return bufCtrl;
}


/**
 *	\brief Rapid fixed case conversion of context value returned from BGx to number.
 *
 *  \param cntxChar [in] RX data buffer to sync.
 */
static inline uint8_t S_convertToContextId(const char cntxtChar)
{
    return (uint8_t)cntxtChar - 0x30;
}


/**
 *	\brief Reset a receive buffer for reuse.
 *
 *  \param bufPtr [in] Pointer to the buffer struct.
 *  \param page [in] Index to buffer page to be reset
 *  \param streamEot [in] True if the stream has complete and the "completion" check fields should also be cleared
 */
void IOP_resetRxDataBufferPage(rxDataBufferCtrl_t *bufPtr, uint8_t page)
{
    //memset(bufPtr->pages[page]._buffer, 0, (bufPtr->pages[page].head - bufPtr->pages[page]._buffer));
    bufPtr->pages[page].head = bufPtr->pages[page].prevHead = bufPtr->pages[page].tail = bufPtr->pages[page]._buffer;
}


#pragma endregion


#pragma region Static Function Definions
/*-----------------------------------------------------------------------------------------------*/

/**
 *	\brief Create the IOP transmit buffer.
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
 *	\brief Create the core/command receive buffer.
 *
 *  \param bufSz [in] Size of the buffer to create.
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


// /**
//  *	\brief Reset a receive buffer for reuse.
//  *
//  *  \param rxBuf [in] Pointer to the buffer struct.
//  *  \param bufPg [in] Index to buffer page to be reset
//  *  \param streamEot [in] True if the stream has complete and the "completion" check fields should also be cleared
//  */
// static void S_resetRxDataBuffer(rxDataBufferCtrl_t *rxBuf, uint8_t bufPg, bool streamEot)
// {
//     memset(rxBuf->pages[bufPg]._buffer, 0, (rxBuf->pages[bufPg].head - rxBuf->pages[bufPg]._buffer));
//     rxBuf->pages[bufPg].head = rxBuf->pages[bufPg].prevHead = rxBuf->pages[bufPg].tail = rxBuf->pages[bufPg]._buffer;
//     if (streamEot)
//         rxBuf->iopPg = 0;
// }


/**
 *  \brief Puts data into the TX buffer control structure. NOTE: this operation performs a copy.
 * 
 *  \param data [in] - Pointer to where source data is now. 
 *  \param dataSz [in] - How much data to put in the TX struct.
 * 
 *  \return The number of bytes of actual queued in the TX struct, compare to dataSz to determine if all data queued. 
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
 *  \brief Gets (dequeues) data from the TX buffer control structure.
 * 
 *  \param data [in] - Pointer to where taken data will be placed.
 *  \param dataSz [in] - How much data to take, if possible.
 * 
 *  \return The number of bytes of data being returned. 
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
 *  \brief Parse recv'd data (in command RX buffer) for async event preambles that need to be handled immediately.
 *  
 *  Internal only function. Dependency in atcmd
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
            char *continuePtr = urcStartPtr + strlen("+QMTRECV: ");
            uint8_t cntxtId = S_convertToContextId(*continuePtr);
            ASSERT(((iop_t*)g_ltem.iop)->rxStreamCtrl == NULL, srcfile_iop_c);                                  // should not be inside another stream recv

            dbg_urcInCmdLock += ((atcmd_t*)g_ltem.atcmd)->isOpenLocked;

            // this chunk, contains both meta data for receive and the data, need to copy this chunk to start of rxDataBuffer for context
            ((iop_t*)g_ltem.iop)->rxStreamCtrl = ((mqttCtrl_t *)((iop_t*)g_ltem.iop)->streamPeers[cntxtId]);    // put IOP in datamode for context new recv as data
            rxDataBufferCtrl_t *dBufPtr = &(((baseCtrl_t*)((iop_t*)g_ltem.iop)->rxStreamCtrl)->recvBufCtrl);    // get reference to context specific data RX buffer

            // move received from core\cmd buffer to context data buffer, preserving prefix text for overflow detection (prefix & trailer text must be in same buffer)
            memcpy(dBufPtr->pages[dBufPtr->iopPg]._buffer, urcStartPtr, ((iop_t*)g_ltem.iop)->rxCBuffer->head - urcStartPtr);
            dBufPtr->pages[dBufPtr->iopPg].head += ((iop_t*)g_ltem.iop)->rxCBuffer->head - urcStartPtr;
            // memcpy(dBufPtr->_buffer, continuePtr, ((iop_t*)g_ltem.iop)->rxCBuffer->head - continuePtr);     // move to data buffer (from cmd\core)

            ((iop_t*)g_ltem.iop)->rxCBuffer->head = ((iop_t*)g_ltem.iop)->rxCBuffer->prevHead;                  // drop recv'd from cmd\core buffer, processed here
        }

        else if (((iop_t*)g_ltem.iop)->mqttMap && memcmp("+QMTSTAT:", urcStartPtr, strlen("+QMTSTAT:")) == 0)
        {
            PRINTF(dbgColor__cyan, "-p=mqttS");
            char *cntxIdPtr = ((iop_t*)g_ltem.iop)->rxCBuffer->prevHead + strlen("+QIURC: \"recv");
            char *endPtr = NULL;
            uint8_t cntxId = (uint8_t)strtol(cntxIdPtr, &endPtr, 10);
            ((mqttCtrl_t *)((iop_t*)g_ltem.iop)->streamPeers[cntxId])->state = mqttStatus_closed;
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

    else if (g_ltem.qbgReadyState != qbg_readyState_appReady && memcmp("\r\nAPP RDY", ((iop_t*)g_ltem.iop)->rxCBuffer->prevHead, strlen("\r\nAPP RDY")) == 0)
    {
        PRINTF(dbgColor__cyan, "-p=aRdy");
        g_ltem.qbgReadyState = qbg_readyState_appReady;
        // discard this chunk, processed here
        ((iop_t*)g_ltem.iop)->rxCBuffer->head = ((iop_t*)g_ltem.iop)->rxCBuffer->prevHead;
    }
}



/**
 *	\brief ISR for NXP UART interrupt events, the NXP UART performs all serial I/O with BGx.
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

    SC16IS741A_IIR iirVal;
    uint8_t rxLevel;
    uint8_t txAvailable;

    iirVal.reg = SC16IS741A_readReg(SC16IS741A_IIR_ADDR);
    retryIsr:
    PRINTF(dbgColor__white, "\rISR[");

    do
    {
        while(iirVal.IRQ_nPENDING == 1)                             // wait for register, IRQ was signaled
        {
            iirVal.reg = SC16IS741A_readReg(SC16IS741A_IIR_ADDR);
            PRINTF(dbgColor__dRed, "*");
        }


        if (iirVal.IRQ_SOURCE == 3)                                 // priority 1 -- receiver line status error : clear fifo of bad char
        {
            PRINTF(dbgColor__error, "RXErr ");
            SC16IS741A_flushRxFifo();
        }

        
        if (iirVal.IRQ_SOURCE == 2 || iirVal.IRQ_SOURCE == 6)       // priority 2 -- receiver RHR full (src=2), receiver time-out (src=6)
        {                                                           // Service Action: read RXLVL, read FIFO to empty
            PRINTF(dbgColor__gray, "RX(%d)", iirVal.IRQ_SOURCE);
            rxLevel = SC16IS741A_readReg(SC16IS741A_RXLVL_ADDR);
            PRINTF(dbgColor__gray, "-sz=%d ", rxLevel);

            if (rxLevel > 0)
            {
                iop_t *iopPtr = ((iop_t*)g_ltem.iop);
                if (iopPtr->rxStreamCtrl != NULL)                                               // in DATA mode
                {
                    rxDataBufferCtrl_t *dBufPtr = S_isrCheckRxBufferSync();                         // check and get rxStream's data buffer control
                    uint8_t iopPg = dBufPtr->iopPg;
                    SC16IS741A_read(dBufPtr->pages[iopPg].head, rxLevel);
                    dBufPtr->pages[iopPg].head += rxLevel;

                    uint16_t fillLevel = dBufPtr->pages[iopPg].head - dBufPtr->pages[iopPg]._buffer;
                    iopPtr->rxLastRecvTck = pMillis();

                    PRINTF(dbgColor__cyan, "-data(%d:%d) ", iopPg, fillLevel);
                    if (fillLevel > dBufPtr->_pageSz - IOP__uartFIFOBufferSz)
                    {
                        PRINTF(dbgColor__cyan, "-BSw-%d> ", iopPg);
                        IOP_swapRxBufferPage(dBufPtr);
                        // check buffer page swapped in for head past page start: OVERFLOW
                        dBufPtr->_overflow =  dBufPtr->pages[dBufPtr->_nextIopPg].head != dBufPtr->pages[dBufPtr->_nextIopPg]._buffer;
                    }
                }
                else                                                                            // in COMMAND\EVENT mode (aka not data mode), use core buffer
                {
                    PRINTF(dbgColor__white, "-cmd ");
                    SC16IS741A_read(((iop_t*)g_ltem.iop)->rxCBuffer->head, rxLevel);
                    ((iop_t*)g_ltem.iop)->rxCBuffer->prevHead = ((iop_t*)g_ltem.iop)->rxCBuffer->head;  // save last head if RX moved/discarded
                    ((iop_t*)g_ltem.iop)->rxCBuffer->head += rxLevel;
                    IOP_rxParseForUrcEvents();                                                          // parse recv'd for events or immediate processing and discard
                }
            }
        }

        
        if (iirVal.IRQ_SOURCE == 1)                                 // priority 3 -- transmit THR (threshold) : TX ready for more data
        {
            uint8_t buf[sc16is741a__LTEM_FIFO_bufferSz] = {0};
            uint8_t thisTxSz;

            txAvailable = SC16IS741A_readReg(SC16IS741A_TXLVL_ADDR);
            PRINTF(dbgColor__gray, "TX-sz=%d ", txAvailable);

            if ((thisTxSz = S_takeTx(buf, txAvailable)) > 0)
            {
                PRINTF(dbgColor__dCyan, "txChunk=%s", buf);
                SC16IS741A_write(buf, thisTxSz);
            }
        }

        /* -- NOT USED --
        // priority 4 -- modem interrupt
        // priority 6 -- receive XOFF/SpecChar
        // priority 7 -- nCTS, nRTS state change:
        */

        iirVal.reg = SC16IS741A_readReg(SC16IS741A_IIR_ADDR);

    } while (iirVal.IRQ_nPENDING == 0);

    PRINTF(dbgColor__white, "]\r");

    gpioPinValue_t irqPin = gpio_readPin(g_ltem.pinConfig.irqPin);
    if (irqPin == gpioValue_low)
    {
        iirVal.reg = SC16IS741A_readReg(SC16IS741A_IIR_ADDR);
        txAvailable = SC16IS741A_readReg(SC16IS741A_TXLVL_ADDR);
        rxLevel = SC16IS741A_readReg(SC16IS741A_RXLVL_ADDR);

        PRINTF(dbgColor__warn, "IRQ failed to reset!!! nIRQ=%d, iir=%d, txLvl=%d, rxLvl=%d \r", iirVal.IRQ_nPENDING, iirVal.reg, txAvailable, rxLevel);
        //ltem1_notifyApp(ltem1NotifType_resetFailed, "IRQ failed to reset.");
        goto retryIsr;
    }
}


#pragma endregion

