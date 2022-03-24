/******************************************************************************
 *  \file ltemc-cbuf.c
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020,2021 LooUQ Incorporated.
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

#include "ltemc-cbuf.h"

/**
 *  @brief Pushes a character on to stack buffer.
*/
uint8_t cbuf_push(cbuf_t *bufStruct, uint8_t data)
{
    int next = (bufStruct->head + 1) % bufStruct->maxlen;   // updt control
    // if (next >= bufStruct->maxlen)
    //     next = 0;
    if (next == bufStruct->tail)                            // if full, not pushing
        return 0;

    bufStruct->buffer[bufStruct->head] = data;
    bufStruct->head = next;
    return 1;
}


/**
 *  @brief Pops a character from stack buffer.
*/
uint8_t cbuf_pop(cbuf_t *bufStruct, uint8_t *data)
{
    if (bufStruct->head == bufStruct->tail)                 // if the head == tail, we don't have any data
        return 0;

    int next = (bufStruct->tail + 1) % bufStruct->maxlen;   // next is where tail will point to after this read.
    // if(next >= bufStruct->maxlen)
    //     next = 0;

    *data = bufStruct->buffer[bufStruct->tail];
    bufStruct->tail = next;
    return 1;
}
