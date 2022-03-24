/******************************************************************************
 *  \file ltemc-cbuf.h
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

#ifndef __LTEMC_CBUF_H__
#define __LTEMC_CBUF_H__

#include <stdint.h>

/** 
 *  @brief Circular buffer for use in the transmit functions.
*/
typedef struct {
    uint8_t * buffer;       ///< The internal char buffer storing the text
    int head;               ///< Integer offset to the position where the next char will be added (pushed).
    int tail;               ///< Integer offset to the consumer position, where the next request will be sourced.
    int maxlen;             ///< The total size of the buffer. 
} cbuf_t;


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

/**
 *  @brief Pushes a character on to stack buffer.
 *  @param bufStruct [in] - The destination buffer.
 *  @param data [in] - Character to add to buffer.
 *  @return true (1) if added, 0 if not added to buffer. 
*/
uint8_t cbuf_push(cbuf_t *c, uint8_t data);


/**
 *  @brief Pops a character from stack buffer.
 *  @param bufStruct [in] - The destination buffer.
 *  @param data [in] - Pointer where to place popped character.
 *  @return true (1) if popped, 0 if not returned. 
*/
uint8_t cbuf_pop(cbuf_t *c, uint8_t *data);


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__LTEMC_CBUF_H__ */
