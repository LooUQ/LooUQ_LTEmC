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
#include "protocols/protocols.h"
#include "ltem1c.h"

#define IOP_PROTOCOLS_COUNT 6
#define IOP_ERROR -1

#define IOP_RX_CTRLBLOCK_COUNT 8
#define IOP_RX_PRIMARY_BUFFER_SIZE 64
#define IOP_EMPTY -1

#define IOP_TX_BUFFER_SZ 1460
#define IOP_URC_STATEMSG_SZ 20


typedef enum
{
    iop_state_idle = 0,
    iop_state_actionPending = 1,
    iop_state_txPending = 2
} iop_state_t;


typedef enum
{
    iop_process_protocol_0 = 0,
    iop_process_protocol_1 = 1,
    iop_process_protocol_2 = 2,
    iop_process_protocol_3 = 3,
    iop_process_protocol_4 = 4,
    iop_process_protocol_5 = 5,
    iop_process_command = 9,
    iop_process_void = 255
} iop_process_t; 


typedef enum
{
    iop_rx_result_ready = 1,
    iop_rx_result_nodata = 0,
    iop_rx_result_truncated = -1
} iop_rxGetResult_t;


typedef struct iop_txCtrlBlock_tag
{
    char *txBuf;
    char *chunkPtr;
    size_t remainSz;
} iop_txCtrlBlock_t;


typedef struct iop_rxCtrlBlock_tag
{
    bool occupied;
    // int8_t next;
    uint8_t primSz;
    char primBuf[65];
    iop_process_t process;
    bool isURC;
    char *dataStart;
    char *extsnBufHead;
    char *extsnBufTail;
} iop_rxCtrlBlock_t;



typedef volatile struct iop_tag
{
    int8_t rxRecvHead;
    iop_state_t iopState;
    int8_t cmdHead;
    int8_t cmdTail;
    int8_t socketHead[IOP_PROTOCOLS_COUNT];
    int8_t socketTail[IOP_PROTOCOLS_COUNT];
    uint8_t irdSocket;
    int16_t socketIrdBytes[IOP_PROTOCOLS_COUNT];
    iop_rxCtrlBlock_t rxCtrlBlks[IOP_RX_CTRLBLOCK_COUNT];
    iop_txCtrlBlock_t txCtrl;
    char urcStateMsg[IOP_URC_STATEMSG_SZ];
} iop_t;



#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


iop_state_t *iop_create();
void iop_start();
void iop_destroy();

bool iop_txClearToSend();
bool iop_txSend(const char *sendData, uint16_t sendSz);

iop_rxGetResult_t iop_rxGetCmdQueued(char *recvData, uint16_t recvSz);
uint16_t iop_rxGetSocketQueued(socket_t socketNm, char *recvBuf, uint16_t recvMaxSz);

void iop_tailFinalize(socket_t socketNm);

//void iop_closeRxCtrl(uint8_t bufIndx);


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__IOP_H__ */
