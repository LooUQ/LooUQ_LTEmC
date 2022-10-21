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


extern ltemDevice_t g_lqLTEM;


/* Static Function Declarations
------------------------------------------------------------------------------------------------- */


#pragma region Public Functions
/*-----------------------------------------------------------------------------------------------*/

/**
 *	\brief Sets options for BGx AT command control (atcmd). 
 */
void atcmd_setOptions(uint32_t timeoutMS, cmdResponseParser_func *cmdResponseParser)
{
    ((atcmd_t*)g_lqLTEM.atcmd)->timeoutMS = (timeoutMS == 0) ? atcmd__defaultTimeoutMS : timeoutMS;

    if (cmdResponseParser)
        ((atcmd_t*)g_lqLTEM.atcmd)->responseParserFunc = cmdResponseParser;
    else
        ((atcmd_t*)g_lqLTEM.atcmd)->responseParserFunc = atcmd_okResponseParser;
}


/**
 *	\brief Reset AT command options to defaults.
 */
inline void atcmd_restoreOptionDefaults()
{
    atcmd_setOptions(atcmd__defaultTimeoutMS, atcmd__useDefaultOKCompletionParser);
}


/**
 *	\brief Invokes a BGx AT command using default option values (automatic locking).
 */
bool atcmd_tryInvoke(const char *cmdTemplate, ...)
{
    if (((atcmd_t*)g_lqLTEM.atcmd)->isOpenLocked || ((iop_t*)g_lqLTEM.iop)->rxStreamCtrl != NULL)
        return false;

    ATCMD_reset(true);                                                  // clear atCmd control
    ((atcmd_t*)g_lqLTEM.atcmd)->autoLock = atcmd__setLockModeAuto;        // set automatic lock control mode
    atcmd_restoreOptionDefaults();                                      // standard behavior reset default option values

    char *cmdStr = ((atcmd_t*)g_lqLTEM.atcmd)->cmdStr;
    va_list ap;

    va_start(ap, cmdTemplate);
    vsnprintf(cmdStr, sizeof(((atcmd_t*)g_lqLTEM.atcmd)->cmdStr), cmdTemplate, ap);

    if (!ATCMD_awaitLock(((atcmd_t*)g_lqLTEM.atcmd)->timeoutMS))          // attempt to acquire new atCmd lock for this instance
        return false;

    ((atcmd_t*)g_lqLTEM.atcmd)->invokedAt = pMillis();
    IOP_sendTx(((atcmd_t*)g_lqLTEM.atcmd)->cmdStr, strlen(((atcmd_t*)g_lqLTEM.atcmd)->cmdStr), false);
    IOP_sendTx("\r", 1, true);
    return true;
}


/**
 *	\brief Invokes a BGx AT command using set option values, previously set with setOptions() (automatic locking).
 */
bool atcmd_tryInvokeWithOptions(const char *cmdTemplate, ...)
{
    if (((atcmd_t*)g_lqLTEM.atcmd)->isOpenLocked || ((iop_t*)g_lqLTEM.iop)->rxStreamCtrl != NULL)
        return false;

    ATCMD_reset(true);                                                  // clear atCmd control
    ((atcmd_t*)g_lqLTEM.atcmd)->autoLock = atcmd__setLockModeAuto;        // set automatic lock control mode

    char *cmdStr = ((atcmd_t*)g_lqLTEM.atcmd)->cmdStr;
    va_list ap;

    va_start(ap, cmdTemplate);
    vsnprintf(cmdStr, sizeof(((atcmd_t*)g_lqLTEM.atcmd)->cmdStr), cmdTemplate, ap);

    if (!ATCMD_awaitLock(((atcmd_t*)g_lqLTEM.atcmd)->timeoutMS))          // attempt to acquire new atCmd lock for this instance
        return false;

    ((atcmd_t*)g_lqLTEM.atcmd)->invokedAt = pMillis();
    IOP_sendTx(((atcmd_t*)g_lqLTEM.atcmd)->cmdStr, strlen(((atcmd_t*)g_lqLTEM.atcmd)->cmdStr), false);
    IOP_sendTx("\r", 1, true);
    return true;
}


/**
 *	\brief Invokes a BGx AT command without acquiring a lock, using previously set setOptions() values.
 */
void atcmd_invokeReuseLock(const char *cmdTemplate, ...)
{
    ASSERT(((atcmd_t*)g_lqLTEM.atcmd)->isOpenLocked, srcfile_ltemc_atcmd_c);            // function assumes re-use of existing lock

    ATCMD_reset(false);                                                         // clear out properties WITHOUT lock release
    ((atcmd_t*)g_lqLTEM.atcmd)->autoLock = atcmd__setLockModeManual;

    char *cmdStr = ((atcmd_t*)g_lqLTEM.atcmd)->cmdStr;
    va_list ap;

    va_start(ap, cmdTemplate);
    vsnprintf(cmdStr, sizeof(((atcmd_t*)g_lqLTEM.atcmd)->cmdStr), cmdTemplate, ap);

    ((atcmd_t*)g_lqLTEM.atcmd)->invokedAt = pMillis();
    IOP_sendTx(((atcmd_t*)g_lqLTEM.atcmd)->cmdStr, strlen(((atcmd_t*)g_lqLTEM.atcmd)->cmdStr), false);
    IOP_sendTx("\r", 1, true);
}


/**
 *	\brief Closes (completes) a BGx AT command structure and frees action resource (release action lock).
 */
void atcmd_close()
{
    ((atcmd_t*)g_lqLTEM.atcmd)->isOpenLocked = false;
    ((atcmd_t*)g_lqLTEM.atcmd)->execDuration = pMillis() - ((atcmd_t*)g_lqLTEM.atcmd)->invokedAt;
}


/**
 *	\brief Performs blind send data transfer to device.
 */
void atcmd_sendCmdData(const char *data, uint16_t dataSz, const char* eotPhrase)
{
    if (((atcmd_t*)g_lqLTEM.atcmd)->invokedAt == 0)
        ((atcmd_t*)g_lqLTEM.atcmd)->invokedAt = pMillis();

    IOP_sendTx(data, dataSz, true);

    if (strlen(eotPhrase) > 0 )
        IOP_sendTx(eotPhrase, strlen(eotPhrase), true);
}


/**
 *	\brief Checks recv buffer for command response and sets atcmd structure data with result.
 */
resultCode_t atcmd_getResult()
{
    atcmd_t *atcmdPtr = (atcmd_t*)g_lqLTEM.atcmd;
    iop_t *iopPtr = (iop_t*)g_lqLTEM.iop;

    cmdParseRslt_t parseRslt = cmdParseRslt_pending;
    atcmdPtr->resultCode = 0;
    
    char *endptr = NULL;
    if (*(iopPtr->rxCBuffer->tail) != '\0')                             // if cmd buffer not empty, test for command complete with registered parser
    {
        atcmdPtr->response = iopPtr->rxCBuffer->tail;                   // tracked in ATCMD struct for convenience
        parseRslt = (*atcmdPtr->responseParserFunc)(&g_lqLTEM);           // PARSE response
        PRINTF(dbgColor__gray, "prsr=%d \r", parseRslt);
    }

    if (parseRslt & cmdParseRslt_error)                                 // check error bit
    {
        if (parseRslt & cmdParseRslt_moduleError)                       // BGx ERROR or CME/CMS
            atcmdPtr->resultCode = resultCode__methodNotAllowed;

        else if (parseRslt & cmdParseRslt_countShort)                   // did not find expected tokens
            atcmdPtr->resultCode = resultCode__notFound;

        else
            atcmdPtr->resultCode = resultCode__internalError;           // covering the unknown

        atcmdPtr->isOpenLocked = false;                                 // close action to release action lock on any error
        atcmdPtr->execDuration = pMillis() - atcmdPtr->invokedAt;
    }

    if (parseRslt == cmdParseRslt_pending)                              // still pending, check for timeout error
    {
        if (pElapsed(atcmdPtr->invokedAt, atcmdPtr->timeoutMS))
        {
            atcmdPtr->resultCode = resultCode__timeout;
            atcmdPtr->isOpenLocked = false;                                     // close action to release action lock
            atcmdPtr->execDuration = pMillis() - atcmdPtr->invokedAt;

            if (!ltem_readDeviceState())                                        // if action timed-out, verify not a device wide failure
                ltem_notifyApp(appEvent_fault_hardLogic, "LTEm Not On");            // BGx status pin low
            else if (!SC16IS7xx_chkCommReady())
                ltem_notifyApp(appEvent_fault_softLogic, "LTEm SPI");               // UART bridge SPI not initialized correctly, IRQ not enabled

            return resultCode__timeout;
        }
        return resultCode__unknown;
    }

    if (parseRslt & cmdParseRslt_success)                                       // success bit: parser completed with success (may have excessRecv warning)
    {
        if (atcmdPtr->autoLock)
            atcmdPtr->isOpenLocked = false;
        atcmdPtr->execDuration = pMillis() - atcmdPtr->invokedAt;
        atcmdPtr->resultCode = resultCode__success;
    }

    if (parseRslt & cmdParseRslt_excessRecv)
    {
        IOP_rxParseForUrcEvents();                                              // got more data than AT-Cmd reponse, need to parseImmediate() for URCs
    }
    return atcmdPtr->resultCode;
}


/**
 *	\brief Waits for atcmd result, periodically checking recv buffer for valid response until timeout.
 */
resultCode_t atcmd_awaitResult()
{
    cmdParseRslt_t respRslt;                                    // resultCode_t result;

    while (1)
    {
        respRslt = atcmd_getResult();

        if (respRslt)
            break;;
        if (g_lqLTEM.cancellationRequest)                         // test for cancellation (RTOS or IRQ)
        {
            ((atcmd_t*)g_lqLTEM.atcmd)->resultCode = resultCode__cancelled;
            break;
        }
        pYield();                                               // give back control momentarily before next loop pass
    }
    return ((atcmd_t*)g_lqLTEM.atcmd)->resultCode;
}

/**
 *	\brief Returns the atCmd result code, 0xFFFF or cmdParseRslt_pending if command is pending completion
 */
resultCode_t atcmd_getLastResult()
{
    return ((atcmd_t*)g_lqLTEM.atcmd)->resultCode;
}


/**
 *	\brief Returns the atCmd result code, 0xFFFF or cmdParseRslt_pending if command is pending completion
 */
int32_t atcmd_getLastValue()
{
    return ((atcmd_t*)g_lqLTEM.atcmd)->retValue;
}


/**
 *	\brief Returns the string captured from the last command response; between preamble and finale (excludes both)
 */
char* atcmd_getLastResponse()
{
    ASSERT(((atcmd_t*)g_lqLTEM.atcmd)->response != NULL, srcfile_ltemc_atcmd_c);
    return ((atcmd_t*)g_lqLTEM.atcmd)->response;
}


/**
 *	\brief Returns the atCmd last execution duration in milliseconds
 */
uint32_t atcmd_getLastDuration()
{
    return ((atcmd_t*)g_lqLTEM.atcmd)->execDuration;
}


// /**
//  *	\brief Returns the atCmd response text if completed, will be empty C-string prior to completion
//  */
// char *ATCMD_getLastResponse()
// {
//     return ((atcmd_t*)g_lqLTEM.atcmd)->response;
// }


// /**
//  *	\brief Returns the atCmd response text beyond internal completion parser's progress
//  */
// char *ATCMD_getLastResponseTail()
// {
//     return ((atcmd_t*)g_lqLTEM.atcmd)->response + ((atcmd_t*)g_lqLTEM.atcmd)->parsedLen;
// }


/**
 *	\brief Sends ESC character to ensure BGx is not in text mode (">" prompt awaiting ^Z/ESC, MQTT publish etc.).
 */
void atcmd_exitTextMode()
{
    IOP_sendTx("\x1B", 1, true);            // send ESC - Ctrl-[
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



#pragma endregion


#pragma region LTEmC Internal Functions
/*-----------------------------------------------------------------------------------------------*/


/**
 *  \brief Awaits exclusive access to QBG module command interface.
*/
bool ATCMD_awaitLock(uint16_t timeoutMS)
{
    uint32_t waitStart = pMillis();
                                                                    // cannot set lock while... 
    // while (((atcmd_t*)g_lqLTEM.atcmd)->isOpenLocked ||                // - existing lock
    //        ((iop_t*)g_lqLTEM.iop)->rxStreamCtrl != NULL)              // - IOP is in data mode (handling a receive)
    // {
    //     pYield();                                                   // call back to platform yield() in case there is work there
    //     if (((iop_t*)g_lqLTEM.iop)->rxStreamCtrl != NULL)
    //         ltem_doWork();                                          // if data receive is blocking, give doWork an opportunity to resolve

    //     if (pMillis() - waitStart > timeoutMS)
    //         return false;                                           // timed out waiting for lock
    // }
    // ((atcmd_t*)g_lqLTEM.atcmd)->isOpenLocked = true;                  // acquired lock
    // return true;

    while (pMillis() - waitStart < timeoutMS)
    {                                                               // can set new lock if...
        if (!((atcmd_t*)g_lqLTEM.atcmd)->isOpenLocked &&              // !not existing lock
            ((iop_t*)g_lqLTEM.iop)->rxStreamCtrl == NULL)             // IOP is not in data mode (handling a receive)
        {
            ((atcmd_t*)g_lqLTEM.atcmd)->isOpenLocked = true;          // acquired new lock
            return true;
        }

        pYield();                                                   // call back to platform yield() in case there is work there that can be done
        if (((iop_t*)g_lqLTEM.iop)->rxStreamCtrl != NULL)
            ltem_doWork();                                          // if data receive is blocking, give doWork an opportunity to resolve
    }
    return false;                                                   // timed out waiting for lock
}


/**
 *	\brief Returns the current atCmd lock state
 */
bool ATCMD_isLockActive()
{
    return ((atcmd_t*)g_lqLTEM.atcmd)->isOpenLocked;
}


/**
 *	\brief Resets atCmd struct and optionally releases lock, a BGx AT command structure.
 */
void ATCMD_reset(bool releaseOpenLock)
{
    /* clearing req/resp buffers now for debug clarity, future likely just insert '\0' */

    atcmd_t* atcmdPtr = (atcmd_t*)g_lqLTEM.atcmd;

    // request side of action
    if (releaseOpenLock)
    {
        atcmdPtr->isOpenLocked = false;                         // reset current lock
    }

    memset(((atcmd_t*)g_lqLTEM.atcmd)->cmdStr, 0, bufferSz__cmdTx);
    // memset(((atcmd_t*)g_lqLTEM.atcmd)->response, 0, IOP__rxCoreBufferSize);
    atcmdPtr->resultCode = 0;
    atcmdPtr->invokedAt = 0;
    atcmdPtr->response = NULL;

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
 *	\brief Validate the response ends in a BGxx OK value.
 */
cmdParseRslt_t atcmd_okResponseParser(ltemDevice_t *modem)
{
    return atcmd__stdResponseParser("", false, ",", 0, 0, "\r\nOK\r\n", 0);
}


/**
 *	\brief Stardard atCmd response parser, flexible response pattern match and parse. 
 */
//cmdParseRslt_t atcmd__defaultResponseParser(const char *preamble, bool preambleReqd, const char *delimiters, uint8_t tokensReqd, uint8_t valueIndx, const char *finale)

cmdParseRslt_t atcmd__stdResponseParser(const char *preamble, bool preambleReqd, const char *delimiters, uint8_t tokensReqd, uint8_t valueIndx, const char *finale, uint16_t lengthReqd)
{
    atcmd_t *atcmdPtr = (atcmd_t *)g_lqLTEM.atcmd;
    cmdParseRslt_t parseRslt = cmdParseRslt_pending;

    ASSERT(!(preambleReqd && STREMPTY(preamble)), srcfile_ltemc_atcmd_c);                       // if preamble required, cannot be empty
    ASSERT(!((tokensReqd || valueIndx) && strlen(delimiters) == 0), srcfile_ltemc_atcmd_c);     // if tokens count or value return, need delimiter

    char *finaleAt = NULL;                                                              // check for existance of finale, NULL term array if found 
    uint8_t finaleLen = STREMPTY(finale) ? 0 : strlen(finale);
    uint8_t preambleLen = STREMPTY(preamble) ? 0 : strlen(preamble);
    bool lengthSatisfied = lengthReqd == 0 || (strlen(atcmdPtr->response) - preambleLen - finaleLen) >= lengthReqd;
    bool finaleSatisfied = STREMPTY(finale) || ((finaleAt = strstr(atcmdPtr->response, finale)) != NULL);

    if (!lengthSatisfied || !finaleSatisfied)                                           // keep searching, haven't found the "end" of expected response
        return cmdParseRslt_pending;

    /*  No longer pending, analyze the response for param settings */
    if (finaleAt)                                                                       // NULL term response at finale, removes it from the response returned
        finaleAt[0] = '\0';
    if (strlen(finaleAt + strlen(finale)) > 0)                                          // look for chars in buffer beyond finale (if there, likely +URC)
        parseRslt = cmdParseRslt_excessRecv;

    char *errorPrefixAt;                                                                // look for error reported in response
    if ( (errorPrefixAt = strstr(atcmdPtr->response, "ERROR")) != NULL)
    {
        if ((errorPrefixAt = strstr(atcmdPtr->response, "+CM")) != NULL)                // if CME or CMS error, capture error code returned
        {
            atcmdPtr->errorCode = strtol(errorPrefixAt + 12, NULL, 0);                  // "+CME ERROR: " and "+CMS ERROR: " are length = 12
        }
        return cmdParseRslt_error | cmdParseRslt_moduleError;
    }

    if (preambleLen)                                                                    // if preamble, jump past it and optionally test existance if req'd
    {
        char *preambleFoundAt = strstr(atcmdPtr->response, preamble);
        if (preambleFoundAt)
            atcmdPtr->response = preambleFoundAt + preambleLen;                         // remove preamble from retResponse

        if (preambleReqd && !preambleFoundAt)
            return cmdParseRslt_error | cmdParseRslt_preambleMissing;
    }

    /*  Parse content between preamble and finale for tokens (reqd cnt) and value extraction
     *  Only supporting 1 delimiter for now; support for optional delimiters in future won't require func signature change
     */
    char *delimAt;
    uint8_t tokenCnt = 0;
    uint32_t retValue = 0;

    if (tokensReqd || valueIndx)                                                        // count tokens to service value return or validate required token count
    {
        if (strlen(atcmdPtr->response))                                                 // at least one token is there
        {
            tokenCnt = 1;
            if (valueIndx == 1)
            {
                atcmdPtr->retValue = strtol(atcmdPtr->response, NULL, 0);
            }

            delimAt = atcmdPtr->response;
            do                                                                          // look for delimeters to get tokens
            {
                delimAt++;
                tokenCnt += (delimAt = strchr(delimAt, (int)delimiters[0])) != NULL;
                if (tokenCnt == valueIndx)
                {
                    atcmdPtr->retValue = strtol(delimAt + 1, NULL, 0);
                }
            } while (delimAt != NULL);
        }
        if (tokenCnt < tokensReqd || tokenCnt < valueIndx)
            parseRslt |= cmdParseRslt_error | cmdParseRslt_countShort;                  // add count short error
    }

    if (!(parseRslt & cmdParseRslt_error))                                          // check error bit
        parseRslt |= cmdParseRslt_success;                                          // no error, preserve possible warnings (excess recv, etc.)
    return parseRslt;                                                               // done
}


/**
 *	\brief Response parser looking for "ready-to-proceed" prompt in order to send to network
 *
 *  \param response [in] The character string received from BGx (so far, may not be complete).
 *  \param rdyPrompt [in] Prompt text to check for.
 *  \param endptr [out] Pointer to the char after match.
 * 
 *  \return Result code enum value (http status code)
 */
cmdParseRslt_t ATCMD_readyPromptParser(const char *response, const char *rdyPrompt)
{
    char *endptr = strstr(response, rdyPrompt);
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
    return cmdParseRslt_pending;
}


/**
 *	\brief [private] Transmit data ready to send "data prompt" parser.
 *
 *  \param response [in] Character data recv'd from BGx to parse for task complete
 *  \param endptr [out] Char pointer to the char following parsed text
 * 
 *  \return HTTP style result code, 0 = not complete
 */
cmdParseRslt_t ATCMD_txDataPromptParser(const char *response) 
{
    return ATCMD_readyPromptParser(response, "> ");
}


/**
 *	\brief "CONNECT" prompt parser.
 *
 *  \param response [in] Character data recv'd from BGx to parse for task complete
 *  \param endptr [out] Char pointer to the char following parsed text
 * 
 *  \return HTTP style result code, 0 = not complete
 */
cmdParseRslt_t ATCMD_connectPromptParser(const char *response)
{
    return ATCMD_readyPromptParser(response, "CONNECT\r\n");
}

#pragma endregion  // completionParsers


#pragma region Static Function Definitions
/*-----------------------------------------------------------------------------------------------*/

// /**
//  *	\brief Copies response\result information at action conclusion. Designed as a diagnostic aid for failed AT actions.
//  */
// static void S_copyToDiagnostics()
// {
//     memset(((atcmd_t*)g_lqLTEM.atcmd)->lastActionError->cmdStr, 0, atcmd__historyBufferSz);
//     strncpy(((atcmd_t*)g_lqLTEM.atcmd)->lastActionError->cmdStr, ((atcmd_t*)g_lqLTEM.atcmd)->cmdStr, atcmd__historyBufferSz-1);
//     memset(((atcmd_t*)g_lqLTEM.atcmd)->lastActionError->response, 0, atcmd__historyBufferSz);
//     strncpy(((atcmd_t*)g_lqLTEM.atcmd)->lastActionError->response, ((atcmd_t*)g_lqLTEM.atcmd)->response, atcmd__historyBufferSz-1);
//     ((atcmd_t*)g_lqLTEM.atcmd)->lastActionError->statusCode = ((atcmd_t*)g_lqLTEM.atcmd)->resultCode;
//     ((atcmd_t*)g_lqLTEM.atcmd)->lastActionError->duration = pMillis() - ((atcmd_t*)g_lqLTEM.atcmd)->invokedAt;
// }

#pragma endregion
