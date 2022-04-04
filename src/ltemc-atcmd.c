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
#if _DEBUG > 0
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG == 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #define PRINTF(c_,f_,__VA_ARGS__...) do { rtt_printf(c_, (f_), ## __VA_ARGS__); } while(0)
    #endif
#else
#define PRINTF(c_, f_, ...) 
#endif


#include "ltemc-internal.h"
#include <stdarg.h>


#define MIN(x, y) (((x) < (y)) ? (x) : (y))


extern ltemDevice_t g_ltem;


/* Static Function Declarations
------------------------------------------------------------------------------------------------- */


#pragma region Public Functions
/*-----------------------------------------------------------------------------------------------*/

/**
 *	@brief Sets options for BGx AT command control (atcmd). 
 */
void atcmd_setOptions(uint32_t timeoutMS, uint16_t (*customCmdCompleteParser_func)(const char *response, char **endptr))
{
    ((atcmd_t*)g_ltem.atcmd)->timeoutMS = (timeoutMS == 0) ? atcmd__defaultTimeoutMS : timeoutMS;

    if (customCmdCompleteParser_func)
        ((atcmd_t*)g_ltem.atcmd)->completeParser_func = customCmdCompleteParser_func;
    else
        ((atcmd_t*)g_ltem.atcmd)->completeParser_func = atcmd_okResultParser;
}


/**
 *	@brief Reset AT command options to defaults.
 */
inline void atcmd_restoreOptionDefaults()
{
    atcmd_setOptions(atcmd__defaultTimeoutMS, atcmd__useDefaultOKCompletionParser);
}


/**
 *	@brief Invokes a BGx AT command using default option values (automatic locking).
 */
bool atcmd_tryInvoke(const char *cmdTemplate, ...)
{
    if (((atcmd_t*)g_ltem.atcmd)->isOpenLocked || ((iop_t*)g_ltem.iop)->rxStreamCtrl != NULL)
        return false;

    ATCMD_reset(true);                                                  // clear atCmd control
    ((atcmd_t*)g_ltem.atcmd)->autoLock = atcmd__setLockModeAuto;        // set automatic lock control mode
    atcmd_restoreOptionDefaults();                                      // standard behavior reset default option values

    char *cmdStr = ((atcmd_t*)g_ltem.atcmd)->cmdStr;
    va_list ap;

    va_start(ap, cmdTemplate);
    vsnprintf(cmdStr, sizeof(((atcmd_t*)g_ltem.atcmd)->cmdStr), cmdTemplate, ap);

    if (!ATCMD_awaitLock(((atcmd_t*)g_ltem.atcmd)->timeoutMS))          // attempt to acquire new atCmd lock for this instance
        return false;

    ((atcmd_t*)g_ltem.atcmd)->invokedAt = pMillis();
    IOP_sendTx(((atcmd_t*)g_ltem.atcmd)->cmdStr, strlen(((atcmd_t*)g_ltem.atcmd)->cmdStr), false);
    IOP_sendTx("\r", 1, true);
    return true;
}


/**
 *	@brief Invokes a BGx AT command using set option values, previously set with setOptions() (automatic locking).
 */
bool atcmd_tryInvokeWithOptions(const char *cmdTemplate, ...)
{
    if (((atcmd_t*)g_ltem.atcmd)->isOpenLocked || ((iop_t*)g_ltem.iop)->rxStreamCtrl != NULL)
        return false;

    ATCMD_reset(true);                                                  // clear atCmd control
    ((atcmd_t*)g_ltem.atcmd)->autoLock = atcmd__setLockModeAuto;        // set automatic lock control mode

    char *cmdStr = ((atcmd_t*)g_ltem.atcmd)->cmdStr;
    va_list ap;

    va_start(ap, cmdTemplate);
    vsnprintf(cmdStr, sizeof(((atcmd_t*)g_ltem.atcmd)->cmdStr), cmdTemplate, ap);

    if (!ATCMD_awaitLock(((atcmd_t*)g_ltem.atcmd)->timeoutMS))          // attempt to acquire new atCmd lock for this instance
        return false;

    ((atcmd_t*)g_ltem.atcmd)->invokedAt = pMillis();
    IOP_sendTx(((atcmd_t*)g_ltem.atcmd)->cmdStr, strlen(((atcmd_t*)g_ltem.atcmd)->cmdStr), false);
    IOP_sendTx("\r", 1, true);
    return true;
}


/**
 *	@brief Invokes a BGx AT command without acquiring a lock, using previously set setOptions() values.
 */
void atcmd_invokeReuseLock(const char *cmdTemplate, ...)
{
    ASSERT(((atcmd_t*)g_ltem.atcmd)->isOpenLocked, srcfile_ltemc_atcmd_c);            // function assumes re-use of existing lock

    ATCMD_reset(false);                                                         // clear out properties WITHOUT lock release
    ((atcmd_t*)g_ltem.atcmd)->autoLock = atcmd__setLockModeManual;

    char *cmdStr = ((atcmd_t*)g_ltem.atcmd)->cmdStr;
    va_list ap;

    va_start(ap, cmdTemplate);
    vsnprintf(cmdStr, sizeof(((atcmd_t*)g_ltem.atcmd)->cmdStr), cmdTemplate, ap);

    ((atcmd_t*)g_ltem.atcmd)->invokedAt = pMillis();
    IOP_sendTx(((atcmd_t*)g_ltem.atcmd)->cmdStr, strlen(((atcmd_t*)g_ltem.atcmd)->cmdStr), false);
    IOP_sendTx("\r", 1, true);
}


/**
 *	@brief Closes (completes) a BGx AT command structure and frees action resource (release action lock).
 */
void atcmd_close()
{
    ((atcmd_t*)g_ltem.atcmd)->isOpenLocked = false;
    ((atcmd_t*)g_ltem.atcmd)->execDuration = pMillis() - ((atcmd_t*)g_ltem.atcmd)->invokedAt;
}


/**
 *	@brief Performs blind send data transfer to device.
 */
void atcmd_sendCmdData(const char *data, uint16_t dataSz, const char* eotPhrase)
{
    if (((atcmd_t*)g_ltem.atcmd)->invokedAt == 0)
        ((atcmd_t*)g_ltem.atcmd)->invokedAt = pMillis();

    IOP_sendTx(data, dataSz, true);

    if (strlen(eotPhrase) > 0 )
        IOP_sendTx(eotPhrase, strlen(eotPhrase), true);
}


/**
 *	@brief Checks recv buffer for command response and sets atcmd structure data with result.
 */
void atcmd_getResult()
{
    atcmd_t *atcmdPtr = (atcmd_t*)g_ltem.atcmd;
    iop_t *iopPtr = (iop_t*)g_ltem.iop;
    atcmdPtr->resultCode = atcmd__parserPendingResult;
    
    char *endptr = NULL;
    if (*iopPtr->rxCBuffer->tail != '\0')                               // if cmd buffer not empty, test for command complete with registered parser
    {
        if (atcmdPtr->response == NULL)                                 // set response pointer now that there is new data
            atcmdPtr->response = iopPtr->rxCBuffer->tail;               // note: this doesn't track with parser, shows full response text 

        atcmdPtr->resultCode = (*atcmdPtr->completeParser_func)(iopPtr->rxCBuffer->tail, &endptr);
        PRINTF(dbgColor__gray, "prsr=%d \r", atcmdPtr->resultCode);
    }

    if (atcmdPtr->resultCode == atcmd__parserPendingResult)             // still pending, check for timeout error
    {
        if (pElapsed(atcmdPtr->invokedAt, atcmdPtr->timeoutMS))
        {
            atcmdPtr->resultCode = resultCode__timeout;
            atcmdPtr->isOpenLocked = false;                             // close action to release action lock
            atcmdPtr->execDuration = pMillis() - atcmdPtr->invokedAt;

            if (!ltem_readDeviceState())                                                  // if action timed-out, verify not a device wide failure
                ltem_notifyApp(appEvent_fault_hardLogic, "Fault-LTEm Not On");            // 0 read = BGx status pin low
            else if (!SC16IS7xx_chkCommReady())
                ltem_notifyApp(appEvent_fault_softLogic, "Fault-LTEm SPI");               // UART bridge SPI not initialized correctly, IRQ not enabled
        }
        return;
    }
    else
    {
        iopPtr->rxCBuffer->tail = endptr;                               // adj tail to unparsed cmd stream
        atcmdPtr->responseTail = endptr;
                                                                        // if parser left data trailing parsed content in cmd buffer: 
        if (iopPtr->rxCBuffer->tail < iopPtr->rxCBuffer->head)          // - need to parseImmediate() for URCs, as if they just arrived
            IOP_rxParseForUrcEvents();
    }

    if (atcmdPtr->resultCode <= resultCode__successMax)                 // parser completed with success code
    {
        if (atcmdPtr->autoLock)
            atcmdPtr->isOpenLocked = false;
        atcmdPtr->execDuration = pMillis() - atcmdPtr->invokedAt;
        return;
    }

    // handled timeout and success results above, if here must be specific error
    atcmdPtr->isOpenLocked = false;                                     // close action to release action lock on any error
    atcmdPtr->execDuration = pMillis() - atcmdPtr->invokedAt;
}


/**
 *	@brief Waits for atcmd result, periodically checking recv buffer for valid response until timeout.
 */
resultCode_t atcmd_awaitResult()
{
    resultCode_t result;
    do
    {
        atcmd_getResult();
        if (g_ltem.cancellationRequest)                         // test for cancellation (RTOS or IRQ)
        {
            ((atcmd_t*)g_ltem.atcmd)->resultCode = resultCode__cancelled;
            break;
        }
        pYield();                                               // give back control momentarily

    } while (((atcmd_t*)g_ltem.atcmd)->resultCode == atcmd__parserPendingResult);
    
    return ((atcmd_t*)g_ltem.atcmd)->resultCode;
}

/**
 *	@brief Returns the atCmd result code, 0xFFFF or atcmd__parserPendingResult if command is pending completion
 */
resultCode_t atcmd_getLastResult()
{
    return ((atcmd_t*)g_ltem.atcmd)->resultCode;
}


/**
 *	@brief Returns the atCmd last execution duration in milliseconds
 */
uint32_t atcmd_getLastDuration()
{
    return ((atcmd_t*)g_ltem.atcmd)->execDuration;
}


/**
 *	@brief Returns the atCmd response text if completed, will be empty C-string prior to completion
 */
char *ATCMD_getLastResponse()
{
    return ((atcmd_t*)g_ltem.atcmd)->response;
}


/**
 *	@brief Returns the atCmd response text beyond internal completion parser's progress
 */
char *ATCMD_getLastResponseTail()
{
    return ((atcmd_t*)g_ltem.atcmd)->responseTail;
}


/**
 *	@brief Sends ESC character to ensure BGx is not in text mode (">" prompt awaiting ^Z/ESC, MQTT publish etc.).
 */
void atcmd_exitTextMode()
{
    IOP_sendTx("\x1B", 1, true);            // send ESC - Ctrl-[
}


/**
 *	@brief Sends +++ sequence to transition BGx out of data mode to command mode.
 */
void atcmd_exitDataMode()
{
    lDelay(1000);
    IOP_sendTx("+++", 1, true);             // send +++, gaurded by 1 second of quiet
    lDelay(1000);
}



#pragma endregion


#pragma region LTEmC Internal Functions
/*-----------------------------------------------------------------------------------------------*/


/**
 *  @brief Awaits exclusive access to QBG module command interface.
*/
bool ATCMD_awaitLock(uint16_t timeoutMS)
{
    uint32_t waitStart = pMillis();
                                                                    // cannot set lock while... 
    // while (((atcmd_t*)g_ltem.atcmd)->isOpenLocked ||                // - existing lock
    //        ((iop_t*)g_ltem.iop)->rxStreamCtrl != NULL)              // - IOP is in data mode (handling a receive)
    // {
    //     pYield();                                                   // call back to platform yield() in case there is work there
    //     if (((iop_t*)g_ltem.iop)->rxStreamCtrl != NULL)
    //         ltem_doWork();                                          // if data receive is blocking, give doWork an opportunity to resolve

    //     if (pMillis() - waitStart > timeoutMS)
    //         return false;                                           // timed out waiting for lock
    // }
    // ((atcmd_t*)g_ltem.atcmd)->isOpenLocked = true;                  // acquired lock
    // return true;

    while (pMillis() - waitStart < timeoutMS)
    {                                                               // can set new lock if...
        if (!((atcmd_t*)g_ltem.atcmd)->isOpenLocked &&              // !not existing lock
            ((iop_t*)g_ltem.iop)->rxStreamCtrl == NULL)             // IOP is not in data mode (handling a receive)
        {
            ((atcmd_t*)g_ltem.atcmd)->isOpenLocked = true;          // acquired new lock
            return true;
        }

        pYield();                                                   // call back to platform yield() in case there is work there that can be done
        if (((iop_t*)g_ltem.iop)->rxStreamCtrl != NULL)
            ltem_doWork();                                          // if data receive is blocking, give doWork an opportunity to resolve
    }
    return false;                                                   // timed out waiting for lock
}


/**
 *	@brief Returns the current atCmd lock state
 */
bool ATCMD_isLockActive()
{
    return ((atcmd_t*)g_ltem.atcmd)->isOpenLocked;
}


/**
 *	@brief Resets atCmd struct and optionally releases lock, a BGx AT command structure.
 */
void ATCMD_reset(bool releaseOpenLock)
{
    /* clearing req/resp buffers now for debug clarity, future likely just insert '\0' */

    // request side of action
    if (releaseOpenLock)
    {
        ((atcmd_t*)g_ltem.atcmd)->isOpenLocked = false;                         // reset current lock
    }

    memset(((atcmd_t*)g_ltem.atcmd)->cmdStr, 0, IOP__rxCoreBufferSize);
    ((atcmd_t*)g_ltem.atcmd)->resultCode = atcmd__parserPendingResult;
    ((atcmd_t*)g_ltem.atcmd)->invokedAt = 0;

    // response side
    IOP_resetCoreRxBuffer();
}


#pragma endregion // LTEmC Internal Functions 


#pragma region Completion Parsers
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
 *	@brief Performs a standardized parse of command responses. This function can be wrapped to match atcmd commandCompletedParse signature.
 *
 *  @param response [in] - The string returned from the getResult function.
 *  @param preamble [in] - The string to look for to signal start of response match.
 *  @param preambleReqd [in] - The preamble string is required, set to false to search for only gap and terminator
 *  @param gapReqd [in] - The minimum char count between preamble (or start) and terminator (0 is valid).
 *  @param terminator [in] - The string to signal the end of the command response.
 *  @param endptr [out] - Char pointer to the next char in response after parser match
 * 
 *  @return HTTP style result code, 0 if parser incomplete (need more response). OK = 200.
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
                return atcmd__parserPendingResult;
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
        if (!terminatorAt)                                              
        {
            terminatorAt = strstr(termSearchAt, CME_PREABLE);                  // no explicit terminator, look for extended CME errors
            if (terminatorAt)
            {
                // return error code, all CME >= 500
                uint16_t cmeVal = strtol(terminatorAt + CME_PREABLE_SZ, endptr, 10);        // strtol will set endptr
                return cmeVal;
            }
        }
        if (!terminatorAt)
        {
            terminatorAt = strstr(termSearchAt, ERROR_COMPLETED_STRING);        // no explicit terminator, look for ERROR
            if (terminatorAt)
            {
                *endptr = terminatorAt + 7;     // + strlen(ERROR_COMPLETED_STRING)
                return resultCode__internalError;
            }
        }
        if (!terminatorAt)
        {
            terminatorAt = strstr(termSearchAt, FAIL_COMPLETED_STRING);         // no explicit terminator, look for FAIL
            if (terminatorAt)
            {
                *endptr = terminatorAt + 6;     // + strlen(FAIL_COMPLETED_STRING)
                return resultCode__internalError;
            }
        }
    }

    if (terminatorAt && termSearchAt + gapReqd <= terminatorAt)                 // explicit or implicit term found with sufficient gap
        return resultCode__success;
    if (terminatorAt)                                                           // else gap insufficient
        return resultCode__internalError;

    return atcmd__parserPendingResult;                                                 // no term, keep looking
}


/**
 *	@brief Performs a standardized parse of command responses. This function can be wrapped to match atcmd commandCompletedParse signature.
 *
 *  @param response [in] - The string returned from the getResult function.
 *  @param preamble [in] - The string to look for to signal response matches command (scans from end backwards).
 *  @param delim [in] - The token delimeter char.
 *  @param reqdTokens [in] - The minimum number of tokens expected.
 *  @param terminator [in] - Text to look for indicating the sucessful end of command response.
 *  @param endptr [out] - Char pointer to the next char in response after parser match
 * 
 *  @return True if the response meets the conditions in the preamble and token count required.
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
            return resultCode__notFound;

        // now count delimitersFound
        char *next = preambleAt + strlen(preamble) + 1;

        while (next < terminatorAt && next != NULL)
        {
            next = strchr(++next, delim);
            delimitersFound += (next == NULL) ? 0 : 1;
        }

        if (++delimitersFound >= reqdTokens)
            return resultCode__success;
        return resultCode__notFound;
    }

    char *cmeErrorCodeAt = strstr(response, CME_PREABLE);           // CME error codes generated by BG96
    if (cmeErrorCodeAt)
    {
        // return error code, all CME >= 500
        return (strtol(cmeErrorCodeAt + CME_PREABLE_SZ, endptr, 10));
    }
    return atcmd__parserPendingResult;
}


/**
 *	@brief Validate the response ends in a BGxx OK value.
 *
 *	@param response [in] - Pointer to the command response received from the BGx.
 *  @param endptr [out] - Char pointer to the next char in response after parser match
 *
 *  @return bool If the response string ends in a valid OK sequence
 */
resultCode_t atcmd_okResultParser(const char *response, char** endptr)
{
    return atcmd_defaultResultParser(response, "", false, 0, NULL, endptr);
}


/**
 *	@brief Parser for open connection response, shared by UDP/TCP/SSL/MQTT.
 *
 *  Expected form: +[landmark]: [otherInfo],[RESULT_CODE],[otherInfo][EOL]
 * 
 *  @param response [in] - Pointer to the command response received from the BGx.
 *  @param landmark [in] - Pointer to a landmark phrase to look for in the response
 *  @param resultIndx [in] - Zero based index after landmark of numeric fields to find result
 *  @param endptr [out] - Pointer to character in response following parser match
 *
 *  @return 200 + RESULT_CODE, if satisfies, otherwise PendingResult code
 */
resultCode_t atcmd_serviceResponseParser(const char *response, const char *landmark, uint8_t resultIndx, char** endptr) 
{
    char *next = strstr(response, landmark);
    if (next == NULL)
        return atcmd__parserPendingResult;

    next += strlen(landmark);
    int16_t resultVal;

    for (size_t i = 0; i < resultIndx; i++)
    {
        if (next == NULL)
            break;
        next = strchr(next, ',');
        next++;         // point past comma
    }
    if (next != NULL)
        resultVal = (uint16_t)strtol(next, endptr, 10);

    // return a success value 200 + result (range: 200 - 300)
    return  (resultVal < 99) ? resultCode__success + resultVal : resultVal;
}


/**
 *	@brief Parser for open connection response, shared by UDP/TCP/SSL/MQTT.
 *
 *  Expected form: +[landmark]: [otherInfo],[RESULT_CODE],[otherInfo][EOL]
 *  @param response [in] - Pointer to the command response received from the BGx.
 *  @param landmark [in] - Pointer to a landmark phrase to look for in the response.
 *  @param resultIndx [in] - Zero based index after landmark of numeric fields to find result.
 *  @param terminator [in] - Character string signaling then end of action response parsing.
 *  @param endptr [out] - Pointer to character in response following parser match.
 *
 *  @return 200 + RESULT_CODE, if satisfies, otherwise PendingResult code.
 */
resultCode_t atcmd_serviceResponseParserTerm(const char *response, const char *landmark, uint8_t resultIndx, const char *terminator, char** endptr) 
{
    char *next = strstr(response, landmark);
    if (next == NULL)
        return atcmd__parserPendingResult;

    next += strlen(landmark);
    int16_t resultVal;

    uint8_t respSz = strlen(next);                                                  // check for terminator, usually CR/LF
    char * termChk = next + strlen(next) - strlen(terminator);
    if (strstr(termChk, terminator) == NULL)
        return atcmd__parserPendingResult;                                          

    for (size_t i = 0; i < resultIndx; i++)
    {
        if (next == NULL)
            break;
        next = strchr(next, ',');
        next++;                                                                     // point past comma
    }
    if (next != NULL)
        resultVal = (uint16_t)strtol(next, endptr, 10);

    return  (resultVal < 99) ? resultCode__success + resultVal : resultVal;         // return a success value 200 + result (range: 200 - 300)
}


/**
 *	@brief Response parser looking for "ready-to-proceed" prompt in order to send to network
 *
 *  @param response [in] The character string received from BGx (so far, may not be complete).
 *  @param rdyPrompt [in] Prompt text to check for.
 *  @param endptr [out] Pointer to the char after match.
 * 
 *  @return Result code enum value (http status code)
 */
resultCode_t atcmd_readyPromptParser(const char *response, const char *rdyPrompt, char **endptr)
{
    *endptr = strstr(response, rdyPrompt);
    if (*endptr != NULL)
    {
        *endptr += strlen(rdyPrompt);                   // point past data prompt
        return resultCode__success;
    }
    *endptr = strstr(response, "ERROR");
    if (*endptr != NULL)
    {
        return resultCode__internalError;
    }
    return atcmd__parserPendingResult;
}


/**
 *	@brief [private] Transmit data ready to send "data prompt" parser.
 *
 *  @param response [in] Character data recv'd from BGx to parse for task complete
 *  @param endptr [out] Char pointer to the char following parsed text
 * 
 *  @return HTTP style result code, 0 = not complete
 */
resultCode_t atcmd_txDataPromptParser(const char *response, char **endptr) 
{
    return atcmd_readyPromptParser(response, "> ", endptr);
}


/**
 *	@brief "CONNECT" prompt parser.
 *
 *  @param response [in] Character data recv'd from BGx to parse for task complete
 *  @param endptr [out] Char pointer to the char following parsed text
 * 
 *  @return HTTP style result code, 0 = not complete
 */
resultCode_t atcmd_connectPromptParser(const char *response, char **endptr)
{
    return atcmd_readyPromptParser(response, "CONNECT\r\n", endptr);
}


/**
 *	@brief C-string token grabber.
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


#pragma region Static Function Definitions
/*-----------------------------------------------------------------------------------------------*/

// /**
//  *	@brief Copies response\result information at action conclusion. Designed as a diagnostic aid for failed AT actions.
//  */
// static void S_copyToDiagnostics()
// {
//     memset(((atcmd_t*)g_ltem.atcmd)->lastActionError->cmdStr, 0, atcmd__historyBufferSz);
//     strncpy(((atcmd_t*)g_ltem.atcmd)->lastActionError->cmdStr, ((atcmd_t*)g_ltem.atcmd)->cmdStr, atcmd__historyBufferSz-1);
//     memset(((atcmd_t*)g_ltem.atcmd)->lastActionError->response, 0, atcmd__historyBufferSz);
//     strncpy(((atcmd_t*)g_ltem.atcmd)->lastActionError->response, ((atcmd_t*)g_ltem.atcmd)->response, atcmd__historyBufferSz-1);
//     ((atcmd_t*)g_ltem.atcmd)->lastActionError->statusCode = ((atcmd_t*)g_ltem.atcmd)->resultCode;
//     ((atcmd_t*)g_ltem.atcmd)->lastActionError->duration = pMillis() - ((atcmd_t*)g_ltem.atcmd)->invokedAt;
// }

#pragma endregion
