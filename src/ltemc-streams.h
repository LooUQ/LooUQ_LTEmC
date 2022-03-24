/******************************************************************************
 *  \file ltemc-streams.h
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020, 2021 LooUQ Incorporated.
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
 ******************************************************************************
 * LTEmC Data Streams: Cellular data or BGx file system
 *****************************************************************************/

#ifndef __LTEMC_STREAMS_H__
#define __LTEMC_STREAMS_H__

#include <lq-types.h>


/** 
 *  @brief Typed numeric constants for stream peers subsystem (sockets, mqtt, http)
 */
enum streams__constants
{
    streams__ctrlMagic = 0x5c
};


/** 
 *  @brief Enum of data stream peer types
 */
typedef enum dataStreamType_tag
{
    dataStream_none = 0,
    dataStream_sckt = 2,
    dataStream_mqtt = 3,
    dataStream_http = 4,
    dataStream_file = 5
} dataStreamType_tag;


/** 
 *  @brief Enum of protocols available on the modem (bit-mask). 
 *  @details All of the protocols are CLIENTS, while the BGx line of modules support server mode the network carriers generally don't
 */
typedef enum protocol_tag
{
    protocol_tcp = 0x00,                ///< TCP client
    protocol_udp = 0x01,                ///< UDP client
    protocol_ssl = 0x02,                ///< SSL client.
    protocol_socket = 0x03,             ///< special test value, < compare includes any of the above IP socket protocols

    protocol_mqtt = 0x10,               ///< MQTT messaging client
    protocol_http = 0x11,               ///< HTTP client

    protocol_void = 0xFF                ///< No protocol, used in code to generally signal a null condition.
} protocol_t;


/** 
 *  @brief Enum of the available data contexts for BGx (only SSL/TLS capable contexts are supported).
 */
typedef enum dataContext_tag
{
    dataContext_0 = 0,
    dataContext_1 = 1,
    dataContext_2 = 2,
    dataContext_3 = 3,
    dataContext_4 = 4,
    dataContext_5 = 5,
    dataContext__cnt = 6,
    dataContext__none = 0xFF
} dataContext_t;


/** 
 *  @brief Enum of data stream peers: network data contexts or the BGx file system
 *  @details Only data contexts that coincide with SSL contexts are supported.
 */
typedef enum streamPeer_tag
{
    streamPeer_dataCntxt0 = 0,
    streamPeer_dataCntxt1 = 1,
    streamPeer_dataCntxt2 = 2,
    streamPeer_dataCntxt3 = 3,
    streamPeer_dataCntxt4 = 4,
    streamPeer_dataCntxt5 = 5,
    streamPeer_file = 6,
    streamPeer_cnt = 7
} streamPeer_t;


/** 
 *  @brief Receive buffer page. Component struct for the rxDataBufferCtrl_t.
 */
typedef volatile struct rxBufferPage_tag
{
    char *_buffer;           ///< base address of page buffer (fixed, does not change)
    char *head;              ///< filled data (in), available for next data in
    char *tail;              ///< data pointer (consumer out)
    char *prevHead;          ///< if the last chunk is copied or consumed immediately used to restore head
} rxBufferPage_t;


/** 
 *  @brief Struct for a IOP smart buffer. Contains the char buffer and controls to marshall data between IOP and consumer (cmd,sockets,mqtt,etc.).
 * 
 *  @details 
 *  bufferSync is a semphore to signal buffer page role swap underway. ISR will sync with this upon entering the RX critical section 
 * 
 *  - Receive consumers (doWork functions) wanting to swap RX buffer pages will set bufferSync
 *  - This will force ISR to check iopPg and _nextIopPg and complete swap if necessary
 *  - Once buffer page swap is done, bufferSync will be reset
 *  - If interrupt fires ISR will check bufferSync prior to servicing a RX event
 * 
 *  _doWork() [consumer] uses IOP_swapRxBufferPage(rxDataBufferCtrl_t *bufPtr), ISR uses IOP_isrCheckBufferSync()
 * 
 *  NOTE: the completion test methods both consider the whole buffer for RX complete, but split buffers are returned to the application as 
 *  each is filled or the entire RX is completed.
 */
typedef volatile struct rxDataBufferCtrl_tag
{
    // _ variables are set at initialization and don't change
    char *_buffer;              ///< data buffer, does not change while used.
    char *_bufferEnd;           ///< end of physical buffer
    uint16_t _bufferSz;         ///< effective buffer size (after split)
    uint16_t _pageSz;           ///< the size of split size
    bool _overflow;             ///< set in ISR if overflow detected

    bool bufferSync;            ///< set when page swap is underway
    uint8_t _nextIopPg;         ///< intended resulting iopPg value

    uint8_t iopPg;              ///< when in split mode, which buffer page is assigned to IOP for filling
    rxBufferPage_t pages[2];    ///< buffer page to support interwoven fill/empty operations
} rxDataBufferCtrl_t;


/** 
 *  @brief Struct for a single page IOP smart buffer. Used by commands (AT cmd) and for capturing BGx async events.
 */
typedef volatile struct rxCoreBufferCtrl_tag
{
    // _variables are set at initialization and don't change
    char *_buffer;              ///< data buffer, does not change while used.
    char *_bufferEnd;           ///< end of physical buffer
    uint16_t _bufferSz;         ///< effective buffer size (after split)
    bool _overflow;             ///< set in ISR if overflow detected; doWork() moves to _prevOverflow, notifies application, then clears

    char *tail;                 ///< consumer out pointer
    char *head;                 ///< data in pointer
    char *prevHead;             ///< if the last chunk is copied or consumed immediately used to restore head
} rxCoreBufferCtrl_t;


/** 
 *  @brief Struct for a IOP transmit (TX) buffer control block. Tracks progress of chunk sends to LTEm1.
 *  @details LTEm SPI bridge works with chunks of ~64 bytes (actual transfers are usually 58 - 62 bytes). IOP abstracts SPI chunks from senders.
 */
typedef struct txBufferCtrl_tag
{
    char *txBuf;                        ///< Pointer to the base address of the TX buffer. Fixed, doesn't change with operations.
    char *chunkPtr;                     ///< Pointer to the next "chunk" of data to send to modem.
    uint16_t remainSz;                    ///< Remaining number of bytes in buffer to send to modem.
} txBufferCtrl_t;


 /** 
 *  @brief Background work function signature.
 *  @details Most subsystems are linked to this struct with pointers to allow for better abstraction and optional subsystems
 */
typedef void (*moduleDoWorkFunc_t)();


/** 
 *  @brief Abstract base struct containing common properties required of a stream control
 */
typedef struct baseCtrl_tag
{
    uint8_t ctrlMagic;                  ///< magic flag to validate incoming requests 
    dataContext_t dataCntxt;            ///< Data context where this control operates
    protocol_t protocol;                ///< Socket's protocol : UDP/TCP/SSL.
    bool useTls;                        ///< flag indicating SSL/TLS applied to stream
    rxDataBufferCtrl_t recvBufCtrl;     ///< RX smart buffer 
} baseCtrl_t;


/** 
 *  @brief Abstract pointer type that is cast into the specific stream control.
 *  @details Example:  scktCtrl_t *sckt = (scktCtrl_t*)((iop_t*)g_ltem.iop)->streamPeers[((iop_t*)g_ltem.iop)->rxStreamPeer];
 */
typedef void * iopStreamCtrl_t;


/**
 *   @brief Brief inline static function to support doWork() readability
 */
static inline uint16_t rxPageDataAvailable(rxDataBufferCtrl_t *buf, uint8_t page)
{
    return buf->pages[page].head - buf->pages[page].tail;
}
                                        

#endif  /* !__LTEMC_STREAMS_H__ */
