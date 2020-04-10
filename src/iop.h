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

#define IOP_PROTOCOLS_MAX 5
#define IOP_PROTOCOLS_SIZE (IOP_PROTOCOLS_MAX + 1)
#define IOP_ERROR -1

#define IOP_READ_BUFFERS_MAX 5
#define IOP_READ_BUFFERS_SIZE (IOP_READ_BUFFERS_MAX + 1)
#define IOP_READ_BUFFERS_PRIMARY_BUFFER_SIZE 60
#define IOP_READ_BUFFERS_END -1



typedef enum
{
	iop_process_notclassified = -1,
    iop_process_protocol_0 = 0,
    iop_process_protocol_1 = 1,
    iop_process_protocol_2 = 2,
    iop_process_protocol_3 = 3,
    iop_process_protocol_4 = 4,
    iop_process_protocol_5 = 5,
    iop_process_command = 9
} iop_processes_t; 


typedef struct iop_readbuffer_descr_tag
{
    bool occupied;
    uint8_t next;
    char primaryBuf[60];
    iop_processes_t ownerProc;
    char *expandBufHead;
    char *expandBufTail;
} iop_readbuffer_descr_t;


//static ltem1_device g_ltem1;


typedef struct iop_status_tag
{
    int8_t readLast;
    int8_t cmdLast;
    int8_t protoLast[IOP_PROTOCOLS_SIZE];
    bool cmdPending;
    bool recvPending;
    bool qirdPending;
    iop_readbuffer_descr_t g_readBuffers[IOP_READ_BUFFERS_SIZE];
} iop_status_t;



#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


void iop_init(ltem1_device ltem1);
void iop_uinit();

void iop_closeBuffer(uint8_t bufferIndex);
void iop_classifyReadBuffers();
void iop_irqCallback_bridge();


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__IOP_H__ */
