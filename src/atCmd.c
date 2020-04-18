// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "ltem1c.h"

#define DEFAULT_TIMEOUT_MILLIS 500
#define DEFAULT_RESULT_BUF_SZ 80

#define ATCMD_RESULT_PENDING 0
#define ATCMD_RESULT_SUCCESS 200
#define ATCMD_RESULT_TIMEOUT 408
#define ATCMD_RESULT_FAILURE 500

// ltem1_device g_ltem1;
char g_atresult[DEFAULT_RESULT_BUF_SZ];


#pragma region static local functions
/* --------------------------------------------------------------------------------------------- */

/**
 *	\brief Validate the response ends in a BG96 OK value.
 *
 *	\param[in] response The command response received from the BG96.
 *  \return bool If the response string ends in a valid OK sequence
 */
static char* okCompleteParser(const char *response)
{
    #define BUFF_SZ 64
    #define EXPECTED_TERMINATOR_STR "OK\r\n"
    #define EXPECTED_TERMINATOR_LEN 4

    const char * end = (const char *)memchr(response, '\0', BUFF_SZ);
    if (end == NULL)
        end = response + BUFF_SZ;

    return strncmp(EXPECTED_TERMINATOR_STR, end - EXPECTED_TERMINATOR_LEN, EXPECTED_TERMINATOR_LEN) == 0;
}


#pragma endregion


/**
 *	\brief Creates an AT command control stucture on the heap.
 *
 *  \return AT command control structure.
 */
atcommand_t *atcmd_create()
{
   	atcommand_t *atcmd = calloc(1, sizeof(atcommand_t));
	if (atcmd == NULL)
	{
        ltem1_faultHandler("atcmd-could not alloc at command object");
	}	
    return atcmd;
}



/**
 *	\brief Destroys AT command control stucture.
 */
void atcmd_destroy()
{
    free(g_ltem1->atcmd);
}



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
    strcpy(g_ltem1->atcmd->cmd, atcmd);
    g_ltem1->atcmd->timeoutMillis = timeout_millis;
    g_ltem1->atcmd->resultCode = ATCMD_RESULT_PENDING;

    // set result to nulls in advance
    g_ltem1->atcmd->result = response_buf;
    memset(g_ltem1->atcmd->result, 0, response_bufsize);

    // result parser
    if (cmdresult_parser_func == NULL)
    {
        g_ltem1->atcmd->cmdresult_parser_func = okCompleteParser;
    }
    g_ltem1->atcmd->invokedAt = timing_millis();

    // at command struct initialized for command, perform actual invoke to BG96
    spi_transferBuffer(g_ltem1->spi, SC16IS741A_FIFO_RnW_WRITE, g_ltem1->atcmd->cmd, strlen(g_ltem1->atcmd->cmd));
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
      lsrValue = sc16is741a_readReg(SC16IS741A_LSR_ADDR);
      if (lsrValue & NXP_LSR_DATA_IN_RECVR)
      {
        uint8_t fifoValue = sc16is741a_readReg(SC16IS741A_FIFO_ADDR);
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

