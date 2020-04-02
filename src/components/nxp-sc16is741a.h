/******************************************************************************
 *  \file sc16is741a.h
 *  \author Jensen Miller, Greg Terrell
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
#ifndef __NXP_SC16IS741A_H__
#define __NXP_SC16IS741A_H__

#ifdef __cplusplus
extern "C"
{
#include <cstdint>
#include <cstdlib>
#include <cstdbool>
#else
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#endif // __cplusplus

#include "../platform/platformSpi.h"



#define SC16IS741A_FIFO_MAX         0x40U
#define SC16IS741A_FIFO_RnW_READ    0x01U
#define SC16IS741A_FIFO_RnW_WRITE   0x00U

typedef const uint8_t ro8;
typedef uint8_t rw8;


/**
 *	\brief A NXP SC16IS741A Serial bridge device (SPI<=>UART on LTEm1)
 */
typedef struct sc16is741a_device_tag
{
	spi_device spi;
	uint8_t tx_buffer[SC16IS741A_FIFO_MAX + 2];
	uint8_t rx_buffer[SC16IS741A_FIFO_MAX + 2];
} sc16is741a_device_t;

typedef sc16is741a_device_t* sc16is741a_device;


#define DEF_SC16IS741A_REG(REG_NAME, REG_BITS) \
typedef union \
{ \
    struct { \
    REG_BITS \
    }; \
    uint8_t reg; \
} SC16IS741A_##REG_NAME;


typedef enum
{
    SC16IS741A_FIFO_ADDR = 0x00U,
    SC16IS741A_IER_ADDR = 0x01U,
    SC16IS741A_FCR_ADDR = 0x02U,
    SC16IS741A_IIR_ADDR = 0x02U,
    SC16IS741A_LCR_ADDR = 0x03U,
    SC16IS741A_MCR_ADDR = 0x04U,
    SC16IS741A_LSR_ADDR = 0x05U,
    SC16IS741A_MSR_ADDR = 0x06U,
    SC16IS741A_SPR_ADDR = 0x07U,
    SC16IS741A_TCR_ADDR = 0x06U,
    SC16IS741A_TLR_ADDR = 0x07U,
    SC16IS741A_TXLVL_ADDR = 0x08U,
    SC16IS741A_RXLVL_ADDR = 0x09U,
    SC16IS741A_UARTRST_ADDR = 0x0EU,
    SC16IS741A_EFCR_ADDR = 0x0FU,
    SC16IS741A_DLL_ADDR = 0x00U,
    SC16IS741A_DLH_ADDR = 0x01U,
    SC16IS741A_EFR_ADDR = 0x02U,
    SC16IS741A_XON1_ADDR = 0x04U,
    SC16IS741A_XON2_ADDR = 0x05U,
    SC16IS741A_XOFF1_ADDR = 0x06U,
    SC16IS741A_XOFF2_ADDR = 0x07U
} sc16is741a_reg_addr;


/**
 *  \brief Interrupt enable register.
 */
DEF_SC16IS741A_REG(IER,
    rw8 RX_DATA_AVAIL_INT_EN : 1;
    rw8 THR_EMPTY_INT_EN : 1;
    rw8 RECEIVE_LINE_STAT_INT_EN : 1;
    rw8 MDM_STAT_INT_EN : 1;
    rw8 SLP_MODE_EN : 1;
    rw8 nXOFF_INT_EN : 1;
    rw8 nRTS_INT_EN : 1;
    rw8 nCTS_INT_EN : 1;
)        


/**
 *  \brief FIFO control register.
 */
DEF_SC16IS741A_REG(FCR,
    rw8 FIFO_EN : 1;
    rw8 RX_FIFO_RST : 1;
    rw8 TX_FIFO_RST : 1;
    ro8 : 1;
    rw8 TX_TRIGGER_LVL_LSB : 1;
    rw8 TX_TRIGGER_LVL_MSB : 1;
    rw8 RX_TRIGGER_LVL_LSB : 1;
    rw8 RX_TRIGGER_LVL_MSB : 1;
)


typedef enum
{
    TX_LVL_8SPACES = 0b00U,
    TX_LVL_16SPACES = 0b01U,
    TX_LVL_32SPACES = 0b10U,
    TX_LVL_56SPACES = 0b11U
} sc16is741a_fcr_tx_trigger_val;


typedef enum
{
    RX_LVL_8CHARS = 0b00U,
    RX_LVL_16CHARS = 0b01U,
    RX_LVL_56CHARS = 0b10U,
    RX_LVL_60CHARS = 0b11U
} sc16is741a_fcr_rx_trigger_val;


/**
 *  \brief Interrupt indicator register.
 */
DEF_SC16IS741A_REG(IIR,
    ro8 INT_STAT : 1;
    ro8 INT_PRIORITY : 5;
    ro8 FIFO_EN : 2;
)


/**
 *  \brief Line control register.
 */
DEF_SC16IS741A_REG(LCR, 
    rw8 WORD_LEN : 2;
    rw8 STOP : 1;
    rw8 PARITY_EN : 1;
    rw8 EVEN_PARITY : 1;
    rw8 SET_PARITY : 1;
    rw8 SET_BREAK : 1;
    rw8 DIVISOR_LATCH_EN : 1;
)


/**
 *  \brief Modem control register.
 */
DEF_SC16IS741A_REG(MCR,
    ro8 : 1;
    rw8 nRTS : 1;
    rw8 TCR_TLR_EN : 1;
    ro8 : 1;
    rw8 LOOPBACK_EN : 1;
    rw8 XON_ANY : 1;
    rw8 IRDA_MODE_EN : 1;
    rw8 CLOCK_DIVISOR : 1;
)


/**
 *  \brief Line status register.
 */
DEF_SC16IS741A_REG(LSR,
    ro8 DATA_IN_RECVR : 1;
    ro8 OVERRUN_ERROR : 1;
    ro8 PARITY_ERROR : 1;
    ro8 FRAMING_ERROR : 1;
    ro8 BREAK_INT : 1;
    ro8 THR_EMPTY : 1;
    ro8 THR_TSR_EMPTY : 1;
    ro8 FIFO_DATA_ERROR : 1;
)


/**
 *  \brief Modem status register.
 */
DEF_SC16IS741A_REG(MSR,
    ro8 DELTA_CTS : 1;
    ro8 : 3;
    ro8 CTS : 1;
    ro8 : 3;
)


/**
 *  \brief Scratch pad register.
 */
DEF_SC16IS741A_REG(SPR,
    rw8 DATA;
)


/**
 *  \brief UART software reset.
 */
DEF_SC16IS741A_REG(UARTRST,
    ro8 : 3;
    rw8 UART_SWRST : 1;
)


/**
 *  \brief
 */
DEF_SC16IS741A_REG(EFCR,
    rw8 MODE_9BIT_EN : 1;
    rw8 RECVR_DISABLE : 1;
    rw8 TRANSMITTER_DISABLE : 1;
    ro8 : 1;
    rw8 AUTO_RS_485_RTS_DIR_CTRL : 1;
    rw8 AUTO_RS_485_RTS_OUTPUT_INV : 1;
    ro8 : 1;
    rw8 IRDA_MODE : 1;
)


/**
 *  \brief
 */
DEF_SC16IS741A_REG(EFR, 
    rw8 SWFLOW_CTRL : 4;
    rw8 ENHANCED_FNS_EN : 1;
    rw8 SPECIAL_CHAR_DETECT : 1;
    rw8 AUTO_nRTS : 1;
    rw8 AUTO_nCTS : 1;
)

#define SC16IS741A_REG_SET_GENERAL 0x00
#define SC16IS741A_REG_SET_SPECIAL 0x80
#define SC16IS741A_REG_SET_ENHANCED 0xBF


sc16is741a_device sc16is741a_init(uint8_t chipSelLine, uint32_t spiClockSpeed, uint32_t uartBaudrate);
void sc16is741a_uninit(sc16is741a_device bridge);

int sc16is741a_write(sc16is741a_device bridge, const void * src, size_t src_len);
int sc16is741a_read(sc16is741a_device bridge, void* dest, size_t dest_len, size_t * read_in);

void sc16is741a_regWrite(sc16is741a_device bridge, uint8_t reg_addr, const uint8_t * reg_data);
void sc16is741a_regRead(sc16is741a_device bridge, uint8_t reg_addr, uint8_t* reg_data);
void sc16is741a_enableFifo(sc16is741a_device bridge, bool enable);
void sc16is741a_setUartBaudrate(sc16is741a_device bridge);

#ifdef __cplusplus
}
#endif // __cplusplus


#endif  /* !__NXP_SC16IS741A_H__ */