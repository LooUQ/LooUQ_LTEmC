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
 *	\brief Sets options for BGx AT command control (atcmd). 
 *
 *	\param autoLock [in] - Enable autolocking mode (default).
 *  \param timeoutMS [in] Number of milliseconds the action can take. Use system default ACTION_TIMEOUTms or your value.
 *  \param taskCompleteParser [in] Custom command response parser to signal result is complete. NULL for std parser.
 * 
 *  \return True if action was invoked, false if not
 */
void atcmd_setOptions(bool lockModeAuto, uint32_t timeoutMS, uint16_t (*customCmdCompleteParser_func)(const char *response, char **endptr))
{
    ASSERT(!(lockModeAuto && ((atcmd_t*)g_ltem.atcmd)->isOpenLocked), srcfile_atcmd_c);               // cannot set autolock mode on if there is a pending lock

    ((atcmd_t*)g_ltem.atcmd)->autoLock = lockModeAuto;
    ((atcmd_t*)g_ltem.atcmd)->timeoutMS = (timeoutMS == 0) ? ATCMD__defaultTimeoutMS : timeoutMS;

    if (customCmdCompleteParser_func)
        ((atcmd_t*)g_ltem.atcmd)->completeParser_func = customCmdCompleteParser_func;
    else
        ((atcmd_t*)g_ltem.atcmd)->completeParser_func = atcmd_okResultParser;
}


/**
 *	\brief Reset AT command options to defaults.
 *  
 *  Use after setOptions() to return to default internal settings.
 */
inline void atcmd_restoreOptionDefaults()
{
    atcmd_setOptions(atcmd__setLockModeAuto, atcmd__useDefaultTimeout, atcmd__useDefaultOKCompletionParser);
}


/**
 *	\brief Invokes a BGx AT command using default option values.
 *
 *	\param cmdStrTemplate [in] The command string to send to the BG96 module.
 *  \param ... [in] Variadic parameter list to integrate into the cmdStrTemplate.
 * 
 *  \return True if action was invoked, false if not
 */
bool atcmd_tryInvokeDefaults(const char *cmdStrTemplate, ...)
{
    if (((atcmd_t*)g_ltem.atcmd)->isOpenLocked)
        return false;

    atcmd_reset(false);                                                 // clear atCmd control, no need to release lock as it is not set (above)
    atcmd_restoreOptionDefaults();                                      // standard behavior reset default option values

    char *cmdStr = ((atcmd_t*)g_ltem.atcmd)->cmdStr;
    va_list ap;

    va_start(ap, cmdStrTemplate);
    vsnprintf(cmdStr, sizeof(((atcmd_t*)g_ltem.atcmd)->cmdStr), cmdStrTemplate, ap);

    if (!atcmd_awaitLock(((atcmd_t*)g_ltem.atcmd)->timeoutMS))          // attempt to acquire new atCmd lock for this instance
        return false;

    ((atcmd_t*)g_ltem.atcmd)->invokedAt = pMillis();
    IOP_sendTx(((atcmd_t*)g_ltem.atcmd)->cmdStr, strlen(((atcmd_t*)g_ltem.atcmd)->cmdStr), false);
    IOP_sendTx("\r", 1, true);
    return true;
}


/**
 *	\brief Invokes a BGx AT command using previously set option values: setOptions().
 *
 *	\param cmdStrTemplate [in] The command string to send to the BG96 module.
 *  \param ... [in] Variadic parameter list to integrate into the cmdStrTemplate.
 * 
 *  \return True if action was invoked, false if not
 */
bool atcmd_tryInvokeOptions(const char *cmdStrTemplate, ...)
{
    if (((atcmd_t*)g_ltem.atcmd)->isOpenLocked)
        return false;

    atcmd_reset(false);                                                 // clear atCmd control, no need to release lock as it is not set (above)

    char *cmdStr = ((atcmd_t*)g_ltem.atcmd)->cmdStr;
    va_list ap;

    va_start(ap, cmdStrTemplate);
    vsnprintf(cmdStr, sizeof(((atcmd_t*)g_ltem.atcmd)->cmdStr), cmdStrTemplate, ap);

    if (!atcmd_awaitLock(((atcmd_t*)g_ltem.atcmd)->timeoutMS))          // attempt to acquire new atCmd lock for this instance
        return false;

    ((atcmd_t*)g_ltem.atcmd)->invokedAt = pMillis();
    IOP_sendTx(((atcmd_t*)g_ltem.atcmd)->cmdStr, strlen(((atcmd_t*)g_ltem.atcmd)->cmdStr), false);
    IOP_sendTx("\r", 1, true);
    return true;
}


/**
 *	\brief Invokes a BGx AT command without acquiring a lock, using previously set setOptions() values.
 *
 *	\param cmdStrTemplate [in] The command string to send to the BG96 module.
 *  \param ... [in] Variadic parameter list to integrate into the cmdStrTemplate.
 */
void atcmd_invokeReuseLock(const char *cmdStrTemplate, ...)
{
    ASSERT(((atcmd_t*)g_ltem.atcmd)->isOpenLocked, srcfile_atcmd_c);            // function assumes re-use of existing lock
    atcmd_reset(false);                                                         // clear out properties WITHOUT lock release

    char *cmdStr = ((atcmd_t*)g_ltem.atcmd)->cmdStr;
    va_list ap;

    va_start(ap, cmdStrTemplate);
    vsnprintf(cmdStr, sizeof(((atcmd_t*)g_ltem.atcmd)->cmdStr), cmdStrTemplate, ap);

    ((atcmd_t*)g_ltem.atcmd)->invokedAt = pMillis();
    IOP_sendTx(((atcmd_t*)g_ltem.atcmd)->cmdStr, strlen(((atcmd_t*)g_ltem.atcmd)->cmdStr), false);
    IOP_sendTx("\r", 1, true);
}


/**
 *	\brief Closes (completes) a BGx AT command structure and frees action resource (release action lock).
 */
void atcmd_close()
{
    ((atcmd_t*)g_ltem.atcmd)->isOpenLocked = false;
    ((atcmd_t*)g_ltem.atcmd)->execDuration = pMillis() - ((atcmd_t*)g_ltem.atcmd)->invokedAt;
}


/**
 *	\brief Performs data transfer (send) sub-action.

 *  \param data [in] - Pointer to the block of binary data to send.
 *  \param dataSz [in] - The size of the data block. 
 *  \param timeoutMillis [in] - Timeout period (in millisecs) for send to complete.
 *  \param taskCompleteParser_func [in] - Function pointer to parser looking for task completion
 */
void atcmd_sendCmdData(const char *data, uint16_t dataSz, const char* eotPhrase)
{
    if (((atcmd_t*)g_ltem.atcmd)->invokedAt == 0)
        ((atcmd_t*)g_ltem.atcmd)->invokedAt = pMillis();


    IOP_sendTx(data, dataSz, true);

    if (strlen(eotPhrase) > 0 )
        IOP_sendTx(eotPhrase, strlen(eotPhrase), true);
}


// /**
//  *	\brief Waits for an AT command result until completed response or timeout.
//  *
//  *  \param autoreleaseLockOnComplete [in] USE WITH CAUTION - On result, close the action. The caller only needs the status code. 
//  * 
//  *  \return Action result. WARNING - the actionResult.response contents are undetermined after close. 
//  */
// atcmdResult_t atcmd_awaitResult(bool autoreleaseLockOnComplete)
// {
//     atcmdResult_t actionResult;
//     do
//     {
//         actionResult = ATCMD_getResult(autoreleaseLockOnComplete);
//         if (g_ltem.cancellationRequest)                         // test for cancellation (RTOS or IRQ)
//         {
//             actionResult.response = 0;
//             actionResult.statusCode = resultCode__cancelled;
//             break;
//         }
//         pYield();                                               // give back control momentarily
//     } while (actionResult.statusCode == ATCMD__parserPendingResult);
    
//     return actionResult;
// }


resultCode_t atcmd_awaitResult()
{
    resultCode_t result;
    do
    {
        ATCMD_getResult();
        if (g_ltem.cancellationRequest)                         // test for cancellation (RTOS or IRQ)
        {
            ((atcmd_t*)g_ltem.atcmd)->resultCode = resultCode__cancelled;
            break;
        }
        pYield();                                               // give back control momentarily

    } while (((atcmd_t*)g_ltem.atcmd)->resultCode == ATCMD__parserPendingResult);
    
    return ((atcmd_t*)g_ltem.atcmd)->resultCode;
}



// /**
//  *	\brief Gets command response and returns immediately.
//  *
//  *  \param releaseLockOnComplete [in] - USE WITH CAUTION - On result, close the action. The caller only needs the status code. 
//  * 
//  *  \return Action result. WARNING - the actionResult.response contents are undetermined after close. 
//  */
// atcmdResult_t ATCMD_getResult(bool releaseLockOnComplete)
// {
//     atcmdResult_t result = {.statusCode = ATCMD__parserPendingResult, .response = ((iop_t*)g_ltem.iop)->rxCBuffer->_buffer};    // default response
//     char *endptr = NULL;

//     if (((iop_t*)g_ltem.iop)->rxCBuffer->buffer[0] != 0)                                                   // if cmd buffer not empty, test for command complete with parser
//     {
//         ((atcmd_t*)g_ltem.atcmd)->resultCode = (*((atcmd_t*)g_ltem.atcmd)->taskCompleteParser_func)(((iop_t*)g_ltem.iop)->rxCBuffer->buffer, &endptr);
//         PRINTF(dbgColor__gray, "prsr=%d \r", ((atcmd_t*)g_ltem.atcmd)->resultCode);
//     }

//     if (((atcmd_t*)g_ltem.atcmd)->resultCode == ATCMD__parserPendingResult)                                     // check for timeout error
//     {
//         if (lTimerExpired(((atcmd_t*)g_ltem.atcmd)->invokedAt, ((atcmd_t*)g_ltem.atcmd)->timeoutMS))
//         {
//             result.statusCode = resultCode__timeout;
//             ((atcmd_t*)g_ltem.atcmd)->resultCode = resultCode__timeout;
//             ((atcmd_t*)g_ltem.atcmd)->isOpenLocked = false;                                                        // close action to release action lock
            
//             // if action timed-out, verify not a device wide failure
//             if (!ltem_chkHwReady())
//                 ltem_notifyApp(lqNotificationType_lqDevice_hwFault, "Modem HW Status Offline");                  // BGx status pin
//             else if (!SC16IS741A_chkCommReady())
//                 ltem_notifyApp(lqNotificationType_lqDevice_hwFault, "Modem comm unresponsive");                  // UART bridge SPI wr/rd

//             ((atcmd_t*)g_ltem.atcmd)->execDuration = pMillis() - ((atcmd_t*)g_ltem.atcmd)->invokedAt;
//         }
//         return result;
//     }

//     result.statusCode = ((atcmd_t*)g_ltem.atcmd)->resultCode;                            // parser completed, set return status code (could be success or error)
//     ((iop_t*)g_ltem.iop)->rxCBuffer->buffer = endptr;                                    // adj prevHead to unparsed cmd stream

//     // if parser left data trailing parsed content in cmd buffer: need to parseImmediate() for URCs, as if they just arrived
//     if (((iop_t*)g_ltem.iop)->rxCBuffer->buffer < ((iop_t*)g_ltem.iop)->rxCBuffer->head)      
//         IOP_rxParseForEvents();

//     if (result.statusCode <= resultCode__successMax)                                // parser completed with success code
//     {
//         if (releaseLockOnComplete)
//             ((atcmd_t*)g_ltem.atcmd)->isOpenLocked = false;
//         return result;
//     }

//     // handled timeout and success results above, if here must be specific error
//     ((atcmd_t*)g_ltem.atcmd)->isOpenLocked = false;                                       // close action to release action lock on any error
//     ((atcmd_t*)g_ltem.atcmd)->execDuration = pMillis() - ((atcmd_t*)g_ltem.atcmd)->invokedAt;
//     return result;
// }


void ATCMD_getResult()
{
    atcmd_t *atcmdPtr = (atcmd_t*)g_ltem.atcmd;
    iop_t *iopPtr = (iop_t*)g_ltem.iop;
    atcmdPtr->resultCode = ATCMD__parserPendingResult;
    
    char *endptr = NULL;
    if (*iopPtr->rxCBuffer->tail != '\0')                               // if cmd buffer not empty, test for command complete with registered parser
    {
        if (atcmdPtr->response == NULL)                                 // set response pointer now that there is new data
            atcmdPtr->response = iopPtr->rxCBuffer->tail;               // note: this doesn't track with parser, shows full response text 

        atcmdPtr->resultCode = (*atcmdPtr->completeParser_func)(iopPtr->rxCBuffer->tail, &endptr);
        PRINTF(dbgColor__gray, "prsr=%d \r", atcmdPtr->resultCode);
    }

    if (atcmdPtr->resultCode == ATCMD__parserPendingResult)             // still pending, check for timeout error
    {
        if (pElapsed(atcmdPtr->invokedAt, atcmdPtr->timeoutMS))
        {
            atcmdPtr->resultCode = resultCode__timeout;
            atcmdPtr->isOpenLocked = false;                             // close action to release action lock
            atcmdPtr->execDuration = pMillis() - atcmdPtr->invokedAt;

            if (!ltem_chkHwReady())                                                                 // if action timed-out, verify not a device wide failure
                ltem_notifyApp(lqNotificationType_lqDevice_hwFault, "Modem HW Status Offline");     // BGx status pin
            else if (!SC16IS741A_chkCommReady())
                ltem_notifyApp(lqNotificationType_lqDevice_hwFault, "Modem comm unresponsive");     // UART bridge SPI wr/rd
        }
        return;
    }
    else
    {
        iopPtr->rxCBuffer->tail = endptr;                               // adj tail to unparsed cmd stream
        atcmdPtr->responseTail = endptr;
                                                                        // if parser left data trailing parsed content in cmd buffer: 
        if (iopPtr->rxCBuffer->tail < iopPtr->rxCBuffer->head)          // - need to parseImmediate() for URCs, as if they just arrived
            IOP_rxParseForEvents();
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
 *	\brief Sends ESC character to ensure BGx is not in text mode (> prompt awaiting ^Z/ESC).
 */
void atcmd_exitTextMode()
{
    IOP_sendTx("\x1B", 1, true);            // send ESC - 0x1B
}


/**
 *	\brief Sends +++ sequence to transition BGx out of data mode to command mode.
 */
void atcmd_exitDataMode()
{
    lDelay(1000);
    IOP_sendTx("+++", 1, true);             // send +++, gaurded by 1 second of quiet
    lDelay(1000);
}


/**
 *	\brief Returns the atCmd result code, 0xFFFF or ATCMD__parserPendingResult if command is pending completion
 */
resultCode_t atcmd_getLastResult()
{
    return ((atcmd_t*)g_ltem.atcmd)->resultCode;
}


/**
 *	\brief Returns the atCmd last execution duration in milliseconds
 */
uint32_t atcmd_getLastDuration()
{
    return ((atcmd_t*)g_ltem.atcmd)->execDuration;
}


/**
 *	\brief Returns the atCmd response text if completed, will be empty C-string prior to completion
 */
char *atcmd_getLastResponse()
{
    return ((atcmd_t*)g_ltem.atcmd)->response;
}

char *atcmd_getLastResponseTail()
{
    return ((atcmd_t*)g_ltem.atcmd)->responseTail;
}

#pragma endregion


#pragma region LTEmC Internal Functions
/*-----------------------------------------------------------------------------------------------*/


/**
 *  \brief Awaits exclusive access to QBG module command interface.
 * 
 *  \param cmdStr [in] - The AT command text.
 *  \param waitMillis [in] - Number of milliseconds to wait for a lock.
 * 
 *  \return true if lock aquired prior to the timeout period.
*/
bool atcmd_awaitLock(uint16_t timeoutMS)
{
    timeoutMS = (timeoutMS == atcmd__useDefaultTimeout) ? ATCMD__defaultTimeoutMS : timeoutMS;

    if (((atcmd_t*)g_ltem.atcmd)->isOpenLocked)
    {
        uint32_t waitNow, waitStart = pMillis();
        do
        {
            pYield();
            if (!((atcmd_t*)g_ltem.atcmd)->isOpenLocked)      // break out on atCmd (!open)
            {
                ((atcmd_t*)g_ltem.atcmd)->isOpenLocked = true;
                return true;
            }
            waitNow = pMillis();
        } while (waitNow - waitStart < timeoutMS);
        return false;
    }
    ((atcmd_t*)g_ltem.atcmd)->isOpenLocked = true;
    return true;
}


/**
 *	\brief Returns the current atCmd lock state
 */
bool atcmd_isLockActive()
{
    return ((atcmd_t*)g_ltem.atcmd)->isOpenLocked;
}


/**
 *	\brief Initializes an atCmd struct, a BGx AT command structure.
 */
void atcmd_reset(bool releaseOpenLock)
{
    /* clearing req/resp buffers now for debug clarity, future likely just insert starting \0 */

    // request side of action
    if (releaseOpenLock)
    {
        ((atcmd_t*)g_ltem.atcmd)->isOpenLocked = false;                         // reset current lock
    }

    memset(((atcmd_t*)g_ltem.atcmd)->cmdStr, 0, ATCMD__commandBufferSz);
    // strncpy(((atcmd_t*)g_ltem.atcmd)->cmdStr, cmdStr, strlen(cmdStr));
    ((atcmd_t*)g_ltem.atcmd)->resultCode = ATCMD__parserPendingResult;
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
                return ATCMD__parserPendingResult;
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
                return resultCode__internalError;
            }
        }
        else if (!terminatorAt)
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

    return ATCMD__parserPendingResult;                                                 // no term, keep looking
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
    return ATCMD__parserPendingResult;
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
        return ATCMD__parserPendingResult;

    next += strlen(preamble);
    int16_t resultVal;

    // expected form: +<preamble>: <some other info>,<RESULT_CODE>
    // return (200 + RESULT_CODE)
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
 *	\brief Response parser looking for "ready-to-proceed" prompt in order to send to network
 *
 *  \param response [in] The character string received from BGx (so far, may not be complete).
 *  \param rdyPropmpt [in] Prompt text to check for.
 * 
 *  \return Result code enum value (http status code)
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
    return ATCMD__parserPendingResult;
}


/**
 *	\brief [private] Transmit data ready to send "data prompt" parser.
 *
 *  \param response [in] Character data recv'd from BGx to parse for task complete
 *  \param endptr [out] Char pointer to the char following parsed text
 * 
 *  \return HTTP style result code, 0 = not complete
 */
resultCode_t atcmd_txDataPromptParser(const char *response, char **endptr) 
{
    return atcmd_readyPromptParser(response, "> ", endptr);
}


/**
 *	\brief "CONNECT" prompt parser.
 *
 *  \param response [in] Character data recv'd from BGx to parse for task complete
 *  \param endptr [out] Char pointer to the char following parsed text
 * 
 *  \return HTTP style result code, 0 = not complete
 */
resultCode_t atcmd_connectPromptParser(const char *response, char **endptr)
{
    return atcmd_readyPromptParser(response, "CONNECT\r\n", endptr);
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


#pragma region Static Function Definitions
/*-----------------------------------------------------------------------------------------------*/

/**
 *	\brief peforms that actual AT command invoke action. Both atcmd_tryInvoke() and atcmd_tryInvokeAdv() use this.
 */
static bool S_invokeCmd()
{
}


// /**
//  *	\brief Copies response\result information at action conclusion. Designed as a diagnostic aid for failed AT actions.
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
