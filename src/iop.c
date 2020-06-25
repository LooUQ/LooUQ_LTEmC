/******************************************************************************
 *  \file iop.h
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
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
 * IOP is the Input/Output processor for the LTEm1 SPI-UART bridge. It clocks
 * data to/from the LTEm1 to be handled by the command or protocol processes.
 *****************************************************************************/

#include "ltem1c.h"
//#include "components\nxp_sc16is741a.h"

#define _DEBUG
#include "platform/platform_stdio.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define IOP_RXCTRLBLK_ADVINDEX(INDX) INDX = (++INDX == IOP_RXCTRLBLK_COUNT) ? 0 : INDX

#define QBG_APPREADY_MILLISMAX 5000


// private function declarations
static cbuf_t *txCreate();
static void txDestroy();
static uint16_t txPut(const char *data, uint16_t dataSz);
static uint16_t txTake(char *data, uint16_t dataSz);
static void txSendChunk();

static uint8_t rxOpenCtrlBlock();
static void requestIrd(socketId_t socketId);
static void rxParseImmediate(uint8_t rxIndx);
static void rxParseDeferred(uint8_t rxIndx);
static uint16_t rxAllocateBuffer(iopRxCtrlBlock_t *rxCtrlStruct, uint8_t prefixSz);
static void rxResetCtrlBlock(uint8_t bufIndx);

static void interruptCallbackISR();


/* public functions
------------------------------------------------------------------------------------------------ */
#pragma region public functions


/**
 *	\brief Initialize the Input/Output Process subsystem.
 */
iop_t *iop_create()
{
    iop_t *iop = calloc(1, sizeof(iop_t));
	if (iop == NULL)
	{
        ltem1_faultHandler(0, "iop-could not alloc iop status struct");
	}
    iop->txBuf = txCreate();
    iop->rdsMode = iopRdsMode_idle;
    iop->rdsSocket = iopProcess_void;
    iop->rdsRxCtrlBlk = IOP_RXCTRLBLK_VOID;

    for (size_t i = 0; i < IOP_RXCTRLBLK_COUNT; i++)
    {
        iop->rxCtrlBlks[i].process = iopProcess_void;
    }
    return iop;
}



/**
 *	\brief Tear down IOP (Input/Output Process) subsystem.
 */
void iop_destroy(void *iop)
{
    txDestroy();
	free(iop);
}



/**
 *	\brief Complete initialization and start running iop processes.
 */
void iop_start()
{
    // attach ISR and enable NXP interrupt mode
    gpio_attachIsr(g_ltem1->gpio->irqPin, true, gpioIrqTriggerOn_falling, interruptCallbackISR);
    spi_protectFromInterrupt(g_ltem1->spi, g_ltem1->gpio->irqPin);
    sc16is741a_enableIrqMode();
}



/**
 *	\brief Verify LTEm1 is ready for IOP operations.
 */
void iop_awaitAppReady()
{
    unsigned long apprdyWaitStart = timing_millis();
    while (g_ltem1->qbgReadyState < qbg_readyState_appReady)
    {
        iop_recvDoWork();                               // ready state parsing in in rxParseDeferred() accessible via doWork()
        timing_yield();
        if (apprdyWaitStart + QBG_APPREADY_MILLISMAX < timing_millis())
            ltem1_faultHandler(500, "qbg-BGx module failed to start in the allowed time");
    }
}



/**
 *	\brief Start a (raw) send operation.
 *
 *  \param [in] sendData Pointer to char data to send out, input buffer can be discarded following call.
 *  \param [in] sendSz The number of characters to send.
 *  \param [in] sendImmediate If false, do not initiate new TX session, more TX data expected.
 */
void iop_txSend(const char *sendData, uint16_t sendSz, bool deferSend)
{
    uint16_t queuedSz = txPut(sendData, sendSz);
    if (queuedSz == sendSz)
    {
        if ( !deferSend )
            txSendChunk();
    }
    else
        ltem1_faultHandler(500, "iop-tx buffer overflow");
}




/**
 *	\brief [private] Response parser looking for ">" prompt to send data to network and then initiates the send.
 */
actionResult_t iop_txDataPromptParser(const char *response)
{
    if (strstr(response, "> ") != NULL)
        return ACTION_RESULT_SUCCESS;

    return ACTION_RESULT_PENDING;
}



/**
 *	 \brief Dequeue received data.
 *
 *   \param[in] recvBuf Pointer to the command receive buffer.
 *   \param[in] recvMaxSz The command response max length (buffer size).
 */
iopXfrResult_t iop_rxGetCmdQueued(char *recvBuf, uint16_t recvBufSz)
{
    iop_recvDoWork();

    if ( !IOP_RXCTRLBLK_ISOCCUPIED(g_ltem1->iop->cmdHead) )
        return iopXfrResult_incomplete;

    uint8_t tail = g_ltem1->iop->cmdTail;
    do
    {
        if (IOP_RXCTRLBLK_ISOCCUPIED(tail) && g_ltem1->iop->rxCtrlBlks[tail].process == iopProcess_command)
        {
            uint16_t copySz = MIN(recvBufSz, g_ltem1->iop->rxCtrlBlks[tail].primDataSz);
            strncpy(recvBuf, g_ltem1->iop->rxCtrlBlks[tail].primBuf, copySz);
            rxResetCtrlBlock(tail);
            recvBuf += copySz;
            recvBufSz -= copySz;                    // tally remaining buffer

            if (g_ltem1->iop->rxCtrlBlks[tail].primDataSz - copySz > 0)
                return iopXfrResult_truncated;
        }


        if (tail == g_ltem1->iop->cmdHead)          // done
            break;

        tail = IOP_RXCTRLBLK_ADVINDEX(tail);
        if (tail == g_ltem1->iop->cmdTail)
            ltem1_faultHandler(500, "iop_rxGetCmdQueued()-failed to find cmd data ");

    } while (true);
    g_ltem1->iop->cmdTail = tail;
    
    return iopXfrResult_complete;
}


/**
 *   \brief Transfer received data from iop buffers to application.
 * 
 *   \param[in] recvBuf Pointer to the command receive buffer.
 *   \param[in] recvMaxSz The command response max length (buffer size).
 * 
 *   \return The number of bytes received (since previous call).
*/
uint16_t iop_rxGetSocketQueued(socketId_t socketId, char **dataPtr, char *rmtHost, char *rmtPort)
{
    iop_recvDoWork();

    int8_t tail = g_ltem1->iop->socketTail[socketId];
    *rmtHost = NULL;
    *rmtPort = NULL;

    if ( !(IOP_RXCTRLBLK_ISOCCUPIED(tail) && g_ltem1->iop->rxCtrlBlks[g_ltem1->iop->socketTail[socketId]].dataReady) )
        return iopXfrResult_incomplete;

    /* FUTURE - Incoming UDP/TCP sockets are minimally supported by network operators, 
    * Verizon and Sprint Curiosity require static IP ($$), Hologram requires a custom proxy 
    * that really isn't even a UDP/TCP socket receiver approach. 
    **/
    /* receive data is prefixed by rmt host IP address, then port number (as strings)
    if (g_ltem1->iop->rxCtrlBlks[tail].rmtHostInData) {}
    */

    // determine which buffer (primary, extension) to return pointer to
    if (g_ltem1->iop->rxCtrlBlks[tail].extsnBufHead)
    {
        uint16_t dataSz = g_ltem1->iop->rxCtrlBlks[tail].extsnBufTail - g_ltem1->iop->rxCtrlBlks[tail].extsnBufHead;
        // extended buffer in use, combine parts: copy primary to start of extended buffer
        memcpy(g_ltem1->iop->rxCtrlBlks[tail].extsnBufHead, g_ltem1->iop->rxCtrlBlks[tail].primBuf, g_ltem1->iop->rxCtrlBlks[tail].primDataSz);
        *dataPtr = &g_ltem1->iop->rxCtrlBlks[tail].extsnBufHead;
        return dataSz;
    }
    else
    {
        *dataPtr = g_ltem1->iop->rxCtrlBlks[tail].primBufData;
        return g_ltem1->iop->rxCtrlBlks[tail].primDataSz;
    }
}



/**
 *   \brief Closes the tail (control block) of the socket stream as consumed.
 * 
 *   \param[in] The socket number to close tail buffer and update controls.
*/
void iop_tailFinalize(socketId_t socketId)
{
    rxResetCtrlBlock(g_ltem1->iop->socketTail[socketId]);

    while (g_ltem1->iop->socketTail[socketId] != g_ltem1->iop->socketHead[socketId])
    {
        g_ltem1->iop->socketTail[socketId] = IOP_RXCTRLBLK_ADVINDEX(g_ltem1->iop->socketTail[socketId]);

        if (IOP_RXCTRLBLK_ISOCCUPIED(g_ltem1->iop->socketTail[socketId]) && g_ltem1->iop->rxCtrlBlks[g_ltem1->iop->socketTail[socketId]].process == socketId)
        {
            g_ltem1->iop->socketTail[socketId] = g_ltem1->iop->socketTail[socketId];            // at next socket rxCtrlBlk
            return;
        }
    };
}



/**
 *	\brief Perform deferred work on IOP RX ctrl block data.
 */
void iop_recvDoWork()
{
    while (g_ltem1->iop->rxTail != g_ltem1->iop->rxHead)                        // spin through rxCtrlBlks to parse any outstanding recv'd messages
    {
        g_ltem1->iop->rxTail = IOP_RXCTRLBLK_ADVINDEX(g_ltem1->iop->rxTail);

        if (g_ltem1->iop->rxCtrlBlks[g_ltem1->iop->rxTail].process == iopProcess_allocated)
        {
            rxParseDeferred(g_ltem1->iop->rxTail);
        }
        if (g_ltem1->iop->rxCtrlBlks[g_ltem1->iop->rxTail].dataReady && g_ltem1->iop->rxCtrlBlks[g_ltem1->iop->rxTail].process <= iopProcess_socketMax)
        {
            g_ltem1->protocols->sockets[g_ltem1->iop->rxCtrlBlks[g_ltem1->iop->rxTail].process].hasData = true;
        }
    }
}


#pragma endregion


/* private local (static) functions
------------------------------------------------------------------------------------------------ */
#pragma region private functions


static cbuf_t *txCreate()
{
    cbuf_t *txBuf = calloc(1, sizeof(cbuf_t));
    if (txBuf == NULL)
        return NULL;

    txBuf->buffer = calloc(1, IOP_TX_BUFFER_SZ);
    if (txBuf->buffer == NULL)
    {
        free(g_ltem1->iop->txBuf);
        return NULL;
    }
    txBuf->maxlen = IOP_TX_BUFFER_SZ;
    return txBuf;
}



static void txDestroy()
{
    free(g_ltem1->iop->txBuf->buffer);
    free(g_ltem1->iop->txBuf);
}



/**
 *  \brief Puts data into the TX buffer control structure.
 * 
 *  \param [in] data Pointer to where source data is now.
 *  \param [in] dataSz How much data to put in the TX struct.
 * 
 *  \return The number of bytes of stored into the TX struct, compare to dataSz to validate results. 
*/
static uint16_t txPut(const char *data, uint16_t dataSz)
{
    uint16_t result = 0;

    for (size_t i = 0; i < dataSz; i++)
    {
        if(cbuf_push(g_ltem1->iop->txBuf, data[i]))
        {
            result++;
            continue;
        }
        break;
    }
    return result;
}



/**
 *  \brief Gets data from the TX buffer control structure.
 * 
 *  \param [in] data Pointer to where taken data will be placed.
 *  \param [in] dataSz How much data to take, if possible.
 * 
 *  \return The number of bytes of data being returned. 
*/
static uint16_t txTake(char *data, uint16_t dataSz)
{
    uint16_t result = 0;
    
    for (size_t i = 0; i < dataSz; i++)
    {
        if (cbuf_pop(g_ltem1->iop->txBuf, data + i))
        {
            result++;
            continue;
        }
        break;
    }
    return result;
}



/**
 *	\brief Test for send active, if not start a new send flow with chunk.
 */
static void txSendChunk()
{
    // if TX buffer empty, start a TX flow
    // otherwise, TX is underway and ISR will continue servicing queue until emptied
    uint8_t txAvail = sc16is741a_readReg(SC16IS741A_TXLVL_ADDR);

    if (txAvail == SC16IS741A_FIFO_BUFFER_SZ)           // bridge buffer is empty, no "in-flight" TX chars
    {
        char txData[65] = {0};
        uint16_t dataAvail = txTake(txData, txAvail);

        if (dataAvail > 0)
        {
            PRINTF(dbgColor_dCyan, "txChunk=%s\r", txData);
            sc16is741a_write(txData, dataAvail);
        }
    }
    // otherwise, just return nothing to do

    // uint8_t thisTxSz = MIN(g_ltem1->iop->txCtrl->remainSz, fifoAvailable);
    // sc16is741a_write(g_ltem1->iop->txCtrl->chunkPtr, thisTxSz);

    // // update TX ctrl with remaining work to complete requested transmit
    // g_ltem1->iop->txCtrl->remainSz -= thisTxSz;
    // g_ltem1->iop->txCtrl->chunkPtr = g_ltem1->iop->txCtrl->chunkPtr + thisTxSz;
    // //g_ltem1->iop->txCtrl->txActive = g_ltem1->iop->txCtrl->remainSz > 0;
}



/**
 *	\brief Scan IOP Buffers structure for next available read destination slot (from BG to host).
 *
 *  \return Index of read buffer location to receive the incoming chars.
 */
static uint8_t rxOpenCtrlBlock()
{
    uint8_t bufIndx = g_ltem1->iop->rxHead;
    do
    {
        bufIndx = IOP_RXCTRLBLK_ADVINDEX(bufIndx);
        if (bufIndx == g_ltem1->iop->rxHead)
            ltem1_faultHandler(500, "iop-rxOpenCtrlBlock()-no ctrlBlk available");
    }
    while (IOP_RXCTRLBLK_ISOCCUPIED(bufIndx));

    g_ltem1->iop->rxCtrlBlks[bufIndx].process = iopProcess_allocated;
    return g_ltem1->iop->rxHead = bufIndx;
}



/**
 *	\brief Clear and dispose of IOP control stuct resources.
 *
 *	\param [in] bufferIndex The index into the buffer control struct to close.
 */
static void rxResetCtrlBlock(uint8_t bufIndx)
{
    iopRxCtrlBlock_t *rxCtrlBlk = &g_ltem1->iop->rxCtrlBlks[bufIndx];

    rxCtrlBlk->process = iopProcess_void;
    memset(rxCtrlBlk->primBuf, 0, IOP_RXCTRLBLK_PRIMBUF_SIZE + 1);
    if (rxCtrlBlk->extsnBufHead != NULL)
    {
        free(rxCtrlBlk->extsnBufHead);
        rxCtrlBlk->extsnBufHead = NULL;
        rxCtrlBlk->extsnBufTail = NULL;
    }
    rxCtrlBlk->dataReady = false;
}



/**
 *   \brief Scan (in ISR) recv'd 1st stream chunk and prepare expansion buffer for remainder of incoming chunks.
 *
 *   \param [in] rxCtrlStruct Pointer to receive control/buffer structure.
 *   \param [in] dataSzAt Index in the receive stream to the start of size information 
 */
static uint16_t rxConfigureIrdBuffer(iopRxCtrlBlock_t *rxCtrlBlk, uint8_t dataSzAt)
{
    /* Note: protocol receive data messaging (like AT+QIRD=#) indicates incoming data size.
    *        This function parses to determine buffer needs to bring the data in.
    * 
    *  Example: +QIRD: 4,  where 4 is the number of chars arriving
    *     if chars arriving <= to primbuf size: all good, read will fit in primary
    *     if chars arriving > primbuf size, need to setup expandbuf (malloc)
    */

    char *dataSzPtr = rxCtrlBlk->primBuf + dataSzAt;
    uint16_t dataLen = strtol(dataSzPtr, &rxCtrlBlk->primBufData, 10);

    rxCtrlBlk->primDataSz = dataLen;

    // setup extended buffer if required
    if (dataLen > IOP_RXCTRLBLK_PRIMBUF_IRDCAP)
    {
        rxCtrlBlk->extsnBufHead = calloc(dataLen, sizeof(uint8_t));
        // leave room to copy primBuf into extended buffer, at copy of recvd data to application
        rxCtrlBlk->extsnBufTail = rxCtrlBlk->extsnBufHead + rxCtrlBlk->primDataSz;
    }
    return dataLen;
}



/**
 *  \brief Invoke IRD command to request BGx for socket (read) data
*/
static void requestIrd(socketId_t socketId)
{
    g_ltem1->iop->rdsSocket = socketId;
    
    char irdCmd[12] = {0};
    snprintf(irdCmd, 12, "AT+QIRD=%d", socketId);
    if ( !action_tryInvoke(irdCmd, false) )
        PRINTF(dbgColor_warn, "IRD DEFERRED");
}



/**
 *	\brief Scan (in ISR) the received data to determine the buffer contents type and process owner.
 *
 *  \param[in\out] rxCtrlStruct receive control/buffer structure.
 */
static void rxParseImmediate(uint8_t rxIndx)
{
    /*  ** Known Header Patterns
    //
    //     Area/Msg Prefix     I/D    Immediate: here  // Deferred: iop_recvDoWork()
    //
        // -- BG96 init
        // \r\nAPP RDY\r\n      D   -- BG96 completed firmware initialization
        // 
        // -- Commands
        // +QPING:              D   -- PING response (instance and summary header)
        // +QIURC: "dnsgip"     D   -- DNS lookup reply
        //
        // -- Protocols
        // +QIURC: "recv",      D   -- "unsolicited response" proto tcp/udp
        // +QIRD: #             I   -- "read data" response 
        // +QSSLURC: "recv"     D   -- "unsolicited response" proto ssl tunnel
        // +QHTTPGET:           D   -- GET response, HTTP-READ 
        // CONNECT<cr><lf>      I   -- HTTP Read
        // +QMTSTAT:            D   -- MQTT state change message recv'd
        // +QMTRECV:            I   -- MQTT subscription data message recv'd
        // 
        // -- Async Status Change Messaging
        // +QIURC: "pdpdeact"   D   -- network pdp context timed out and deactivated

        // default content type is command response
    */

    #define IRDRECV_HDRSZ 7
    #define SSLRECV_HDRSZ 11
    #define MQTTRECV_HDRSZ 10

    iopRxCtrlBlock_t *rxCtrlBlk = &g_ltem1->iop->rxCtrlBlks[rxIndx];

    // udp/tcp/ssl
    if (strncmp("+QIRD: ", rxCtrlBlk->primBuf + 2, IRDRECV_HDRSZ) == 0 ||
        strncmp("+QSSLRECV: ", rxCtrlBlk->primBuf + 2, SSLRECV_HDRSZ) == 0)
    {
        // msg is a response to a QIRD/QSSLRECV action, complete it
        g_ltem1->action->cmdBrief[0] = ASCII_cNULL;

        // setup buffer destination
        uint16_t irdBytes = rxConfigureIrdBuffer(rxCtrlBlk, ((rxCtrlBlk->primBuf[4] == 'I') ? IRDRECV_HDRSZ : SSLRECV_HDRSZ) + 2);
        if (irdBytes == 0)                                                      // special IRD response: empty IRD signals end of data
        {
            g_ltem1->iop->rdsMode = iopRdsMode_idle;
            g_ltem1->iop->rdsSocket = iopProcess_void;
            rxCtrlBlk->process = iopProcess_void;
            return;
        }

        rxCtrlBlk->rmtHostInData = (*rxCtrlBlk->primBufData == ASCII_cCOMMA);   // signal remote IP addr available, if socket opened as UDP service

        rxCtrlBlk->primBufData += 2;                                            // skip CrLf 
        rxCtrlBlk->process = g_ltem1->iop->rdsSocket;

        if (rxCtrlBlk->extsnBufHead == NULL)
        {
            rxCtrlBlk->dataReady = true;
            g_ltem1->iop->rdsMode = iopRdsMode_idle;
        }
        else                                                                    // extended buffer opened, setup continuation properties
        {
            rxCtrlBlk->dataReady = false;
            g_ltem1->iop->rdsMode = iopRdsMode_irdBytes;
            g_ltem1->iop->rdsBytes = irdBytes;
            g_ltem1->iop->rdsRxCtrlBlk = rxIndx;
        }
        g_ltem1->iop->socketHead[g_ltem1->iop->rdsSocket] = rxIndx;             // data present, so point head to this ctrlBlk
    }

    /* mqtt >> +QMTRECV: <tcpconnectID>,<msgID>,<topic>,<payload>
     *         +QMTRECV: 0,0, "topic/example", "This is the payload related to topic" */
    else if  (strncmp("+QMTRECV: ", rxCtrlBlk->primBuf + 2, MQTTRECV_HDRSZ) == 0)
    {
        // allocate extended buffer to hold topic and payload
        rxCtrlBlk->extsnBufHead = calloc(MQTT_URC_PREFIXSZ + MQTT_SUBTOPIC_MAXSZ + MQTT_MESSAGE_MAXSZ + 6, sizeof(uint8_t));
        rxCtrlBlk->extsnBufTail = rxCtrlBlk->extsnBufHead + rxCtrlBlk->primDataSz;
        rxCtrlBlk->dataReady = false;

        g_ltem1->iop->rdsMode = iopRdsMode_eotPhrase;
        strcpy(g_ltem1->iop->rdsEotPhrase, ASCII_sMQTTTERM);
        g_ltem1->iop->rdsEotSz = 3;
        char* termTestAt = rxCtrlBlk->primBuf + rxCtrlBlk->primDataSz - g_ltem1->iop->rdsEotSz;
        if (strncmp(g_ltem1->iop->rdsEotPhrase, termTestAt, g_ltem1->iop->rdsEotSz))
        {
            rxCtrlBlk->dataReady = true;
            g_ltem1->iop->rdsMode = iopRdsMode_idle;
        }
    }
}



/**
 *  \brief Complete parsing of RX control blocks that was deferred from ISR. 
*/
static void rxParseDeferred(uint8_t rxIndx)
{
    #define RECV_HEADERSZ_URC_IPRECV 13
    #define RECV_HEADERSZ_URC_SSLRECV 15
    #define QIURC_HDRSZ 8

    iopRxCtrlBlock_t *rxCtrlBlock = &g_ltem1->iop->rxCtrlBlks[rxIndx];
    // preserve comparison for dbg
    uint8_t ipCmp, sslCmp;

    if ((ipCmp = memcmp("+QIURC: \"recv", rxCtrlBlock->primBuf + 2, RECV_HEADERSZ_URC_IPRECV)) == 0)
    {
        char *connIdPtr = rxCtrlBlock->primBuf + RECV_HEADERSZ_URC_IPRECV;
        char *endPtr = NULL;
        uint8_t socketId = (uint8_t)strtol(connIdPtr, &endPtr, 10);

        requestIrd(socketId);
        g_ltem1->iop->socketTail[socketId] = rxIndx;
        rxResetCtrlBlock(rxIndx);
    }

    else if ((sslCmp  = memcmp("+QSSLURC: \"recv", rxCtrlBlock->primBuf + 2, RECV_HEADERSZ_URC_SSLRECV)) == 0)
    {
        char *connIdPtr = rxCtrlBlock->primBuf + RECV_HEADERSZ_URC_SSLRECV;
        char *endPtr = NULL;
        uint8_t socketId = (uint8_t)strtol(connIdPtr, &endPtr, 10);

        requestIrd(socketId);
        g_ltem1->iop->socketTail[socketId] = rxIndx;
        rxResetCtrlBlock(rxIndx);
    }

    // catch async unsolicited state changes here
    //  +QIURC: "pdpdeact",<contextID>
    else if (memcmp("+QIURC: ", rxCtrlBlock->primBuf + 2, QIURC_HDRSZ) == 0)
    {
        // state message buffer is only 1 message
        if (g_ltem1->iop->urcStateMsg[0] != NULL)
            ltem1_faultHandler(500, "IOP-URC state msg buffer overflow.");
        strncpy(g_ltem1->iop->urcStateMsg, rxCtrlBlock->primBuf + QIURC_HDRSZ, IOP_URC_STATEMSG_SZ);
        rxCtrlBlock->process = iopProcess_void;                         // release ctrl/buffer, processed here
    }

    else if (g_ltem1->qbgReadyState != qbg_readyState_appReady && memcmp("APP RDY\r\n", rxCtrlBlock->primBuf + 2, 9) == 0)
    {
        PRINTF(dbgColor_white, "\rQBG-AppRdy\r");
        g_ltem1->qbgReadyState = qbg_readyState_appReady;
        rxCtrlBlock->process = iopProcess_void;                         // release ctrlBlk, done with it
    }

    // if rx process still unassigned, set as command
    else
    {
        g_ltem1->iop->cmdHead = rxIndx;
        rxCtrlBlock->process = iopProcess_command;
    }
}



/**
 *	\brief ISR for NXP bridge interrupt events; primary (1st bridge chunk) read/write actions to LTEm1.
 */
static void interruptCallbackISR()
{
    /* ----------------------------------------------------------------------------------------------------------------
     * NOTE: The IIR, TXLVL and RXLVL are read seemingly redundantly, this is required to ensure NXP SC16IS741
     * IRQ line is reset (belt AND suspenders).  During initial testing it was determined that without this 
     * duplication of register reads IRQ would latch active randomly.
     * ------------------------------------------------------------------------------------------------------------- */
    /*
    * IIR servicing:
    *   read (RHR) : buffer full (need to empty), timeout (chars recv'd, buffer not full but no more coming)
    *   write (THR): buffer emptied sufficiently to send more chars
    */

    SC16IS741A_IIR iirVal;
    uint8_t rxLevel;
    uint8_t txAvailable;

    iirVal.reg = sc16is741a_readReg(SC16IS741A_IIR_ADDR);
    retryIsr:
    PRINTF(dbgColor_white, "\rISR[");

    do
    {
        while(iirVal.IRQ_nPENDING == 1)
        {
            iirVal.reg = sc16is741a_readReg(SC16IS741A_IIR_ADDR);
            PRINTF(dbgColor_warn, "*");
        }

        // if (iirVal.IRQ_nPENDING == 1)
        // {
        //     // PRINTF_WARN("** ");
        //     // IRQ fired, so re-read IIR **VERIFY** nothing to service & reset source
        //     iirVal.reg = sc16is741a_readReg(SC16IS741A_IIR_ADDR);
        // }


        // priority 1 -- receiver line status error : clear fifo of bad char
        if (iirVal.IRQ_SOURCE == 3)
        {
            PRINTF(dbgColor_error, "RXErr ");
            sc16is741a_flushRxFifo();
        }


        // priority 2 -- receiver RHR full (src=2), receiver time-out (src=6)
        // Service Action: read RXLVL, read FIFO to empty
        if (iirVal.IRQ_SOURCE == 2 || iirVal.IRQ_SOURCE == 6)
        {
            PRINTF(dbgColor_gray, "RX=%d ", iirVal.IRQ_SOURCE);
            rxLevel = sc16is741a_readReg(SC16IS741A_RXLVL_ADDR);
            PRINTF(dbgColor_gray, "-lvl=%d ", rxLevel);

            if (rxLevel > 0)
            {
                uint8_t rxIndx = (g_ltem1->iop->rdsMode == iopRdsMode_idle) ? rxOpenCtrlBlock() : g_ltem1->iop->rdsRxCtrlBlk;
                PRINTF(dbgColor_gray, "-ix=%d ", rxIndx);

                if (g_ltem1->iop->rdsMode == iopRdsMode_idle)           // if rdsMode is idle, 1st rx chunk: primary buffer
                {
                    sc16is741a_read(g_ltem1->iop->rxCtrlBlks[rxIndx].primBuf, rxLevel);
                    g_ltem1->iop->rxCtrlBlks[rxIndx].primDataSz = rxLevel;
                    g_ltem1->iop->rxCtrlBlks[rxIndx].dataReady = true;
                }
                else                                                    // else, extended buffer
                {
                    sc16is741a_read(g_ltem1->iop->rxCtrlBlks[rxIndx].extsnBufTail, rxLevel);
                    g_ltem1->iop->rxCtrlBlks[rxIndx].extsnBufTail += rxLevel;

                    if (g_ltem1->iop->rdsMode == iopRdsMode_irdBytes)
                    {
                        g_ltem1->iop->rdsBytes -= rxLevel;
                        g_ltem1->iop->rxCtrlBlks[rxIndx].dataReady = (g_ltem1->iop->rdsBytes == 0);         // dataReady if all bytes read
                        g_ltem1->iop->rdsMode = iopRdsMode_idle;
                    }
                    else
                    {
                        char* termTestAt = g_ltem1->iop->rxCtrlBlks[rxIndx].primBuf - g_ltem1->iop->rdsEotSz;
                        if (strncmp(g_ltem1->iop->rdsEotPhrase, termTestAt, g_ltem1->iop->rdsEotSz))
                        {
                            g_ltem1->iop->rxCtrlBlks[rxIndx].dataReady = true;
                            g_ltem1->iop->rdsMode = iopRdsMode_idle;
                        }
                    }
                    
                }
                PRINTF(dbgColor_cyan, "\r%s\r", g_ltem1->iop->rxCtrlBlks[rxIndx].primBuf);

                // parse and update IOP RX ctrl block
                rxParseImmediate(rxIndx);

                if (g_ltem1->iop->rxCtrlBlks[rxIndx].dataReady)
                {
                    g_ltem1->iop->rdsMode = iopRdsMode_idle;
                    g_ltem1->action->cmdBrief[0] = NULL;                              // close IRD action (regardless of RDS mode)
                }
            }
        }


        // priority 3 -- transmit THR (threshold) : TX ready for more data
        if (iirVal.IRQ_SOURCE == 1)                                         // TX available
        {
            uint8_t buf[65] = {0};
            uint8_t thisTxSz;

            txAvailable = sc16is741a_readReg(SC16IS741A_TXLVL_ADDR);
            PRINTF(dbgColor_gray, "TX ");
            PRINTF(dbgColor_gray, "-lvl=%d ", txAvailable);

            if ((thisTxSz = txTake(buf, txAvailable)) > 0)
            {
                PRINTF(dbgColor_dCyan, "txChunk=%s", buf);
                sc16is741a_write(buf, thisTxSz);
            }

            // if (g_ltem1->iop->txCtrl->remainSz > 0)
            // {
            //     txSendNextChunk(txAvailable);
            //     PRINTF_GRAY("-chunk ");
            // }
        }

        /* -- NOT USED --
        // priority 4 -- modem interrupt
        // priority 6 -- receive XOFF/SpecChar
        // priority 7 -- nCTS, nRTS state change:
        */

        iirVal.reg = sc16is741a_readReg(SC16IS741A_IIR_ADDR);

    } while (iirVal.IRQ_nPENDING == 0);

    PRINTF(dbgColor_white, "]\r");

    gpioPinValue_t irqPin = gpio_readPin(g_ltem1->gpio->irqPin);
    if (irqPin == gpioValue_low)
    {
        txAvailable = sc16is741a_readReg(SC16IS741A_TXLVL_ADDR);
        rxLevel = sc16is741a_readReg(SC16IS741A_RXLVL_ADDR);
        iirVal.reg = sc16is741a_readReg(SC16IS741A_IIR_ADDR);
        PRINTF(dbgColor_warn, "IRQ failed to reset!!! nIRQ=%d, iir=%d, txLvl=%d, rxLvl=%d \r", iirVal.IRQ_nPENDING, iirVal.reg, txAvailable, rxLevel);

        //ltem1_faultHandler(500, "IRQ failed to reset.");
        goto retryIsr;
    }
}


#pragma endregion

