/******************************************************************************
 *  \file ltemc-atcmd.c
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020,2021 LooUQ Incorporated.
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
 * BGx AT command processor 
 *****************************************************************************/

#define _DEBUG 0                        // set to non-zero value for PRINTF debugging output, 
// debugging output options             // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if defined(_DEBUG) && _DEBUG > 0
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG == 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#endif

#include "ltemc.h"


// Local scoped #defines
#define ACTION_LOCKRETRIES             3        ///< Number of attemps to acquire action lock
#define ACTION_LOCKRETRY_INTERVALml   50        ///< Millis to wait between action lock acquisition attempts
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

// Local scoped function declarations
static void s_atcmdInit(const char *cmdStr);
static void s_copyToDiagnostics();


/**
 *	\brief Closes (completes) a BGx AT command structure and frees action resource (release action lock).
 */
void atcmd_close()
{
    g_ltem->atcmd->isOpen = false;
}


/**
 *	\brief Invokes a BGx AT command with default action values. 
 *
 *	\param cmdStr [in] The command string to send to the BG96 module.
 * 
 *  \return True if action was invoked, false if not
 */
bool atcmd_tryInvoke(const char *cmdStr)
{
    return atcmd_tryInvokeAdv(cmdStr, ACTION_TIMEOUTml, atcmd_okResultParser);
}



/**
 *	\brief Invokes a BGx AT command with application specified action values. 
 *
 *	\param cmdStr [in] The command string to send to the BG96 module.
 *  \param retries [in] Number of retries to obtain action lock that should be attempted. 0 = no retries.
 *  \param timeout [in] Number of milliseconds the action can take. Use system default ACTION_TIMEOUTml or your value.
 *  \param taskCompleteParser [in] Custom command response parser to signal result is complete. NULL for std parser.
 * 
 *  \return True if action was invoked, false if not
 */
bool atcmd_tryInvokeAdv(const char *cmdStr, uint16_t timeout, uint16_t (*taskCompleteParser)(const char *response, char **endptr))
{
    if ( !atcmd__acquireLock(cmdStr, ACTION_LOCKRETRIES) )
        return false;

    g_ltem->atcmd->timeoutMillis = timeout;
    g_ltem->atcmd->invokedAt = lMillis();
    g_ltem->atcmd->taskCompleteParser_func = taskCompleteParser == NULL ? atcmd_okResultParser : taskCompleteParser;

    iop_txSend(cmdStr, strlen(cmdStr), false);
    iop_txSend(ASCII_sCR, 1, true);
    return true;
}



/**
 *	\brief Performs data transfer (send) sub-action.

 *  \param data [in] - Pointer to the block of binary data to send.
 *  \param dataSz [in] - The size of the data block. 
 *  \param timeoutMillis [in] - Timeout period (in millisecs) for send to complete.
 *  \param taskCompleteParser_func [in] - Function pointer to parser looking for task completion
 */
void atcmd_sendRaw(const char *data, uint16_t dataSz, uint16_t timeoutMillis, uint16_t (*taskCompleteParser_func)(const char *response, char **endptr))
{
    if (timeoutMillis > 0)
        g_ltem->atcmd->timeoutMillis = timeoutMillis;
    if (taskCompleteParser_func == NULL)
        g_ltem->atcmd->taskCompleteParser_func = atcmd_okResultParser;
    else 
        g_ltem->atcmd->taskCompleteParser_func = taskCompleteParser_func;

    iop_txSend(data, dataSz, true);
}



/**
 *	\brief Performs data transfer (send) sub-action.

 *  \param data [in] - Pointer to the block of binary data to send.
 *  \param dataSz [in] - The size of the data block. 
 *  \param eotPhrase [in] - Char string that is sent a the end-of-transmission.
 *  \param timeoutMillis [in] - Timeout period (in millisecs) for send to complete.
 *  \param taskCompleteParser_func [in] - Function pointer to parser looking for task completion
 */
void atcmd_sendRawWithEOTs(const char *data, uint16_t dataSz, const char *eotPhrase, uint16_t timeoutMillis, uint16_t (*taskCompleteParser_func)(const char *response, char **endptr))
{
    if (timeoutMillis > 0)
        g_ltem->atcmd->timeoutMillis = timeoutMillis;
    if (taskCompleteParser_func != NULL)
        g_ltem->atcmd->taskCompleteParser_func = taskCompleteParser_func;
    else
        g_ltem->atcmd->taskCompleteParser_func = atcmd_okResultParser;
        
    iop_txSend(data, dataSz, false);
    iop_txSend(eotPhrase, strlen(eotPhrase), true);
}



/**
 *	\brief Waits for an AT command result until completed response or timeout.
 *
 *  \param autCloseAction [in] USE WITH CAUTION - On result, close the action. The caller only needs the status code. 
 * 
 *  \return Action result. WARNING - the actionResult.response contents are undetermined after close. 
 */
atcmdResult_t atcmd_awaitResult(bool autoCloseAction)
{
    atcmdResult_t actionResult;

    do
    {
        actionResult = atcmd_getResult(autoCloseAction);
        
        if (g_ltem->cancellationRequest)
        {
            actionResult.response = 0;
            actionResult.statusCode = RESULT_CODE_CANCELLED;
            break;
        }
        lYield();

    } while (actionResult.statusCode == RESULT_CODE_PENDING);
    
    return actionResult;
}



/**
 *	\brief Gets command response and returns immediately.
 *
 *  \param closeAction [in] - USE WITH CAUTION - On result, close the action. The caller only needs the status code. 
 * 
 *  \return Action result. WARNING - the actionResult.response contents are undetermined after close. 
 */
atcmdResult_t atcmd_getResult(bool closeAction)
{
    atcmdResult_t result = {.statusCode = RESULT_CODE_PENDING, .response = g_ltem->iop->rxCmdBuf->buffer};    // default response
    char *endptr = NULL;

    if (g_ltem->iop->rxCmdBuf->tail[0] != 0)                                                   // if cmd buffer not empty, test for command complete with parser
    {
        g_ltem->atcmd->resultCode = (*g_ltem->atcmd->taskCompleteParser_func)(g_ltem->iop->rxCmdBuf->tail, &endptr);
        PRINTF(DBGCOLOR_gray, "prsr=%d \r", g_ltem->atcmd->resultCode);
    }

    if (g_ltem->atcmd->resultCode == RESULT_CODE_PENDING)                                     // check for timeout error
    {
        if (lTimerExpired(g_ltem->atcmd->invokedAt, g_ltem->atcmd->timeoutMillis))
        {
            result.statusCode = RESULT_CODE_TIMEOUT;
            g_ltem->atcmd->resultCode = RESULT_CODE_TIMEOUT;
            g_ltem->atcmd->isOpen = false;                                                        // close action to release action lock
            s_copyToDiagnostics();                                                                  // copy to diagnostics on error
            
            // if action timed-out, verify not a device wide failure
            if (!ltem_chkHwReady())
                ltem_notifyApp(ltemNotifType_hwNotReady, "Modem HW Status Offline");                  // BGx status pin

            else if (!sc16is741a_chkCommReady())
                ltem_notifyApp(ltemNotifType_hwNotReady, "Modem comm unresponsive");                  // UART bridge SPI wr/rd
        }
        return result;
    }

    result.statusCode = g_ltem->atcmd->resultCode;                            // parser completed, set return status code (could be success or error)
    g_ltem->iop->rxCmdBuf->tail = endptr;                                      // adj prevHead to unparsed cmd stream

    // if parser left data trailing parsed content in cmd buffer: need to parseImmediate() for URCs, as if they just arrived
    if (g_ltem->iop->rxCmdBuf->tail < g_ltem->iop->rxCmdBuf->head)      
        iop_rxParseImmediate();

    if (result.statusCode <= RESULT_CODE_SUCCESSMAX)                            // parser completed with success code
    {
        if (closeAction)
            g_ltem->atcmd->isOpen = false;
        return result;
    }

    // handled timeout and success results above, if here must be specific error
    g_ltem->atcmd->isOpen = false;                                           // close action to release action lock on any error
    s_copyToDiagnostics();                                                     // if failure result, copy action info to history for diagnostics
    return result;
}


/**
 *	\brief Sends ESC character to ensure BGx is not in text mode (> prompt awaiting ^Z/ESC).
 */
void atcmd_exitTextMode()
{
    iop_txSend("\x1B", 1, true);            // send ESC - 0x1B
}


/**
 *	\brief Sends +++ sequence to transition BGx out of data mode to command mode.
 */
void atcmd_exitDataMode()
{
    lDelay(1000);
    iop_txSend("+++", 1, true);             // send +++, gaurded by 1 second of quiet
    lDelay(1000);
}


#pragma endregion


#pragma region completionParsers
/* --------------------------------------------------------------------------------------------------------
 * Action command completion parsers
 * ----------------------------------------------------------------------------------------------------- */

#define OK_COMPLETED_STRING "OK\r\n"
#define OK_COMPLETED_LENGTH 4
#define ERROR_COMPLETED_STRING "ERROR\r\n"
#define ERROR_VALUE_OFFSET 7
#define FAIL_COMPLETED_STRING "FAIL\r\n"
#define FAIL_VALUE_OFFSET 6
#define NOCARRIER_COMPLETED_STRING "NO CARRIER\r\n"
#define NOCARRIER_VALUE_OFFSET 12
#define CME_PREABLE "+CME ERROR:"
#define CME_PREABLE_SZ 11



/**
 *	\brief Performs a standardized parse of command responses. This function can be wrapped to match atcmd commandCompletedParse signature.
 *
 *  \param response [in] - The string returned from the getResult function.
 *  \param preamble [in] - The string to look for to signal start of response match.
 *  \param preambleReqd [in] - The preamble string is required, set to false to search for only gap and terminator
 *  \param gapReqd [in] - The minimum char count between preamble (or start) and terminator (0 is valid).
 *  \param terminator [in] - The string to signal the end of the command response.
 *  \param endptr [out] - Char pointer to the next char in response after parser match
 * 
 *  \return HTTP style result code, 0 if parser incomplete (need more response). OK = 200.
 */
resultCode_t atcmd_defaultResultParser(const char *response, const char *preamble, bool preambleReqd, uint8_t gapReqd, const char *terminator, char** endptr)
{
    char *preambleAt = NULL;
    char *terminatorAt = NULL;

    uint8_t preambleSz = strlen(preamble);
    if (preambleSz)                                                 // process preamble requirements
    {
        preambleAt = strstr(response, preamble);
        if (preambleReqd && preambleAt == NULL)
                return RESULT_CODE_PENDING;
    }
    else
        preambleAt = response;
    
    char *termSearchAt = preambleAt ? preambleAt + preambleSz : response;    // if preamble is NULL, start remaing search from response start

    if (terminator)                                                 // explicit terminator
    {
        terminatorAt = strstr(termSearchAt, terminator);
        *endptr = terminatorAt + strlen(terminator);
    }
    else                                                            // no explicit terminator, look for standard AT responses
    {
        terminatorAt = strstr(termSearchAt, OK_COMPLETED_STRING);
        if (terminatorAt)
        {
            *endptr = terminatorAt + 4;         // + strlen(OK_COMPLETED_STRING)
        }
        else if (!terminatorAt)                                              
        {
            terminatorAt = strstr(termSearchAt, CME_PREABLE);                  // no explicit terminator, look for extended CME errors
            if (terminatorAt)
            {
                // return error code, all CME >= 500
                uint16_t cmeVal = strtol(terminatorAt + CME_PREABLE_SZ, endptr, 10);        // strtol will set endptr
                return cmeVal;
            }
        }
        else if (!terminatorAt)
        {
            terminatorAt = strstr(termSearchAt, ERROR_COMPLETED_STRING);        // no explicit terminator, look for ERROR
            if (terminatorAt)
            {
                *endptr = terminatorAt + 7;     // + strlen(ERROR_COMPLETED_STRING)
                return RESULT_CODE_ERROR;
            }
        }
        else if (!terminatorAt)
        {
            terminatorAt = strstr(termSearchAt, FAIL_COMPLETED_STRING);         // no explicit terminator, look for FAIL
            if (terminatorAt)
            {
                *endptr = terminatorAt + 6;     // + strlen(FAIL_COMPLETED_STRING)
                return RESULT_CODE_ERROR;
            }
        }
    }

    if (terminatorAt && termSearchAt + gapReqd <= terminatorAt)                 // explicit or implicit term found with sufficient gap
        return RESULT_CODE_SUCCESS;
    if (terminatorAt)                                                           // else gap insufficient
        return RESULT_CODE_ERROR;

    return RESULT_CODE_PENDING;                                                 // no term, keep looking
}



/**
 *	\brief Performs a standardized parse of command responses. This function can be wrapped to match atcmd commandCompletedParse signature.
 *
 *  \param response [in] - The string returned from the getResult function.
 *  \param preamble [in] - The string to look for to signal response matches command (scans from end backwards).
 *  \param delim [in] - The token delimeter char.
 *  \param reqdTokens [in] - The minimum number of tokens expected.
 *  \param terminator [in] - Text to look for indicating the sucessful end of command response.
 *  \param endptr [out] - Char pointer to the next char in response after parser match
 * 
 *  \return True if the response meets the conditions in the preamble and token count required.
 */
resultCode_t atcmd_tokenResultParser(const char *response, const char *preamble, char delim, uint8_t reqdTokens, const char *terminator, char** endptr)
{
    uint8_t delimitersFound = 0;

    char *terminatorAt = strstr(response, terminator);
    *endptr = terminatorAt + strlen(terminator) + 1;

    if (terminatorAt != NULL)
    {
        char *preambleAt = strstr(response, preamble);
        if (preambleAt == NULL)
            return RESULT_CODE_NOTFOUND;

        // now count delimitersFound
        char *next = preambleAt + strlen(preamble) + 1;

        while (next < terminatorAt && next != NULL)
        {
            next = strchr(++next, delim);
            delimitersFound += (next == NULL) ? 0 : 1;
        }

        if (++delimitersFound >= reqdTokens)
            return RESULT_CODE_SUCCESS;
        return RESULT_CODE_NOTFOUND;
    }

    char *cmeErrorCodeAt = strstr(response, CME_PREABLE);           // CME error codes generated by BG96
    if (cmeErrorCodeAt)
    {
        // return error code, all CME >= 500
        return (strtol(cmeErrorCodeAt + CME_PREABLE_SZ, endptr, 10));
    }
    return RESULT_CODE_PENDING;
}




/**
 *	\brief Validate the response ends in a BGxx OK value.
 *
 *	\param response [in] - Pointer to the command response received from the BGx.
 *  \param endptr [out] - Char pointer to the next char in response after parser match
 *
 *  \return bool If the response string ends in a valid OK sequence
 */
resultCode_t atcmd_okResultParser(const char *response, char** endptr)
{
    return atcmd_defaultResultParser(response, NULL, false, 0, NULL, endptr);
}



/**
 *	\brief Parser for open connection response, shared by UDP/TCP/SSL/MQTT.
 *  \param response [in] - Pointer to the command response received from the BGx.
 *  \param preamble [in] - Pointer to a landmark phrase to look for in the response
 *  \param resultIndx [in] - Zero based index after landmark of numeric fields to find result
 *  \param endptr [out] - Pointer to character in response following parser match
 */
resultCode_t atcmd_serviceResponseParser(const char *response, const char *preamble, uint8_t resultIndx, char** endptr) 
{
    char *next = strstr(response, preamble);
    if (next == NULL)
        return RESULT_CODE_PENDING;

    next += strlen(preamble);
    int16_t resultVal;

    // expected form: +<preamble>: <some other info>,<RESULT_CODE>
    // return (200 + RESULT_CODE)
    for (size_t i = 0; i < resultIndx; i++)
    {
        if (next == NULL)
            break;
        next = strchr(next, ASCII_cCOMMA);
        next++;         // point past comma
    }
    if (next != NULL)
        resultVal = (uint16_t)strtol(next, endptr, 10);

    // return a success value 200 + result (range: 200 - 300)
    return  (resultVal < RESULT_CODE_SUCCESSRANGE) ? RESULT_CODE_SUCCESS + resultVal : resultVal;
}


/**
 *	\brief C-string token grabber.
 */
char *atcmd_strToken(char *source, int delimiter, char *token, uint8_t tokenMax) 
{
    char *delimAt;
    if (source == NULL)
        return false;

    delimAt = strchr(source, delimiter);
    uint8_t tokenSz = delimAt - source;
    if (tokenSz == 0)
        return NULL;

    memset(token, 0, tokenMax);
    strncpy(token, source, MIN(tokenSz, tokenMax-1));
    return delimAt + 1;
}

#pragma endregion  // completionParsers



#pragma region private functions
/* private (static) fuctions
 * --------------------------------------------------------------------------------------------- */


/**
 *	\brief Initializes (locks) a Bgx AT command structure.
 *
 *  \param cmdStr [in] - Initializes\resets action control struct for use\reuse.
 */
static void s_atcmdInit(const char *cmdStr)
{
    /* clearing req/resp buffers now for debug clarity, future likely just insert starting \0 */

    // request side of action
    g_ltem->atcmd->isOpen = true;
    memset(g_ltem->atcmd->cmdStr, 0, IOP_TX_BUFFER_SZ);
    strncpy(g_ltem->atcmd->cmdStr, cmdStr, MIN(strlen(cmdStr), IOP_TX_BUFFER_SZ));
    g_ltem->atcmd->timeoutMillis = 0;
    g_ltem->atcmd->resultCode = RESULT_CODE_PENDING;
    g_ltem->atcmd->invokedAt = 0;
    g_ltem->atcmd->taskCompleteParser_func = NULL;
    // response side
    iop_resetCmdBuffer();

}



/**
 *	\brief Copies response\result information at action conclusion. Designed as a diagnostic aid for failed AT actions.
 */
static void s_copyToDiagnostics()
{
    memset(g_ltem->atcmd->lastActionError->cmdStr, 0, ACTION_HISTRESPBUF_SZ);
    strncpy(g_ltem->atcmd->lastActionError->cmdStr, g_ltem->atcmd->cmdStr, ACTION_HISTRESPBUF_SZ-1);
    memset(g_ltem->atcmd->lastActionError->response, 0, ACTION_HISTRESPBUF_SZ);
    strncpy(g_ltem->atcmd->lastActionError->response, g_ltem->atcmd->response, ACTION_HISTRESPBUF_SZ-1);
    g_ltem->atcmd->lastActionError->statusCode = g_ltem->atcmd->resultCode;
    g_ltem->atcmd->lastActionError->duration = lMillis() - g_ltem->atcmd->invokedAt;
}



/**
 *  \brief Attempts to get exclusive access to QBG module command interface.
 * 
 *  \param cmdStr [in] - The AT command text.
 *  \param retries [in] - Number of retries to be attempted, while obtaining lock.
*/
bool atcmd__acquireLock(const char *cmdStr, uint8_t retries)
{
    if (g_ltem->atcmd->isOpen)
    {
        if (retries)
        {
            while(g_ltem->atcmd->isOpen)
            {
                retries--;
                if (retries == 0)
                    return false;

                lDelay(ACTION_LOCKRETRY_INTERVALml);
                //ip_recvDoWork();
            }
        }
        else
            return false;
    }
    s_atcmdInit(cmdStr);
    return true;
}


#pragma endregion

