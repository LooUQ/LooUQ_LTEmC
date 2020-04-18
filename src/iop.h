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

#define IOP_RD_BUFFERS_COUNT 6
#define IOP_RD_BUFFERS_PRIMARY_BUFFER_SIZE 65
#define IOP_RD_BUFFER_EMPTY -1
#define IOP_RD_BUFFER_NOTFOUND -1


typedef enum
{
    iop_process_protocol_0 = 0,
    iop_process_protocol_1 = 1,
    iop_process_protocol_2 = 2,
    iop_process_protocol_3 = 3,
    iop_process_protocol_4 = 4,
    iop_process_protocol_5 = 5,
    iop_process_command = 9
} iop_processes_t; 


typedef struct iop_rd_buffer_tag
{
    bool occupied;
    uint8_t next;
    char primBuf[65];
    iop_processes_t process;
    bool isIrd;
    char *expBufHead;
    char *expBufTail;
} iop_rd_buffer_t;


typedef volatile struct iop_status_tag
{
    int8_t readLast;
    int8_t cmdHead;
    int8_t protoHead[IOP_PROTOCOLS_COUNT];
    iop_rd_buffer_t rdBufs[IOP_RD_BUFFERS_COUNT];
} iop_status_t;



#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


iop_status_t *iop_create();
void iop_start();
void iop_destroy();

void iop_closeRdBuffer(uint8_t bufferIndex);


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__IOP_H__ */
