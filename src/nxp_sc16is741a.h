/******************************************************************************
 *  \file nxp_sc16is741a.h
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

#include "platform/platform_spi.h"

#pragma region structures

#define SC16IS741A_FIFO_BUFFER_SZ   0x40
#define SC16IS741A_FIFO_RnW_READ    0x01
#define SC16IS741A_FIFO_RnW_WRITE   0x00

#define NXP_TX_FIFOSZ 0x40
#define NXP_RX_FIFOSZ 0x40

// NXP Bridge register set selector values (applied to LCR register)
#define SC16IS741A_REG_SET_GENERAL  0x00
#define SC16IS741A_REG_SET_SPECIAL  0x80
#define SC16IS741A_REG_SET_ENHANCED 0xBF

#define SC16IS741A_HW_RESET_DELAY      1U
#define SC16IS741A_SW_RESET_MASK       0x08


typedef const uint8_t ro8;
typedef uint8_t rw8;


/**
 *	\brief SC16IS741A First SPI byte for register addressing.
 *
 *	This byte tells the SPI slave what register to access and
 *	whether this operation is a read or write.
 */
union __sc16is741a_reg_addr_byte__
{
	struct
	{
		ro8 : 1;
		rw8 CH0 : 1;
		rw8 CH1 : 1;
		rw8 A : 4;
		rw8 RnW : 1;
	};
	uint8_t reg_address;
};



/**
 *	\brief SC16IS741A SPI bytes containing address and register value.
 */
union __sc16is741a_reg_payload__
{
	struct
	{
		union __sc16is741a_reg_addr_byte__ reg_addr;
		uint8_t reg_data;
	};
	uint16_t reg_payload;
};



// /**
//  *	\brief A NXP SC16IS741A Serial bridge device (SPI<=>UART on LTEm1)
//  */
// typedef struct sc16is741a_device_tag
// {
// 	spi_device_t *spi;
// } sc16is741a_device_t;

//typedef sc16is741a_device_t* sc16is741a_device;


#define DEF_SC16IS741A_REG(REG_NAME, REG_BITS) \
typedef union \
{ \
    struct { \
    REG_BITS \
    }; \
    uint8_t reg; \
} SC16IS741A_##REG_NAME;


/** 
 *  \brief Enum representing the register addresses of the NXP SPI bridge chip on the LTEm1.
 * 
 *  For details about registers see the NXP SC16IS741A data sheet.
*/
typedef enum
{
    SC16IS741A_FIFO_ADDR = 0x00U,               ///< FIFO data register, accesses the TX/RX buffers
    SC16IS741A_IER_ADDR = 0x01U,                ///< Interrupt enable register
    SC16IS741A_FCR_ADDR = 0x02U,                ///< Frame control register
    SC16IS741A_IIR_ADDR = 0x02U,                ///< Interrupt identification register
    SC16IS741A_LCR_ADDR = 0x03U,                ///< Line control register
    SC16IS741A_MCR_ADDR = 0x04U,                ///< Modem control register
    SC16IS741A_LSR_ADDR = 0x05U,                ///< Line state register
    SC16IS741A_MSR_ADDR = 0x06U,                ///< Modem status register
    SC16IS741A_SPR_ADDR = 0x07U,                ///< Scratchpad register (test loopback)
    SC16IS741A_TCR_ADDR = 0x06U,                ///< Transmission control register
    SC16IS741A_TLR_ADDR = 0x07U,                ///< Trigger level register
    SC16IS741A_TXLVL_ADDR = 0x08U,              ///< TX level register
    SC16IS741A_RXLVL_ADDR = 0x09U,              ///< RX level register
    SC16IS741A_UARTRST_ADDR = 0x0EU,            ///< UART reset
    SC16IS741A_EFCR_ADDR = 0x0FU,               ///< Extra features register
    SC16IS741A_DLL_ADDR = 0x00U,                ///< Divisor latch register (LSB)
    SC16IS741A_DLH_ADDR = 0x01U,                ///< Divisor latch register (MSB)
    SC16IS741A_EFR_ADDR = 0x02U,                ///< Enhanced features register
    SC16IS741A_XON1_ADDR = 0x04U,               ///< XON-1 word 
    SC16IS741A_XON2_ADDR = 0x05U,               ///< XON-2 word
    SC16IS741A_XOFF1_ADDR = 0x06U,              ///< XOFF-1 word
    SC16IS741A_XOFF2_ADDR = 0x07U               ///< XOFF-2 word
} sc16is741a_reg_addr;


/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
*
*  bit-order may be affected by compiler and platform for DEF_SC16IS741A_REG 
*  register structs below. These are composed as bit [0] first.
*
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */


/**
 *  \brief Interrupt enable register.
 */
DEF_SC16IS741A_REG(IER,
    rw8 RHR_DATA_AVAIL_INT_EN : 1;
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
    rw8 TX_TRIGGER_LVL : 2;
    rw8 RX_TRIGGER_LVL : 2;
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
 *  \brief Decode for Interrupt indicator register priority bits.
 */
typedef enum
{
    IRQ_1_RCVR_STATUS_ERROR = 0x03U,
    IRQ_2_RCVR_TIMEOUT = 0x06U,
    IRQ_2_RCVR_RHR = 0x02U,
    IRQ_3_XMIT_THR = 0x01U,
    IRQ_4_MODEM = 0x00U,
    IRQ_6_XOFF = 0x08U,
    IRQ_7_CTSRTS = 0x10U
} sc16is741a_irq_priority_val;


/**
 *  \brief Interrupt indicator register.
 */
DEF_SC16IS741A_REG(IIR,
    ro8 IRQ_nPENDING : 1;
    ro8 IRQ_SOURCE : 5;
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

#define NXP_LSR_DATA_IN_RECVR 0x01U
#define NXP_LSR_THR_EMPTY 0x02U
#define NXP_LSR_FIFO_DATA_ERROR 0x80U

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
 *  \brief Modem status register.
 */
DEF_SC16IS741A_REG(TLR,
    rw8 TX_TRIGGER_LVL : 4;
    rw8 RX_TRIGGER_LVL : 4;
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

typedef enum
{
  resetFifo_action_Rx = 0x02U,
  resetFifo_action_Tx = 0x04U,
  resetFifo_action_RxTx = 0x06U
} resetFifo_action_t;


#pragma endregion


// sc16is741a_device_t *sc16is741a_create(uint8_t chipSelLine, uint32_t spiClockSpeed, uint32_t uartBaudrate);
// void sc16is741a_uninit();
void sc16is741a_start();
void sc16is741a_enableIrqMode();
bool sc16is741a_chkCommReady();

void sc16is741a_write(const void * src, uint8_t src_len);
void sc16is741a_read(void* dest, uint8_t dest_len);

void sc16is741a_writeReg(uint8_t reg_addr, uint8_t reg_data);
uint8_t sc16is741a_readReg(uint8_t reg_addr);

void sc16is741a_resetFifo(resetFifo_action_t resetAction);
void sc16is741a_flushRxFifo();

void displayFifoStatus(const char *dispMsg);


#ifdef __cplusplus
}
#endif // __cplusplus


#endif  /* !__NXP_SC16IS741A_H__ */