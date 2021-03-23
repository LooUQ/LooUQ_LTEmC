/******************************************************************************
 *  \file cbuf.c
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
 *****************************************************************************/

#include "cbuf.h"

/**
 *  \brief Pushes a character on to stack buffer.
 * 
 *  \param bufStruct [in] - The destination buffer.
 *  \param data [in] - Character to add to buffer.
 * 
 *  \return true (1) if added, 0 if not added to buffer. 
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
 *  \brief Pops a character from stack buffer.
 * 
 *  \param bufStruct [in] - The destination buffer.
 *  \param data [in] - Pointer where to place popped character.
 * 
 *  \return true (1) if popped, 0 if not returned. 
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

