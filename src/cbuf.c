/******************************************************************************
 *  \file txbuf.c
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
 *****************************************************************************/

#include "cbuf.h"


uint8_t cbuf_push(cbuf_t *bufCtrl, uint8_t data)
{
    int next;
    next = bufCtrl->head + 1;         // updt control
    if (next >= bufCtrl->maxlen)
        next = 0;

    if (next == bufCtrl->tail)        // if full, not pushing
        return 0;

    bufCtrl->buffer[bufCtrl->head] = data;
    bufCtrl->head = next;
    return 1;
}



uint8_t cbuf_pop(cbuf_t *bufCtrl, uint8_t *data)
{
    int next;
    if (bufCtrl->head == bufCtrl->tail)     // if the head == tail, we don't have any data
        return 0;

    next = bufCtrl->tail + 1;         // next is where tail will point to after this read.
    if(next >= bufCtrl->maxlen)
        next = 0;

    *data = bufCtrl->buffer[bufCtrl->tail];
    bufCtrl->tail = next;
    return 1;
}

