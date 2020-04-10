// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "atcmd.h"

#define DEFAULT_TIMEOUT_MILLIS 500
#define DEFAULT_RESULT_BUF_SZ 80

#define ATCMD_RESULT_PENDING 0
#define ATCMD_RESULT_SUCCESS 200
#define ATCMD_RESULT_TIMEOUT 408
#define ATCMD_RESULT_FAILURE 500

// ltem1_device g_ltem1;
at_command_t g_atcommand;
char g_atresult[DEFAULT_RESULT_BUF_SZ];


#pragma region static local functions
/*
------------------------------------------------------------------------------------------------- */

static char* cmdComplete_okParser()
{
}


#pragma endregion



/**
 *	\brief Invokes a simple AT command to the BG96 module (response < 80 chars, timeout < 10,000 millis and OK response).
 *
 *	\param[in] atCmd The command string to send to the BG96.
 *  \param[in] startIo Initialize hardware and start modem.
 *  \param[in] enableIrqMode Initialize ltem1 to use IRQ/ISR for communication events.
 */
void atcmd_invoke(const char * atCmd)
{
    atcmd_invokeAdv(atCmd, DEFAULT_TIMEOUT_MILLIS, g_atresult, DEFAULT_RESULT_BUF_SZ, NULL);
}


/**
 *	\brief Invokes an AT command to the BG96 module with options (URC, long timeout or verbose response).
 *
 *	\param[in] atCmd The command string to send to the BG96.
 *  \param[in] timeoutMillis Command timeout period in milliseconds.
 *  \param[in] responseBuf Custom response buffer for long URC type commands.
 *  \param[in] responseBufSz Custom response buffer maximum chars.
 *  \param[in] urc_parse_func_ptr_t Function pointer to custom response parser.
 */
void atcmd_invokeAdv(const char* atcmd, uint8_t timeout_millis,  char* response_buf, uint8_t response_bufsize, char* (*cmdresult_parser_func)())
{
    strcpy(g_atcommand.cmd, atcmd);
    g_atcommand.timeoutMillis = timeout_millis;
    g_atcommand.resultCode = ATCMD_RESULT_PENDING;

    // set result to nulls in advance
    g_atcommand.result = response_buf;
    memset(g_atcommand.result, 0, response_bufsize);

    // result parser
    if (cmdresult_parser_func == NULL)
    {
        g_atcommand.cmdresult_parser_func = cmdComplete_okParser;
    }
    g_atcommand.invokedAt = timing_millis();

    // at command struct initialized for command, perform actual invoke to BG96
    spi_transferBuffer(g_ltem1->bridge->spi, SC16IS741A_FIFO_RnW_WRITE, g_atcommand.cmd, strlen(g_atcommand.cmd));
}



uint8_t atcmd_getResult(char *result)
{
  //    return pendingAtCommand.resultCode;
  // wait for BG96 response in FIFO buffer

  uint8_t lsrValue = 0;
  while (!lsrValue & NXP_LSR_DATA_IN_RECVR)
  {
    for (int i = 0; i < 64; i++)
    {
      lsrValue = sc16is741a_readReg(g_ltem1->bridge, SC16IS741A_LSR_ADDR);
      if (lsrValue & NXP_LSR_DATA_IN_RECVR)
      {
        uint8_t fifoValue = sc16is741a_readReg(g_ltem1->bridge, SC16IS741A_FIFO_ADDR);
        g_atresult[i] = fifoValue;
        continue;
      }
      break;
    }
  }
  return 200;

}



void atcmd_cancel()
{
    // pendingAtCommand.invokedAt = 0;
    // pendingAtCommand.resultCode = -1;
    // pendingAtCommand.cmd = NULL;
    // pendingAtCommand.result = NULL;
}

