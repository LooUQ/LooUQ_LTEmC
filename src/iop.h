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
 *****************************************************************************/

#ifndef __IOP_H__
#define __IOP_H__

#include <stdint.h>
#include "network.h"
#include "cbuf.h"
#include "ltem1c.h"

#define IOP_SOCKET_COUNT 6
#define IOP_ERROR -1

#define IOP_RXCTRLBLK_COUNT 8
#define IOP_RXCTRLBLK_VOID 255
#define IOP_RXCTRLBLK_PRIMBUF_SIZE 64
#define IOP_RXCTRLBLK_PRIMBUF_IRDCAP 43
// up to 20 bytes, depends on data size reported from 1-1460 bytes:: \r\n+QIRD: ####\r\n<data>\r\nOK\r
#define IOP RX_IRD_OVRHD_SZ 21
#define IOP RX_IRD_TRAILER_SZ 8

#define IOP_TX_BUFFER_SZ 1460
#define IOP_URC_STATEMSG_SZ 80

#define IOP_RXCTRLBLK_ISOCCUPIED(INDX) (g_ltem1->iop->rxCtrlBlks[INDX].process != iopProcess_void)


typedef enum
{
    iopProcess_socket_0 = 0,
    iopProcess_socket_1 = 1,
    iopProcess_socket_2 = 2,
    iopProcess_socket_3 = 3,
    iopProcess_socket_4 = 4,
    iopProcess_socket_5 = 5,
    iopProcess_socketMax = 5,
    iopProcess_command = 9,
    iopProcess_allocated = 254,
    iopProcess_void = 255
} iopProcess_t; 


typedef enum
{
    iopXfrResult_complete = 1,
    iopXfrResult_incomplete = 0,
    iopXfrResult_busy = -1,
    iopXfrResult_truncated = -2
} iopXfrResult_t;



typedef struct iopTxCtrlBlock_tag
{
    //bool txActive;
    char *txBuf;
    char *chunkPtr;
    size_t remainSz;
} iopTxCtrlBlock_t;

#define IOP_TX_ACTIVE() g_ltem1->iop->txCtrl->remainSz > 0

typedef enum iopRdsMode_tag
{
    iopRdsMode_idle = 0,
    iopRdsMode_irdBytes = 1,
    iopRdsMode_eotPhrase = 2
} iopRdsMode_t;


typedef struct iopRxCtrlBlock_tag
{
    iopProcess_t process;
    char primBuf[65];
    char *primBufData;
    uint8_t primDataSz;
    bool rmtHostInData;
    char *extsnBufHead;
    char *extsnBufTail;
    bool dataReady;
} iopRxCtrlBlock_t;



typedef struct iop_tag
{
    uint8_t rxHead;
    uint8_t rxTail;
    uint8_t cmdHead;
    uint8_t cmdTail;
    uint8_t socketHead[IOP_SOCKET_COUNT];
    uint8_t socketTail[IOP_SOCKET_COUNT];
    cbuf_t *txBuf;
    iopRdsMode_t rdsMode;
    uint8_t rdsSocket;
    uint8_t rdsRxCtrlBlk;
    uint16_t rdsBytes;
    char rdsEotPhrase[5];
    uint8_t rdsEotSz;
    iopRxCtrlBlock_t rxCtrlBlks[IOP_RXCTRLBLK_COUNT];
    char urcStateMsg[IOP_URC_STATEMSG_SZ];
} iop_t;


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


iop_t *iop_create();
void iop_destroy();

void iop_start();
void iop_awaitAppReady();

void iop_txSend(const char *sendData, uint16_t sendSz, bool deferTx);
actionResult_t iop_txDataPromptParser(const char *response);

iopXfrResult_t iop_rxGetCmdQueued(char *recvData, uint16_t recvSz);
uint16_t iop_rxGetSocketQueued(socketId_t socketId, char **data, char *rmtHost, char *rmtPort);
void iop_tailFinalize(socketId_t socketId);
void iop_recvDoWork();


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__IOP_H__ */
