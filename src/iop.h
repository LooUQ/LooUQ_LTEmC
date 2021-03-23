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

#define CBUF_SZ  1749       // cmd 
#include "cbuf.h"

#define IOP_SOCKET_COUNT 6
#define IOP_ERROR -1

// up to 20 bytes, depends on data size reported from 1-1460 bytes:: \r\n+QIRD: ####\r\n<data>\r\nOK\r
#define IOP RX_IRD_OVRHD_SZ 21
#define IOP RX_IRD_TRAILER_SZ 8

#define IOP_TX_BUFFER_SZ 1460
#define IOP_URC_STATEMSG_SZ 80

#define IOP_RXCTRLBLK_ISOCCUPIED(INDX) (g_ltem1->iop->rxCtrlBlks[INDX].process != iopProcess_void)


/* IOPv2 new objects
-------------------------------------------------------------------------------------- */

#define IOP_RX_DATABUFFERS_MAX 3
#define IOP_RX_CMDBUF_SZ 256
#define IOP_RX_DATABUF_SZ 2048
#define IOP_NO_BUFFER 255


typedef enum iopDataPeer_tag
{
    iopDataPeer_SOCKET_0 = 0,
    iopDataPeer_SOCKET_1 = 1,
    iopDataPeer_SOCKET_2 = 2,
    iopDataPeer_SOCKET_3 = 3,
    iopDataPeer_SOCKET_4 = 4,
    iopDataPeer_SOCKET_5 = 5,

    iopDataPeer_MQTT = 6,
    iopDataPeer_HTTP = 7,
    iopDataPeer_FTP = 8,

    iopDataPeer__SOCKET = 0,
    iopDataPeer__SOCKET_CNT = 6,
    iopDataPeer__TABLESZ = iopDataPeer_FTP + 1,

    iopDataPeer__NONE = 255
} iopDataPeer_t;


typedef struct peerTypeMap_tag      // Remote data sources, 1 indicates active session (session can source an URC event)
                                    // a 1 bit in the member bytes indicates an active session partner (network connection)
                                    // - Some peers have only a single possible partner; so bit position not relevant
                                    // - Some peers have multiple session partners (sockets); these represent the partner by bit position 
                                    //    example (sockets): bit 0 = socket 0 open, bit 1 = socket 1 open.
                                    // A bit "set" in tcpudpSocket and sslSocket are mutually exclusive
                                    // BGx uses distinct AT command syntax for TCP/UDP and SSL
{
    uint8_t pdpContext;             // bit-map of open network pdp contexts (cellular network)
    uint8_t tcpudpSocket;           // bit-map of open TCP or UDP sockets
    uint8_t sslSocket;              // bit-map of open SSL sockets
    uint8_t mqttConnection;         // bool of MQTT server connection (only one connection to manage\monitor)
    uint8_t mqttSubscribe;          // bool of MQTT topic subscription, incoming message (only one incoming message receiver currently supported)
} peerTypeMap_t;    


/** 
 *  \brief Struct for a IOP smart buffer. Contains the char buffer and controls to marshall data between IOP and consumer (cmd,sockets,mqtt,etc.).
 * 
 *  Head and tail reference incoming (head) and outgoing (tail). IOP loads head, consumers (commands,sockets,mqtt,etc.) read from tail.
*/
typedef struct iopBuffer_tag
{
    char *buffer;               ///< data buffer, does not change while used.
    char *bufferEnd;            ///< end of physical buffer
    char *head;                 ///< fill (in)
    char *prevHead;             ///< if the last chunk is copied or consumed immediately used to restore head
    char *tail;                 ///< consumer (out)
    iopDataPeer_t dataPeer;     ///< data owner, peer sourcing this data
    uint16_t irdSz;             ///< the number of expected bytes (sockets: reported by BGx IRD message)
    bool dataReady;             ///< EOT (End-Of-Transmission) reached, either # of expected bytes received or EOT char sequence detected
} iopBuffer_t;


/** 
 *  \brief Struct for a IOP transmit (TX) buffer control block. Tracks progress of chunk sends to LTEm1.
 * 
 *  LTEm1 SPI bridge works with chunks of ~64 bytes (actual transfers are usually 58 - 62 bytes). IOP abstracts SPI chunks from senders.
*/
typedef struct iopTxCtrlBlock_tag
{
    char *txBuf;                ///< Pointer to the base address of the TX buffer. Fixed, doesn't change with operations.
    char *chunkPtr;             ///< Pointer to the next "chunk" of data to send to modem.
    size_t remainSz;            ///< Remaining number of bytes in buffer to send to modem.
} iopTxCtrlBlock_t;


#define IOP_TX_ACTIVE() g_ltem1->iop->txCtrl->remainSz > 0


/** 
 *  \brief Struct for the IOP subsystem state. During initialization a pointer to this structure is reference in g_ltem1.
*/
typedef struct iop_tag
{
    cbuf_t *txBuf;                                      ///< transmit buffer (there is just one)
    uint16_t txPend;                                    ///< outstanding TX char pending
    iopBuffer_t *rxCmdBuf;                              ///< command receive buffer, this is the default RX buffer
    iopDataPeer_t rxDataPeer;                           ///< protocol data source: if no peer, IOP is in command mode
    uint8_t rxDataBufIndx;                              ///< data goes into this slot rxDataBufs
    iopBuffer_t *rxDataBufs[IOP_RX_DATABUFFERS_MAX];    ///< the data buffers (smart buffer structs)
    peerTypeMap_t peerTypeMap;                          ///< map (struct) of possible IOP peers (data sources), used to optimise ISR string scanning
} iop_t;

typedef iop_t *iopPtr_t ;


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


void iop_create();
void iop_registerProtocol(ltem1OptnModule_t proto, void *protoPtr);

void iop_start();
void iop_awaitAppReady();

uint16_t iop_txSend(const char *sendData, uint16_t sendSz, bool sendReady);
void iop_rxParseImmediate();
void iop_resetCmdBuffer();

resultCode_t iop_txDataPromptParser(const char *response, char **endptr);


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__IOP_H__ */
