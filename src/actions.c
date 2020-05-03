// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "ltem1c.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))


#pragma region private functions
/* --------------------------------------------------------------------------------------------- */


static const char *strnend(const char *charStr, uint16_t maxSz)
{
    return memchr(charStr, '\0', maxSz);
}


#pragma endregion


/**
 *	\brief Creates an AT command control stucture on the heap.
 *
 *  \param[in]resultSz The size (length) of the command response buffer. Pass in 0 to use default size.
 *
 *  \return AT command control structure.
 */
action_t *action_create(uint8_t resultSz)
{
   	action_t *atcmd = calloc(1, sizeof(action_t));
	if (atcmd == NULL)
	{
        ltem1_faultHandler("atcmd-could not alloc at command object");
	}

    if (resultSz == 0)
        resultSz = ACTION_DEFAULT_RESULT_BUF_SZ;

    atcmd->resultHead = calloc(resultSz + 1, sizeof(char));
    if (atcmd->resultHead == NULL)
    {
        free(atcmd);
        ltem1_faultHandler("atcmd-could not alloc response buffer");
    }

    action_reset(atcmd);
    atcmd->cmdCompleteParser_func = action_okResultParser;

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
action_t *action_build(const char* cmdStr, size_t resultBufSz, uint16_t timeoutMillis,  uint16_t (*cmdCompleteParser_func)(const char *response))
{
    action_t *atCmd = action_create(resultBufSz);

    memcpy(atCmd->cmdStr, cmdStr, MIN(ACTION_INVOKE_CMDSTR_SZ, strlen(cmdStr)));
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
void action_destroy(action_t * atCmd)
{
    free(atCmd->resultHead);
    free(atCmd);
}



/**
 *	\brief Resets (initializes) an AT command structure.
 *
 *  \param[in] atCmd Pointer to command struct to reset
 */
void action_reset(action_t *atCmd)
{
    memset(atCmd->cmdStr, 0, ACTION_INVOKE_CMDSTR_SZ);
    memset(atCmd->resultHead, 0, ACTION_DEFAULT_RESULT_BUF_SZ);
    atCmd->resultTail = atCmd->resultHead;
    atCmd->resultSz = ACTION_DEFAULT_RESULT_BUF_SZ;
    atCmd->resultCode = ACTION_RESULT_PENDING;
    atCmd->invokedAt = 0;
    atCmd->timeoutMillis = ACTION_DEFAULT_TIMEOUT_MILLIS;
    atCmd->irdPending = iop_process_notAssigned;
    atCmd->cmdCompleteParser_func = action_okResultParser;
}



/**
 *	\brief Invokes a simple AT command to the BG96 module using the driver default atCmd object. For more complex or verbose
 *  command workflows use action_invokeAdv() with a custom atCmd object (see action_create()).
 *
 *	\param[in] cmdStr The command string to send to the BG96 module.
 */
void action_invoke(const char * cmdStr)
{
    action_reset(g_ltem1->dAction);
    strncpy(g_ltem1->dAction->cmdStr, cmdStr, ACTION_INVOKE_CMDSTR_SZ);
    action_invokeCustom(g_ltem1->dAction);
}



/**
 *	\brief Invokes a simple AT command to the BG96 module using the driver default atCmd object. For more complex or verbose
 *  command workflows use action_invokeAdv() with a custom atCmd object (see action_create()).
 *
 *	\param[in] cmdStr The command string to send to the BG96 module.
 */
void action_invokeWithParser(const char * cmdStr, uint16_t (*cmdCompleteParser_func)(const char *response))
{
    action_reset(g_ltem1->dAction);
    strncpy(g_ltem1->dAction->cmdStr, cmdStr, ACTION_INVOKE_CMDSTR_SZ);
    g_ltem1->dAction->cmdCompleteParser_func = cmdCompleteParser_func;
    action_invokeCustom(g_ltem1->dAction);
}



/**
 *	\brief Invokes an AT command to the BG96 module.
 */
void action_invokeCustom(action_t *atCmd)
{
    if (atCmd->cmdCompleteParser_func == NULL)
    {
        atCmd->cmdCompleteParser_func = action_okResultParser;
    }
    atCmd->invokedAt = timing_millis();

    g_ltem1->ltem1State = ltem1_state_actionPending;
    g_ltem1->pendAction = atCmd;

    PRINTF("CMD=%s\r", atCmd->cmdStr);
    strcat(atCmd->cmdStr, ASCII_sCR);
    iop_txSend(atCmd->cmdStr, strlen(atCmd->cmdStr));
}



/**
 *	\brief Invokes an AT command to the BG96 module.
 */
void action_invokeSendData(const char *txData)
{
    
    g_ltem1->dAction->cmdCompleteParser_func = action_okResultParser;
    g_ltem1->dAction->invokedAt = timing_millis();

    g_ltem1->ltem1State = ltem1_state_actionPending;
    g_ltem1->pendAction = g_ltem1->dAction;

    iop_txSend(txData, strlen(txData));
}



/**
 *	\brief Gathers command response strings and determines if command has completed.
 *
 *  \param[in] atCmd Pointer to command struct
 * 
 *  \return AT command result type: completed=200, timedout=408, or (still) pending=0.
 */
action_result_t action_getResult(action_t *atCmd, bool autoClose)
{
    // return pendingAtCommand.resultCode;
    // wait for BG96 response in FIFO buffer

    if (atCmd == NULL)
        atCmd = g_ltem1->dAction;

    action_result_t parserResult = 0;
    iop_rx_result_t rxResult = iop_rxGetCmdQueued(atCmd->resultTail, atCmd->resultSz);

    if (rxResult == iop_rx_result_ready || rxResult == iop_rx_result_truncated)
    {
        // deplete last result from the overall command result buffer
        uint8_t resultSz = strlen(atCmd->resultTail);
        atCmd->resultSz -= resultSz;
        atCmd->resultTail += resultSz;

        // invoke command result complete parser, true returned if complete
        parserResult = (*atCmd->cmdCompleteParser_func)(atCmd->resultHead);
        PRINTF_DBG1("prsr=%d \r", parserResult);
    }

    if (parserResult >= ACTION_RESULT_SUCCESS)                       // parser signaled complete
    {
        if (autoClose)
        {
            g_ltem1->pendAction = NULL;
            g_ltem1->ltem1State = ltem1_state_idle;
        }
        return parserResult;
    }
    bool timeout = timing_millis() - atCmd->invokedAt > atCmd->timeoutMillis;

    if (timeout && atCmd->resultHead != atCmd->resultTail)          // got something but not valid completion
    {
        if (autoClose)
        {
            g_ltem1->pendAction = NULL;
            g_ltem1->ltem1State = ltem1_state_idle;
        }
        return ACTION_RESULT_ERROR;
    }
    if (timeout)                                                    // got nothing and ran out of time
    {
        if (autoClose)
        {
            g_ltem1->pendAction = NULL;
            g_ltem1->ltem1State = ltem1_state_idle;
        }
        return ACTION_RESULT_TIMEOUT;
    }
    return ACTION_RESULT_PENDING;
}



/**
 *	\brief Waits for an AT command result.
 *
 *  \param[in] atCmd Pointer to command struct. Pass in NULL to reference integrated AT command structure.
 * 
 *  \return AT command result type: completed=200, timedout=408, or response error=500.
 */
action_result_t action_awaitResult(action_t *atCmd)
{
    action_result_t cmdResult;
    PRINTF_DBG1("AwtRslt\r");
    do
    {
        cmdResult = action_getResult(atCmd, true);
        yield();

    } while (cmdResult == ACTION_RESULT_PENDING);
    return cmdResult;
}



/**
 *	\brief Cancels an AT command currently underway.
 *
 *  \param[in] atCmd Pointer to command struct
 */
void action_cancel(action_t *action)
{
    // if singleton g_ltem1 atCmd is being cancelled reset it, if custom this is up to the caller
    if (action == g_ltem1->dAction)
    {
        action_reset(action);
    }
    g_ltem1->pendAction = NULL;
    g_ltem1->ltem1State = ltem1_state_idle;

}



#define OK_COMPLETED_STRING "OK\r\n"
#define OK_COMPLETED_LENGTH 4
#define ERROR_COMPLETED_STRING "ERROR\r\n"
#define ERROR_VALUE_OFFSET 7


/**
 *	\brief Performs a standardized parse of command responses. This function can be wrapped to match atcmd commandCompletedParse signature.
 *
 *  \param[in] response The string returned from the getResult function.
 *  \param[in] landmark The string to look for to signal response matches command (scans from end backwards).
 *  \param[in] landmarkReqd The landmark string is required, set to false to allow for only gap and terminator
 *  \param[in] gap The min. char count between landmark and terminator (0 is valid).
 *  \param[in] terminator The string to signal the end of the command response, chars between landmark and terminator are considered valid part of response.
 * 
 *  \return True if the response meets the conditions in the landmark, gap and terminator values.
 */
action_result_t action_gapResultParser(const char *response, const char *landmark, bool landmarkReqd, uint8_t gap, const char *terminator)
{
    char *searchPtr = NULL;
    char *terminatorAt = NULL;
    uint8_t landmarkSz = 0;

    if (landmark)
    {
        landmarkSz = strlen(landmark);
        searchPtr = strstr(response, landmark);
        
        if (landmarkReqd && searchPtr == NULL)
                return ACTION_RESULT_PENDING;

        while (searchPtr != NULL)                                   // find last occurrence of landmark
        {
            searchPtr = strstr(searchPtr + landmarkSz, landmark);
        }
    }
    
    searchPtr = searchPtr ? searchPtr + landmarkSz : response;      // landmark is NULL, adjust search details

    bool completedIsSuccess = true;
    if (terminator)                                                 // explicit terminator
        terminatorAt = strstr(searchPtr, terminator);

    if (!terminator)                                                // no explicit terminator, look for OK
        terminatorAt = strstr(searchPtr, OK_COMPLETED_STRING);

    if (!terminator && !terminatorAt)                               // no explicit terminator, look for ERROR
    {
        completedIsSuccess = false;
        terminatorAt = strstr(searchPtr, ERROR_COMPLETED_STRING);
    }

    if (terminatorAt && searchPtr + gap <= terminatorAt)            // term found with sufficient gap
        return completedIsSuccess ? ACTION_RESULT_SUCCESS : ACTION_RESULT_ERROR;

    if (terminatorAt)                                               // else gap insufficient
        return ACTION_RESULT_ERROR;

    return ACTION_RESULT_PENDING;                                // no term, keep looking
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
action_result_t action_tokenResultParser(const char *response, const char *landmark, char token, uint8_t tokensRequired)
{
    char *landmarkAt;
    uint8_t delimitersFound = 0;

    char *next = strstr(response, landmark);
    if (next == NULL)
        return ACTION_RESULT_PENDING;

    // find last occurrence of landmark
    uint8_t landmarkSz = strlen(landmark);
    while (next != NULL)
    {
        landmarkAt = next;
        next = strstr(next + landmarkSz, landmark);
    }
    next = landmarkAt + landmarkSz + 1;

    while (delimitersFound < (tokensRequired - 1) && *next != '\0')
    {
        next = strchr(next + 1, token);
        delimitersFound++;
    }
    return (delimitersFound == (tokensRequired -1)) ? ACTION_RESULT_SUCCESS : ACTION_RESULT_PENDING;
}



/**
 *	\brief Scans cmd response looking for a CME (extended) error code.
 *
 *	\param[in] response The command response received from the BG96.
 *
 *  \return Integer extended CME error code.
 */
action_result_t action_cmeErrorResultParser(const char *response)
{
    #define CME_PREABLE "+CME ERROR:"
    #define CME_PREABLE_SZ 11

    char *preambleAt = strstr(response, CME_PREABLE);

    if (preambleAt == NULL)
        return ACTION_RESULT_PENDING;

    // return error code, all CME >= 500
    return (strtol(preambleAt + CME_PREABLE_SZ, NULL, 10));
}



/**
 *	\brief Validate the response ends in a BG96 OK value.
 *
 *	\param[in] response The command response received from the BG96.
 *
 *  \return bool If the response string ends in a valid OK sequence
 */
action_result_t action_okResultParser(const char *response)
{
    return action_gapResultParser(response, NULL, false, 0, NULL);
}
