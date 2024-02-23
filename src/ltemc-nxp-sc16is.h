/** ***************************************************************************
  @file 
  @brief UART control and transfer functions/services.

  @author Greg Terrell/Jensen Miller, LooUQ Incorporated

  \loouq

  @warning Internal dependencies, changes only as directed by LooUQ staff.

-------------------------------------------------------------------------------

LooUQ-LTEmC // Software driver for the LooUQ LTEm series cellular modems.
Copyright (C) 2017-2023 LooUQ Incorporated

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
Also add information on how to contact you by electronic and paper mail.

**************************************************************************** */


#ifndef __LTEMC_NXP_SC16IS_H__
#define __LTEMC_NXP_SC16IS_H__

// #ifdef __cplusplus
// #include <cstdint>
// #include <cstdlib>
// #include <cstdbool>
// #else
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
// #endif // __cplusplus

#include "ltemc-types.h"
//#include "platform/platform-spi.h"


#define DEF_SC16IS7xx_REG(REG_NAME, REG_BITS)   \
typedef union                                   \
{                                               \
    struct {                                    \
    REG_BITS                                    \
    };                                          \
    uint8_t reg;                                \
} SC16IS7xx_##REG_NAME;                                             ///< Shortcut for UART register access

#define AWAIT_READY_TRIES 5                                         ///< Restart sync attempts

#pragma region structures

typedef const uint8_t ro8;                                          ///< Read-only register
typedef uint8_t rw8;                                                ///< Read-write register


/**
 * @brief NXP UART register constants and command values
 */
enum SC16IS7xx__constants
{
    // BGx default baudrate is 115200, LTEm-OSC raw clock is 7.378MHz (SC16IS740 section 7.8)
    SC16IS7xx__DLL_baudClockDivisorLOW = 0x04U,
    SC16IS7xx__DLH_baudClockDivisorHIGH = 0x00U,

    // Bridge<>BG96 UART framing - 8 data, no parity, 1 stop (bits)
    SC16IS7xx__LCR_UARTframing = 0x03U,
    SC16IS7xx__LCR_break = 0x40U,
    //SC16IS7xx__SC16IS7xx_EFR_ENHANCED_FUNCTIONS = 0x10U

    // TLR Register [7:4] RX, [3:0] TX Interrupt trigger points (MCR[2]=1 and EFR[4]=1 required)
    // set field to level / 4 (buffer granularity is 4)
    // SC16IS7xx__TLR_TriggerLevels = 0x22U,						// **!** testing value - 8 char buffer **!**
    // SC16IS7xx__TLR_TRIGGER_LEVELS = 0xFFU					    // 0xF=15 (*4) = 60 char buffer

    // fcr is a WrOnly register, flush and FIFO enable are both in this register 
    SC16IS7xx__FCR_BASIC_MODE = 0xB7U,
    SC16IS7xx__FCR_IOP_FIFO_ENABLE = 0xB1U,
    SC16IS7xx__FCR_IOP_RX_FLUSH = 0x02U,
    SC16IS7xx__FCR_IOP_TX_FLUSH = 0x04U,

    SC16IS7xx__FIFO_bufferSz = 0x40,
    SC16IS7xx__FIFO_readRnW = 0x01,
    SC16IS7xx__FIFO_writeRnW = 0x00,

    SC16IS7xx__LCR_REGSET_general = 0x00,
    SC16IS7xx__LCR_REGSET_special = 0x80,
    SC16IS7xx__LCR_REGSET_enhanced = 0xBF,

    SC16IS7xx__HW_resetDelay = 1U,
    SC16IS7xx__SW_resetMask = 0x08,

    SC16IS7xx__LSR_RHR_dataReady = 0x01U,
    SC16IS7xx__LSR_THR_empty = 0x02U,
    SC16IS7xx__LSR_FIFO_dataError = 0x80U,
    SC16IS7xx__LSR_FIFO_overrun = 0x02U
};


/** 
 *  @brief NXP SC16IS740/750/760/741 register addresses, for details about registers see the NXP SC16IS740/750/760 data sheet.NXP SC16IS740/750/760/741 register addresses, for details about registers see the NXP SC16IS740/750/760 data sheet.
 */ 
typedef enum SC16IS7xx_regAddr_tag
{
    SC16IS7xx_FIFO_regAddr = 0x00U,               ///< FIFO data register, accesses the TX/RX buffers
    SC16IS7xx_IER_regAddr = 0x01U,                ///< Interrupt enable register
    SC16IS7xx_FCR_regAddr = 0x02U,                ///< Frame control register
    SC16IS7xx_IIR_regAddr = 0x02U,                ///< Interrupt identification register
    SC16IS7xx_LCR_regAddr = 0x03U,                ///< Line control register
    SC16IS7xx_MCR_regAddr = 0x04U,                ///< Modem control register
    SC16IS7xx_LSR_regAddr = 0x05U,                ///< Line state register
    SC16IS7xx_MSR_regAddr = 0x06U,                ///< Modem status register
    SC16IS7xx_SPR_regAddr = 0x07U,                ///< Scratchpad register (test loopback)
    SC16IS7xx_TCR_regAddr = 0x06U,                ///< Transmission control register
    SC16IS7xx_TLR_regAddr = 0x07U,                ///< Trigger level register
    SC16IS7xx_TXLVL_regAddr = 0x08U,              ///< TX level register
    SC16IS7xx_RXLVL_regAddr = 0x09U,              ///< RX level register
    SC16IS7xx_UARTRST_regAddr = 0x0EU,            ///< UART reset
    SC16IS7xx_EFCR_regAddr = 0x0FU,               ///< Extra features register
    SC16IS7xx_DLL_regAddr = 0x00U,                ///< Divisor latch register (LSB)
    SC16IS7xx_DLH_regAddr = 0x01U,                ///< Divisor latch register (MSB)
    SC16IS7xx_EFR_regAddr = 0x02U,                ///< Enhanced features register
    SC16IS7xx_XON1_regAddr = 0x04U,               ///< XON-1 word 
    SC16IS7xx_XON2_regAddr = 0x05U,               ///< XON-2 word
    SC16IS7xx_XOFF1_regAddr = 0x06U,              ///< XOFF-1 word
    SC16IS7xx_XOFF2_regAddr = 0x07U               ///< XOFF-2 word
}SC16IS7xx_regAddr_t;


// // BG96 default baudrate is 115200, LTEm1 raw clock is 7.378MHz (SC16IS741a pg13)
// #define BAUDCLOCK_DIVISOR_DLL 0x04U
// #define BAUDCLOCK_DIVISOR_DLH 0x00U

// // Bridge<>BG96 UART framing - 8 data, no parity, 1 stop (bits)
// #define SC16IS7xx_LCR_UART_FRAMING 0x03U
// //#define SC16IS7xx_EFR_ENHANCED_FUNCTIONS 0x10U

// // [7:4] RX, [3:0] TX - level / 4 (buffer granularity is 4)
// #define SC16IS7xx_TLR_TRIGGER_LEVELS 0x22U							// **!** testing value **!**
// //#define SC16IS7xx_TLR_TRIGGER_LEVELS 0xFFU					    // 15 (*4) = 60 char buffer

// // fcr is a RdOnly register, flush and FIFO enable are both in this register 
// #define FCR_REGISTER_VALUE_BASIC_MODE 0xB7U
// #define FCR_REGISTER_VALUE_IOP_FIFO_ENABLE 0xB1U
// #define FCR_REGISTER_VALUE_IOP_RX_FLUSH 0x02U
// #define FCR_REGISTER_VALUE_IOP_TX_FLUSH 0x04U



// #define SC16IS7xx_FIFO_BUFFER_SZ   0x40
// #define SC16IS7xx_FIFO_RnW_READ    0x01
// #define SC16IS7xx_FIFO_RnW_WRITE   0x00

// #define NXP_TX_FIFOSZ 0x40
// #define NXP_RX_FIFOSZ 0x40

// // NXP Bridge register set selector values (applied to LCR register)
// #define SC16IS7xx_REG_SET_GENERAL  0x00
// #define SC16IS7xx_REG_SET_SPECIAL  0x80
// #define SC16IS7xx_REG_SET_ENHANCED 0xBF

// #define SC16IS7xx_HW_RESET_DELAY      1U
// #define SC16IS7xx_SW_RESET_MASK       0x08


/**
 *	@brief SC16IS7xx FIFO buffer reset actions
 */
typedef enum sc16IS7xx_FifoResetAction_tag
{ 
    SC16IS7xx_FIFO_resetActionRx = 0x02U,
    SC16IS7xx_FIFO_resetActionTx = 0x04U,
    SC16IS7xx_FIFO_resetActionRxTx = 0x06U,
} sc16IS7xx_FifoResetAction_t;


/**
 *	@brief SC16IS741A First SPI byte for register addressing.
 *
 *	This byte tells the SPI slave what register to access and
 *	whether this operation is a read or write.
 */
union __SC16IS7xx_reg_addr_byte__
{
	struct
	{
		ro8 : 1;
		rw8 CH0 : 1;
		rw8 CH1 : 1;
		rw8 A : 4;
		rw8 RnW : 1;
	};
    uint8_t reg_address;                                            ///< Combined bits into a byte variable
};


/**
 *	@brief SC16IS741A SPI bytes containing address and register value.
 */
union __SC16IS7xx_reg_payload__
{
	struct
	{
		union __SC16IS7xx_reg_addr_byte__ reg_addr;
		uint8_t reg_data;
	};
	uint16_t reg_payload;                                           ///< Combined register address and data
};





/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
*
*  bit-order may be affected by compiler and platform for DEF_SC16IS7xx_REG 
*  register structs below. These are composed as bit [0] first.
*
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

/**
 *  @brief Interrupt enable register.
 */
DEF_SC16IS7xx_REG(IER,
    rw8 RHR_DATA_AVAIL_INT_EN : 1;                                  ///< data available interrupt enable
    rw8 THR_EMPTY_INT_EN : 1;                                       ///< transmit hold register empty interrupt enable
    rw8 RECEIVE_LINE_STAT_INT_EN : 1;                               ///< receive line status change interrupt enable
    rw8 MDM_STAT_INT_EN : 1;                                        ///< modem status lines change interrupt enable
    rw8 SLP_MODE_EN : 1;                                            ///< sleep mode enable
    rw8 nXOFF_INT_EN : 1;                                           ///< XON/XOFF control enable
    rw8 nRTS_INT_EN : 1;                                            ///< RTS status pin interrupt enable
    rw8 nCTS_INT_EN : 1;                                            ///< CTS status pin interrupt enable
)        


/**
 *  @brief FIFO control register.
 */
DEF_SC16IS7xx_REG(FCR,
    rw8 FIFO_EN : 1;                                                ///< Enable FIFO RX/TX buffers
    rw8 RX_FIFO_RST : 1;                                            ///< Reset RX FIFO 
    rw8 TX_FIFO_RST : 1;                                            ///< Reset TX FIFO
    ro8 : 1;                                                        ///< Not used
    rw8 TX_TRIGGER_LVL : 2;                                         ///< 2-bit TX trigger level
    rw8 RX_TRIGGER_LVL : 2;                                         ///< 2-bit RX trigger level
)


/**
 * @brief Transmit (TX) buffer fill trigger levels
 */
typedef enum
{
    TX_LVL_8SPACES = 0b00U,
    TX_LVL_16SPACES = 0b01U,
    TX_LVL_32SPACES = 0b10U,
    TX_LVL_56SPACES = 0b11U
} SC16IS7xx_fcr_tx_trigger_val;


/**
 * @brief Receive (RX) buffer fill trigger levels
 */
typedef enum
{
    RX_LVL_8CHARS = 0b00U,
    RX_LVL_16CHARS = 0b01U,
    RX_LVL_56CHARS = 0b10U,
    RX_LVL_60CHARS = 0b11U
} SC16IS7xx_fcr_rx_trigger_val;


/**
 *  @brief Decode for Interrupt indicator register priority bits.
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
} SC16IS7xx_irq_priority_val;


/**
 *  @brief Interrupt indicator register.
 */
DEF_SC16IS7xx_REG(IIR,
    ro8 IRQ_nPENDING : 1;                                           ///< Interrupt pending (inverted, 0=pending)
    ro8 IRQ_SOURCE : 5;                                             ///< Interrupt source field
    ro8 FIFO_EN : 2;                                                ///< FIFO interrupt enable
)


/**
 *  @brief Line control register, UART controls.
 */
DEF_SC16IS7xx_REG(LCR, 
    rw8 WORD_LEN : 2;                                               ///< Word length
    rw8 STOP : 1;                                                   ///< Stop bit destination
    rw8 PARITY_EN : 1;                                              ///< Parity enable
    rw8 EVEN_PARITY : 1;                                            ///< Parity odd/even
    rw8 SET_PARITY : 1;                                             ///< Fixed parity value
    rw8 SET_BREAK : 1;                                              ///< Set break condition
    rw8 DIVISOR_LATCH_EN : 1;                                       ///< Enable access to divisor register
)


/**
 *  @brief Modem control register.
 */
DEF_SC16IS7xx_REG(MCR,
    ro8 : 1;                                                        ///< Not used
    rw8 nRTS : 1;                                                   ///< Set RTS active
    rw8 TCR_TLR_EN : 1;                                             ///< Enable transmission control register
    ro8 : 1;                                                        ///< Not used
    rw8 LOOPBACK_EN : 1;                                            ///< Place TX/RX in loopback mode
    rw8 XON_ANY : 1;                                                ///< XON/XOFF enable
    rw8 IRDA_MODE_EN : 1;                                           ///< iRDA enable
    rw8 CLOCK_DIVISOR : 1;                                          ///< clock divisor bit
)


/**
 *  @brief Line status register.
 */
DEF_SC16IS7xx_REG(LSR,
    ro8 DATA_IN_RECVR : 1;                                          ///< Data present in RX FIFO/hold 
    ro8 OVERRUN_ERROR : 1;                                          ///< Data received prior to position vacated, overrun error
    ro8 PARITY_ERROR : 1;                                           ///< Data received with parity error
    ro8 FRAMING_ERROR : 1;                                          ///< Improper data frame received    
    ro8 BREAK_INT : 1;                                              ///< Serial break received
    ro8 THR_EMPTY : 1;                                              ///< Transmit hold register empty
    ro8 THR_TSR_EMPTY : 1;                                          ///< Transmit send register empty
    ro8 FIFO_DATA_ERROR : 1;                                        ///< FIFO data error (one or more positions with error)
)


/**
 *  @brief Modem status register.
 */
DEF_SC16IS7xx_REG(MSR,
    ro8 DELTA_CTS : 1;                                              ///< Change in CTS     
    ro8 : 3;                                                        ///< Not used
    ro8 CTS : 1;                                                    ///< CTS (clear-to-send) received
    ro8 : 3;                                                        ///< Not used
)


/**
 *  @brief Scratch pad register.
 */
DEF_SC16IS7xx_REG(SPR,
    rw8 DATA;                                                       ///< 8-bit scratchpad register
)


/**
 *  @brief UART software reset.
 */
DEF_SC16IS7xx_REG(UARTRST,
    ro8 : 3;                                                        ///< Not used
    rw8 UART_SWRST : 1;                                             ///< Perform a UART software initiated reset
)


/**
 *  @brief EFCR register (extra features)
 */
DEF_SC16IS7xx_REG(EFCR,
    rw8 MODE_9BIT_EN : 1;                                           ///< Enable 9-bit mode
    rw8 RECVR_DISABLE : 1;                                          ///< Disable RX 
    rw8 TRANSMITTER_DISABLE : 1;                                    ///< Disable TX
    ro8 : 1;                                                        ///< Not used
    rw8 AUTO_RS_485_RTS_DIR_CTRL : 1;                               ///< RS-485 RTS direction control
    rw8 AUTO_RS_485_RTS_OUTPUT_INV : 1;                             ///< RS-485 RTS output inverted
    ro8 : 1;                                                        ///< Not used
    rw8 IRDA_MODE : 1;                                              ///< IRDA mode
)


/**
 *  @brief Modem status register.
 */
DEF_SC16IS7xx_REG(TLR,
    rw8 TX_TRIGGER_LVL : 4;                                         ///< TX trigger level
    rw8 RX_TRIGGER_LVL : 4;                                         ///< RX trigger level
)


/**
 *  @brief
 */
DEF_SC16IS7xx_REG(EFR, 
    rw8 SWFLOW_CTRL : 4;                                            ///< SW flow control enabled
    rw8 ENHANCED_FNS_EN : 1;                                        ///< Enhanced feature set enabled
    rw8 SPECIAL_CHAR_DETECT : 1;                                    ///< Special character detection enabled
    rw8 AUTO_nRTS : 1;                                              ///< Auto RTS enable
    rw8 AUTO_nCTS : 1;                                              ///< Auto CTS enable
)

#pragma endregion
/* ----------------------------------------------------------------------------------------------------------------- */


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


/**
*	@brief Configure base NXP bridge settings: reset (opt), FIFO, trigger levels (no trig IRQ yet), baud and framing
*/
void SC16IS7xx_start();


/**
 *	@brief Enable IRQ servicing for communications between SC16IS741 and BG96
 */
void SC16IS7xx_enableIrqMode();


/**
 *	@brief Perform simple write/read using SC16IS741A scratchpad register. Used to test SPI communications.
 *  @return True if SC16IS7xx device responded.
 */
bool SC16IS7xx_ping();


/**
 *	@brief Ping UART for a limited period of time until SPI sync'd between host and UART.
 *  @return True if SC16IS7xx device responded.
 */
bool SC16IS7xx_awaitReady();


/**
 *	@brief Read from a SC16IS741A bridge register
 *	@param [in] reg_addr The register address
 *  @return reg_data - Byte of data read from register
 */
uint8_t SC16IS7xx_readReg(uint8_t reg_addr);


/**
 *	@brief Write to a SC16IS741A bridge register
 *	@param reg_addr [in] - The register address
 *	@param reg_data [in] - Pointer to the data to write to the register
 */
void SC16IS7xx_writeReg(uint8_t reg_addr, uint8_t reg_data);


/**
 *	@brief Reads through the SC16IS741A bridge (its RX FIFO)
 *	@param [out] rxData Destination buffer
 *	@param [in] size Length of the destination buffer
 */
void SC16IS7xx_read(uint8_t* rxData, uint32_t size);


/**
 *	@brief Write through the SC16IS741A bridge
 *	@param [in] txData Source data to write
 *	@param [in] size Length of the source
 */
void SC16IS7xx_write(const uint8_t * txData, uint32_t size);


/**
 *	@brief Clear FIFO contents
 *  @param resetAction [in] - What to reset TX, RX or both
 */
void SC16IS7xx_resetFifo(sc16IS7xx_FifoResetAction_t resetAction);


/**
 *	@brief Send serial break signal
 */
void SC16IS7xx_sendBreak();


/**
 *	@brief Send Ctrl-Z 
 */
void SC16IS7xx_sendCtrlZ();


/**
 *	@brief Send flushChar a number of times
 */
void SC16IS7xx_flushTx(char flushChar, uint16_t sendCnt);


// /**
//  *	@brief Flush contents of RX FIFO
//  */
// void SC16IS7xx_flushRxFifo();


/**
 *	@brief DEBUG: Show FIFO buffers fill level
 */
void SC16IS7xx__displayFifoStatus(const char *dispMsg);


#ifdef __cplusplus
}
#endif // __cplusplus

#endif  /* !__LTEMC_NXP_SC16IS_H__ */
