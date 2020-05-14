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
#include "network/network.h"
#include "ltem1c.h"

#define IOP_SOCKET_COUNT 6
#define IOP_ERROR -1

#define IOP_RX_CTRLBLOCK_COUNT 8
#define IOP_RX_PRIMARY_BUFFER_SIZE 64

#define IOP_TX_BUFFER_SZ 1460
#define IOP_URC_STATEMSG_SZ 20


typedef enum
{
    iopProcess_socket_0 = 0,
    iopProcess_socket_1 = 1,
    iopProcess_socket_2 = 2,
    iopProcess_socket_3 = 3,
    iopProcess_socket_4 = 4,
    iopProcess_socket_5 = 5,
    iopProcess_command = 9,
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

typedef struct iopRxCtrlBlock_tag
{
    bool occupied;
    // int8_t next;
    uint8_t primSz;
    char primBuf[65];
    iopProcess_t process;
    // bool isURC;
    char *dataStart;
    char *extsnBufHead;
    char *extsnBufTail;
} iopRxCtrlBlock_t;



typedef volatile struct iop_tag
{
    //iop_state_t iopState;
    int8_t rxRecvHead;
    int8_t cmdHead;
    int8_t cmdTail;
    int8_t socketHead[IOP_SOCKET_COUNT];
    int8_t socketTail[IOP_SOCKET_COUNT];
    uint8_t irdSocket;
    int16_t socketIrdBytes[IOP_SOCKET_COUNT];
    iopRxCtrlBlock_t rxCtrlBlks[IOP_RX_CTRLBLOCK_COUNT];
    iopTxCtrlBlock_t *txCtrl;
    char urcStateMsg[IOP_URC_STATEMSG_SZ];
} iop_t;



#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


iop_t *iop_create();
void iop_start();
void iop_destroy();

void iop_txSend(const char *sendData, uint16_t sendSz);

iopXfrResult_t iop_rxGetCmdQueued(char *recvData, uint16_t recvSz);
uint16_t iop_rxGetSocketQueued(socket_t socketNm, char *recvBuf, uint16_t recvMaxSz);

void iop_tailFinalize(socket_t socketNm);

//void iop_closeRxCtrl(uint8_t bufIndx);


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__IOP_H__ */
