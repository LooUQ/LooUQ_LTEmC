// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "ltem1c.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))


#pragma region private functions
/* --------------------------------------------------------------------------------------------- */


static const char *strnlen(const char *charStr, uint16_t maxSz)
{
    return memchr(charStr, '\0', maxSz);
}



/**
 *	\brief Validate the response ends in a BG96 OK value.
 *
 *	\param[in] response The command response received from the BG96.
 *
 *  \return bool If the response string ends in a valid OK sequence
 */
static atcmd_result_t okCompletedParser(const char *response)
{
    // #define OK_COMPLETED_STRING "0\r"
    // #define OK_COMPLETED_LENGTH 2
    #define OK_COMPLETED_STRING "OK\r\n"
    #define ERROR_COMPLETED_STRING "ERROR\r\n"
    #define OK_COMPLETED_LENGTH 4
    #define ERROR_COMPLETED_LENGTH 7

    char *tail = strnlen(response, ATCMD_DEFAULT_RESULT_BUF_SZ);
    if (tail == NULL)
        tail = response + ATCMD_DEFAULT_RESULT_BUF_SZ;

    return strncmp(OK_COMPLETED_STRING, tail - OK_COMPLETED_LENGTH, OK_COMPLETED_LENGTH) == 0;
}


#pragma endregion


/**
 *	\brief Creates an AT command control stucture on the heap.
 *
 *  \param[in]resultSz The size (length) of the command response buffer. Pass in 0 to use default size.
 *
 *  \return AT command control structure.
 */
atcmd_t *atcmd_create(uint8_t resultSz)
{
   	atcmd_t *atcmd = calloc(1, sizeof(atcmd_t));
	if (atcmd == NULL)
	{
        ltem1_faultHandler("atcmd-could not alloc at command object");
	}

    if (resultSz == 0)
        resultSz = ATCMD_DEFAULT_RESULT_BUF_SZ;

    atcmd->resultHead = calloc(resultSz, sizeof(char));
    if (atcmd->resultHead == NULL)
    {
        free(atcmd);
        ltem1_faultHandler("atcmd-could not alloc response buffer");
    }

    atcmd_reset(atcmd);
    atcmd->cmdCompleteParser_func = okCompletedParser;

    return atcmd;
}



/**
 *	\brief Creates an AT command object with custom properties.
 *
 *	\param[in] cmdStr The AT command string to send to the BG96 module.
 *  \param[in] timeoutMillis Command timeout period in milliseconds.
 *  \param[in] resultBuf Pointer to application buffer for AT cmds with long or URC style result.
 *  \param[in] resultBufSz Result buffer maximum chars.
 *  \param[in] cmdCompleteParser_func Function pointer to custom result completion parser.
 */
atcmd_t *atcmd_build(const char* cmdStr, char* resultBuf, size_t resultBufSz, uint16_t timeoutMillis,  uint16_t (*cmdCompleteParser_func)(const char *response))
{
    atcmd_t *atCmd = atcmd_create(resultBufSz);

    memcpy(atCmd->cmdStr, cmdStr, MIN(ATCMD_INVOKE_CMDSTR_SZ, strlen(cmdStr)));
    atCmd->resultTail = atCmd->resultHead;
    atCmd->resultSz = resultBufSz;
    atCmd->timeoutMillis = timeoutMillis;
    atCmd->cmdCompleteParser_func = cmdCompleteParser_func;

    atCmd->invokedAt = 0;
    atCmd->resultCode = 0;
    atCmd->irdPending = iop_process_notAssigned;
}



/**
 *	\brief Destroys AT command control stucture.
 */
void atcmd_destroy(atcmd_t * atCmd)
{
    free(atCmd);
}



/**
 *	\brief Resets (initializes) an AT command structure.
 *
 *  \param[in] atCmd Pointer to command struct to reset
 */
void atcmd_reset(atcmd_t *atCmd)
{
    memset(atCmd->cmdStr, 0, ATCMD_INVOKE_CMDSTR_SZ);
    atCmd->resultTail = atCmd->resultHead;
    atCmd->resultSz = ATCMD_DEFAULT_RESULT_BUF_SZ;
    atCmd->resultCode = ATCMD_RESULT_PENDING;
    atCmd->invokedAt = 0;
    atCmd->timeoutMillis = ATCMD_DEFAULT_TIMEOUT_MILLIS;
    atCmd->irdPending = iop_process_notAssigned;
}



/**
 *	\brief Invokes a simple AT command to the BG96 module using the driver default atCmd object. For more complex or verbose
 *  command workflows use atcmd_invokeAdv() with a custom atCmd object (see atcmd_create()).
 *
 *	\param[in] cmdStr The command string to send to the BG96 module.
 */
void atcmd_invoke(const char * cmdStr)
{
    atcmd_reset(g_ltem1->atcmd);
    strncpy(g_ltem1->atcmd->cmdStr, cmdStr, ATCMD_INVOKE_CMDSTR_SZ);
    atcmd_invokeAdv(g_ltem1->atcmd);
}



/**
 *	\brief Invokes an AT command to the BG96 module.
 */
void atcmd_invokeAdv(atcmd_t *atCmd)
{
    // result completed parser
    if (atCmd->cmdCompleteParser_func == NULL)
    {
        atCmd->cmdCompleteParser_func = okCompletedParser;
    }
    atCmd->invokedAt = timing_millis();

    g_ltem1->pendingCmd = atCmd;
    // at command struct initialized for command, perform actual invoke to BG96
    sc16is741a_write(atCmd->cmdStr, MIN(strlen(atCmd->cmdStr), ATCMD_INVOKE_CMDSTR_SZ));
}



/**
 *	\brief Gathers command response strings and determines if command has completed.
 *
 *  \param[in] atCmd Pointer to command struct
 * 
 *  \return AT command result type: completed=200, timedout=408, or (still) pending=0.
 */
atcmd_result_t atcmd_getResult(atcmd_t *atCmd)
{
    // return pendingAtCommand.resultCode;
    // wait for BG96 response in FIFO buffer

    atcmd_result_t parserResult = 0;
    atcmd_result_t cmdResult = 0;

    iop_rx_result_t rxResult = iop_rxGetQueued(iop_process_command, atCmd->resultTail, atCmd->resultSz);

    if (rxResult == iop_rx_result_ready || rxResult == iop_rx_result_truncated)
    {
        // deplete last result from the overall command result buffer
        uint8_t resultSz = strlen(atCmd->resultTail);
        atCmd->resultSz -= resultSz;
        atCmd->resultTail += resultSz;

        // invoke command result complete parser, true returned if complete
        parserResult = (*atCmd->cmdCompleteParser_func)(atCmd->resultHead);
    }

    if (parserResult == ATCMD_RESULT_SUCCESS)    // parser signaled complete, successful
    {
        g_ltem1->pendingCmd = NULL;
        return ATCMD_RESULT_SUCCESS;
    }

    if (parserResult >= ATCMD_RESULT_BASEERROR)          // parser signaled complete, with failure
    {
        g_ltem1->pendingCmd = NULL;
        return parserResult;
    }

    bool timeout = timing_millis() - atCmd->invokedAt > atCmd->timeoutMillis;

    if (timeout && atCmd->resultHead != atCmd->resultTail)          // got something but not valid completion
    {
        g_ltem1->pendingCmd = NULL;
        return ATCMD_RESULT_ERROR;
    }
    else if (timeout)                                               // got nothing and ran out of time
    {
        g_ltem1->pendingCmd = NULL;
        return ATCMD_RESULT_TIMEOUT;
    }
    return ATCMD_RESULT_PENDING;
}



/**
 *	\brief Waits for an AT command result.
 *
 *  \param[in] atCmd Pointer to command struct
 * 
 *  \return AT command result type: completed=200, timedout=408, or response error=500.
 */
atcmd_result_t atcmd_awaitResult(atcmd_t *atCmd)
{
    atcmd_result_t atResult;
    do
    {
        atResult = atcmd_getResult(atCmd);

    } while (atResult == ATCMD_RESULT_PENDING);
}



/**
 *	\brief Cancels an AT command currently underway.
 *
 *  \param[in] atCmd Pointer to command struct
 */
void atcmd_cancel(atcmd_t *atCmd)
{
    // if singleton g_ltem1 atCmd is being cancelled reset it, if custom this is up to the caller
    if (atCmd == g_ltem1->atcmd)
    {
        atcmd_reset(atCmd);
    }
    g_ltem1->pendingCmd = NULL;
}


/**
 *	\brief Performs a standardized parse of command responses. This function can be wrapped to match atcmd commandCompletedParse signature.
 *
 *  \param[in] response The string returned from the getResult function.
 *  \param[in] landmark The string to look for to signal response matches command (scans from end backwards).
 *  \param[in] gap The min. char count between landmark and terminator (0 is valid).
 *  \param[in] terminator The string to signal the end of the command response, chars between landmark and terminator are considered valid part of response.
 * 
 *  \return True if the response meets the conditions in the landmark, gap and terminator values.
 */
uint16_t atcmd_gapCompletedHelper(const char *response, const char *landmark, u_int8_t gap, const char *terminator)
{
    char *landmarkAt;
    char *next;
    char *terminatorAt;

    next = strstr(response, landmark);
    if (next == NULL)
        return false;

    // find last occurrence of landmark
    uint8_t landmarkSz = strlen(landmark);
    while (next != NULL)
    {
        landmarkAt = next;
        next = strstr(landmarkAt + landmarkSz, landmark);
    }
    terminatorAt = strstr(landmarkAt + landmarkSz, terminator);

    if (terminatorAt != NULL && terminatorAt - (landmarkAt + landmarkSz) >= gap)
        return true;

    return false;
}



/**
 *	\brief Performs a standardized parse of command responses. This function can be wrapped to match atcmd commandCompletedParse signature.
 *
 *  \param[in] response The string returned from the getResult function.
 *  \param[in] landmark The string to look for to signal response matches command (scans from end backwards).
 *  \param[in] token The token delimeter char.
 *  \param[in] tokenCnt The min number of tokens expected.
 * 
 *  \return True if the response meets the conditions in the landmark and token count required.
 */
uint16_t atcmd_tokenCompletedHelper(const char *response, const char *landmark, char token, uint8_t tokensRequired)
{
    char *landmarkAt;
    char *next;
    uint8_t delimitersFound = 0;

    next = strstr(response, landmark);
    if (next == NULL)
        return false;

    // find last occurrence of landmark
    uint8_t landmarkSz = strlen(landmark);
    while (next != NULL)
    {
        next = strstr(next + landmarkSz, landmark);
    }

    next = next + landmarkSz + 1;
    while (delimitersFound < (tokensRequired - 1) && *next != NULL)
    {
        next = strchr(next + 1, token);
        delimitersFound++;
    }

    return delimitersFound == (tokensRequired -1);
}
