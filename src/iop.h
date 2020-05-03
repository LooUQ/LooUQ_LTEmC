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
#include "ltem1c.h"

#define IOP_PROTOCOLS_COUNT 6
#define IOP_ERROR -1

#define IOP_RX_CTRLBLOCK_COUNT 6
#define IOP_RX_PRIMARY_BUFFER_SIZE 65
#define IOP_EMPTY -1

#define IOP_TX_BUFFER_SZ 1500


typedef enum
{
    iop_process_notAssigned = -1,
    iop_process_protocol_0 = 0,
    iop_process_protocol_1 = 1,
    iop_process_protocol_2 = 2,
    iop_process_protocol_3 = 3,
    iop_process_protocol_4 = 4,
    iop_process_protocol_5 = 5,
    iop_process_command = 9
} iop_process_t; 


typedef enum
{
    iop_rx_result_ready = 1,
    iop_rx_result_nodata = 0,
    iop_rx_result_truncated = -1
} iop_rx_result_t;


typedef struct iop_txCtrlBlock_tag
{
    bool sendActive;
    char *txBuf;
    char *nextChunk;
    size_t sendSz;
} iop_txCtrlBlock_t;


typedef struct iop_rxCtrlBlock_tag
{
    bool occupied;
    uint8_t next;
    uint8_t primSz;
    char primBuf[65];
    iop_process_t process;
    bool isIrd;
    char *expBufHead;
    char *expBufTail;
} iop_rxCtrlBlock_t;


typedef volatile struct iop_state_tag
{
    int8_t rxTailIndx;
    int8_t rxCmdHeadIndx;
    int8_t rxProtoHeadIndx[IOP_PROTOCOLS_COUNT];
    iop_rxCtrlBlock_t rxCtrls[IOP_RX_CTRLBLOCK_COUNT];
    iop_txCtrlBlock_t txCtrl;
} iop_state_t;



#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


iop_state_t *iop_create();
void iop_start();
void iop_destroy();

bool iop_txClearToSend();
bool iop_txSend(const char *sendData, size_t sendSz);

iop_rx_result_t iop_rxGetQueued(iop_process_t process, char *recvData, size_t recvSz);
//void iop_closeRxCtrl(uint8_t bufIndx);


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__IOP_H__ */
