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

#include <stdarg.h>
//#include "ltemc-atcmd.h"
#include "ltemc-internal.h"


#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

extern ltemDevice_t g_lqLTEM;


/* Static Function Declarations
------------------------------------------------------------------------------------------------- */


#pragma region Public Functions
/*-----------------------------------------------------------------------------------------------*/

/**
 *	\brief Reset AT command options to defaults.
 */
inline void atcmd_restoreOptionDefaults()
{
    g_lqLTEM.atcmd->timeoutMS = atcmd__defaultTimeoutMS;
    g_lqLTEM.atcmd->responseParserFunc = ATCMD_okResponseParser;
}


/**
 *	\brief Invokes a BGx AT command using default option values (automatic locking).
 */
bool atcmd_tryInvoke(const char *cmdTemplate, ...)
{
    if (g_lqLTEM.atcmd->isOpenLocked || g_lqLTEM.iop->rxStreamCtrl != NULL)
        return false;

    atcmd_reset(true);                                                  // clear atCmd control
    g_lqLTEM.atcmd->autoLock = atcmd__setLockModeAuto;                  // set automatic lock control mode
    atcmd_restoreOptionDefaults();                                      // standard behavior reset default option values

    char *cmdStr = g_lqLTEM.atcmd->cmdStr;
    va_list ap;

    va_start(ap, cmdTemplate);
    vsnprintf(cmdStr, sizeof(g_lqLTEM.atcmd->cmdStr), cmdTemplate, ap);

    if (!ATCMD_awaitLock(g_lqLTEM.atcmd->timeoutMS))          // attempt to acquire new atCmd lock for this instance
        return false;

    g_lqLTEM.atcmd->invokedAt = pMillis();
    IOP_sendTx(g_lqLTEM.atcmd->cmdStr, strlen(g_lqLTEM.atcmd->cmdStr), false);
    IOP_sendTx("\r", 1, true);
    return true;
}


/**
 *	\brief Invokes a BGx AT command without acquiring a lock, using previously set setOptions() values.
 */
void atcmd_invokeReuseLock(const char *cmdTemplate, ...)
{
    ASSERT(g_lqLTEM.atcmd->isOpenLocked, srcfile_ltemc_atcmd_c);    // function assumes re-use of existing lock

    atcmd_reset(false);                                                         // clear out properties WITHOUT lock release
    g_lqLTEM.atcmd->autoLock = atcmd__setLockModeManual;

    char *cmdStr = g_lqLTEM.atcmd->cmdStr;
    va_list ap;

    va_start(ap, cmdTemplate);
    vsnprintf(cmdStr, sizeof(g_lqLTEM.atcmd->cmdStr), cmdTemplate, ap);

    g_lqLTEM.atcmd->invokedAt = pMillis();
    IOP_sendTx(g_lqLTEM.atcmd->cmdStr, strlen(g_lqLTEM.atcmd->cmdStr), false);
    IOP_sendTx("\r", 1, true);
}


/**
 *	\brief Closes (completes) a BGx AT command structure and frees action resource (release action lock).
 */
void atcmd_close()
{
    g_lqLTEM.atcmd->isOpenLocked = false;
    g_lqLTEM.atcmd->execDuration = pMillis() - g_lqLTEM.atcmd->invokedAt;
}


/**
 *	\brief Performs blind send data transfer to device.
 */
void atcmd_sendCmdData(const char *data, uint16_t dataSz, const char* eotPhrase)
{
    if (g_lqLTEM.atcmd->invokedAt == 0)
        g_lqLTEM.atcmd->invokedAt = pMillis();

    IOP_sendTx(data, dataSz, true);

    if (strlen(eotPhrase) > 0 )
        IOP_sendTx(eotPhrase, strlen(eotPhrase), true);
}


/**
 *	\brief Checks receive buffer for command response and sets atcmd structure data with result.
 */
resultCode_t ATCMD_readResult()
{
    g_lqLTEM.atcmd->parserResult = cmdParseRslt_pending;
    g_lqLTEM.atcmd->resultCode = 0;
    
    char *endptr = NULL;
    if (*(g_lqLTEM.iop->rxCBuffer->tail) != '\0')                                   // if cmd buffer not empty, test for command complete with registered parser
    {
        g_lqLTEM.atcmd->response = g_lqLTEM.iop->rxCBuffer->tail;                   // tracked in ATCMD struct for convenience
        g_lqLTEM.atcmd->parserResult = (*g_lqLTEM.atcmd->responseParserFunc)();     // PARSE response
        PRINTF(dbgColor__gray, "prsr=%d \r", g_lqLTEM.atcmd->parserResult);
    }

    if (g_lqLTEM.atcmd->parserResult & cmdParseRslt_error)                          // check error bit
    {
        if (g_lqLTEM.atcmd->parserResult & cmdParseRslt_moduleError)                // BGx ERROR or CME/CMS
            g_lqLTEM.atcmd->resultCode = resultCode__methodNotAllowed;

        else if (g_lqLTEM.atcmd->parserResult & cmdParseRslt_countShort)            // did not find expected tokens
            g_lqLTEM.atcmd->resultCode = resultCode__notFound;

        else
            g_lqLTEM.atcmd->resultCode = resultCode__internalError;                 // covering the unknown

        g_lqLTEM.atcmd->isOpenLocked = false;                                       // close action to release action lock on any error
        g_lqLTEM.atcmd->execDuration = pMillis() - g_lqLTEM.atcmd->invokedAt;
    }

    if (g_lqLTEM.atcmd->parserResult == cmdParseRslt_pending)                       // still pending, check for timeout error
    {
        if (pElapsed(g_lqLTEM.atcmd->invokedAt, g_lqLTEM.atcmd->timeoutMS))
        {
            g_lqLTEM.atcmd->resultCode = resultCode__timeout;
            g_lqLTEM.atcmd->isOpenLocked = false;                                   // close action to release action lock
            g_lqLTEM.atcmd->execDuration = pMillis() - g_lqLTEM.atcmd->invokedAt;

            if (ltem_getDeviceState() != deviceState_appReady)                      // if action timed-out, verify not a device wide failure
                ltem_notifyApp(appEvent_fault_hardLogic, "LTEm Not AppReady");
            else if (!SC16IS7xx_isCommReady())
                ltem_notifyApp(appEvent_fault_softLogic, "LTEm SPI Fault");         // UART bridge SPI not initialized correctly, IRQ not enabled

            return resultCode__timeout;
        }
        return resultCode__unknown;
    }

    if (g_lqLTEM.atcmd->parserResult & cmdParseRslt_success)                        // success bit: parser completed with success (may have excessRecv warning)
    {
        if (g_lqLTEM.atcmd->autoLock)
            g_lqLTEM.atcmd->isOpenLocked = false;
        g_lqLTEM.atcmd->execDuration = pMillis() - g_lqLTEM.atcmd->invokedAt;
        g_lqLTEM.atcmd->resultCode = resultCode__success;
        g_lqLTEM.metrics.cmdInvokes++;
    }

    if (g_lqLTEM.atcmd->parserResult & cmdParseRslt_excessRecv)
    {
        IOP_rxParseForUrcEvents();                                                  // got more data than AT-Cmd reponse, need to parseImmediate() for URCs
    }
    return g_lqLTEM.atcmd->resultCode;
}


/**
 *	\brief Waits for atcmd result, periodically checking recv buffer for valid response until timeout.
 */
resultCode_t atcmd_awaitResult()
{
    cmdParseRslt_t respRslt;                                        // resultCode_t result;
    while (1)
    {
        respRslt = ATCMD_readResult();

        if (respRslt)
            break;;
        if (g_lqLTEM.cancellationRequest)                           // test for cancellation (RTOS or IRQ)
        {
            g_lqLTEM.atcmd->resultCode = resultCode__cancelled;
            break;
        }
        pYield();                                                   // give back control momentarily before next loop pass
    }
    return g_lqLTEM.atcmd->resultCode;
}


/**
 *	\brief Waits for atcmd result, periodically checking recv buffer for valid response until timeout.
 */
resultCode_t atcmd_awaitResultWithOptions(uint32_t timeoutMS, cmdResponseParser_func cmdResponseParser)
{
    // set options
    if (timeoutMS != atcmd__noTimeoutChange)
    {
        g_lqLTEM.atcmd->timeoutMS = (timeoutMS == atcmd__useDefaultTimeout) ? atcmd__defaultTimeoutMS : timeoutMS;
    }
    if (cmdResponseParser)                                          // caller can use atcmd__useDefaultOKCompletionParser
        g_lqLTEM.atcmd->responseParserFunc = cmdResponseParser;
    else
        g_lqLTEM.atcmd->responseParserFunc = ATCMD_okResponseParser;

    return atcmd_awaitResult();
}


/**
 *	\brief Returns the atCmd result code, 0xFFFF or cmdParseRslt_pending if command is pending completion
 */
resultCode_t atcmd_getResult()
{
    return g_lqLTEM.atcmd->resultCode;
}


/**
 *	\brief Returns the string captured from the last command response; between preamble and finale (excludes both)
 */
char* atcmd_getLastResponse()
{
    ASSERT(g_lqLTEM.atcmd->response != NULL, srcfile_ltemc_atcmd_c);
    return g_lqLTEM.atcmd->response;
}


/**
 *	\brief Returns the atCmd value response
 */
int32_t atcmd_getValue()
{
    return g_lqLTEM.atcmd->retValue;
}


/**
 *	\brief Returns the atCmd last execution duration in milliseconds
 */
uint32_t atcmd_getLastDuration()
{
    return g_lqLTEM.atcmd->execDuration;
}


/**
 *	\brief Returns the atCmd parser result code, 0xFFFF or cmdParseRslt_pending if command is pending completion
 */
cmdParseRslt_t atcmd_getParserResult()
{
    return g_lqLTEM.atcmd->parserResult;
}


/**
 *	\brief Returns the BGx module error code or 0 if no error. Use this function to get details on a resultCode_t = 500
 */
char *atcmd_getLastError()
{
    return &g_lqLTEM.atcmd->errorDetail;
}


/**
 *	\brief Resets atCmd struct and optionally releases lock, a BGx AT command structure.
 */
void atcmd_reset(bool releaseOpenLock)
{
    /* clearing req/resp buffers now for debug clarity, future likely just insert '\0' */
    // request side of action
    if (releaseOpenLock)
    {
        g_lqLTEM.atcmd->isOpenLocked = false;                         // reset current lock
    }
    memset(g_lqLTEM.atcmd->cmdStr, 0, bufferSz__cmdTx);
    g_lqLTEM.atcmd->resultCode = 0;
    g_lqLTEM.atcmd->invokedAt = 0;
    g_lqLTEM.atcmd->response = NULL;
    memset(g_lqLTEM.atcmd->errorDetail, 0, ltem__errorDetailSz);

    // response side
    IOP_resetCoreRxBuffer();

    // restore response defaults
    g_lqLTEM.atcmd->timeoutMS = atcmd__defaultTimeoutMS;
    g_lqLTEM.atcmd->responseParserFunc = ATCMD_okResponseParser;
}




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
                                                                    
    while (pMillis() - waitStart < timeoutMS)           // cannot set lock while... 
    {                                                       // can set new lock if...
        if (!g_lqLTEM.atcmd->isOpenLocked &&                // !not existing lock
            g_lqLTEM.iop->rxStreamCtrl == NULL)             // IOP is not in data mode (handling a receive)
        {
            g_lqLTEM.atcmd->isOpenLocked = true;            // acquired new lock
            return true;
        }
        pYield();                                           // call back to platform yield() in case there is work there that can be done
        if (g_lqLTEM.iop->rxStreamCtrl != NULL)
            ltem_doWork();                                  // if data receive is blocking, give doWork an opportunity to resolve
    }
    return false;                                           // timed out waiting for lock
}


/**
 *	\brief Returns the current atCmd lock state
 */
bool ATCMD_isLockActive()
{
    return g_lqLTEM.atcmd->isOpenLocked;
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
cmdParseRslt_t ATCMD_okResponseParser()
{
    return atcmd_stdResponseParser("", false, ",", 0, 0, "\r\nOK\r\n", 0);
}


/**
 *	\brief Stardard atCmd response parser, flexible response pattern match and parse. 
 */
cmdParseRslt_t atcmd_stdResponseParser(const char *preamble, bool preambleReqd, const char *delimiters, uint8_t tokensReqd, uint8_t valueIndx, const char *finale, uint16_t lengthReqd)
{
    cmdParseRslt_t parseRslt = cmdParseRslt_pending;

    ASSERT(!(preambleReqd && STREMPTY(preamble)), srcfile_ltemc_atcmd_c);                       // if preamble required, cannot be empty
    ASSERT(!((tokensReqd || valueIndx) && strlen(delimiters) == 0), srcfile_ltemc_atcmd_c);     // if tokens count or value return, need delimiter

    char *finaleAt = NULL;                                                                      // check for existance of finale, NULL term array if found 
    uint8_t finaleLen = STREMPTY(finale) ? 0 : strlen(finale);
    uint8_t preambleLen = STREMPTY(preamble) ? 0 : strlen(preamble);
    // bool lengthSatisfied = lengthReqd == 0 || (strlen(g_lqLTEM.atcmd->response) - preambleLen - finaleLen) >= lengthReqd;
    lengthReqd = MAX(preambleLen - finaleLen, lengthReqd);
    bool lengthSatisfied = strlen(g_lqLTEM.atcmd->response) >= lengthReqd;
    bool finaleSatisfied = STREMPTY(finale) || ((finaleAt = strstr(g_lqLTEM.atcmd->response, finale)) != NULL);

    // always look for error short-circuit if CME/CMS
    char *errorPrefixAt;                                                     
    if ( (errorPrefixAt = strstr(g_lqLTEM.atcmd->response, "ERROR")) != NULL)                   // look for any error reported in response
    {
        if ((errorPrefixAt = strstr(g_lqLTEM.atcmd->response, "+CM")) != NULL)                  // if it is a CME/CMS error: capture error code returned
        {
            strncpy(g_lqLTEM.atcmd->errorDetail, errorPrefixAt + 12, ltem__errorDetailSz - 1);  // point past prefix and grab likely error details
        }
        return cmdParseRslt_error | cmdParseRslt_moduleError;
    }

    if (!lengthSatisfied || !finaleSatisfied)                                                   // keep searching, haven't found the "end" of expected response
        return cmdParseRslt_pending;

    /*  No longer pending, analyze the response for param settings
     --------------------------------------------------------------------------*/
    if (finaleAt)                                                                               // NULL term response at finale, removes it from the response returned
        finaleAt[0] = '\0';
    if (strlen(finaleAt + strlen(finale)) > 0)                                                  // look for chars in buffer beyond finale (if there, likely +URC)
        parseRslt = cmdParseRslt_excessRecv;

    if (preambleLen)                                                                            // if preamble, jump past it and test existance if req'd
    {
        char *preambleFoundAt = strstr(g_lqLTEM.atcmd->response, preamble);
        if (preambleFoundAt)
            g_lqLTEM.atcmd->response = preambleFoundAt + preambleLen;                           // remove preamble from retResponse

        if (preambleReqd && !preambleFoundAt)
            return cmdParseRslt_error | cmdParseRslt_preambleMissing;
    }

    /*  Parse content between preamble and finale for tokens (reqd cnt) and value extraction
     *  Only supporting 1 delimiter for now; support for optional delimiters in future won't require func signature change
     */
    char *delimAt;
    uint8_t tokenCnt = 0;
    uint32_t retValue = 0;

    if (tokensReqd || valueIndx)                                                                // count tokens to service value return or validate required token count
    {
        if (strlen(g_lqLTEM.atcmd->response))                                                   // at least one token is there
        {
            tokenCnt = 1;
            if (valueIndx == 1)
            {
                g_lqLTEM.atcmd->retValue = strtol(g_lqLTEM.atcmd->response, NULL, 0);
            }

            delimAt = g_lqLTEM.atcmd->response;
            do                                                                                  // look for delimeters to get tokens
            {
                delimAt++;
                tokenCnt += (delimAt = strchr(delimAt, (int)delimiters[0])) != NULL;
                if (tokenCnt == valueIndx)
                {
                    g_lqLTEM.atcmd->retValue = strtol(delimAt + 1, NULL, 0);
                }
            } while (delimAt != NULL);
        }
        if (tokenCnt < tokensReqd || tokenCnt < valueIndx)
            parseRslt |= cmdParseRslt_error | cmdParseRslt_countShort;                          // add count short error
    }

    if (!(parseRslt & cmdParseRslt_error))                                                      // check error bit
        parseRslt |= cmdParseRslt_success;                                                      // no error, preserve possible warnings (excess recv, etc.)
    return parseRslt;                                                                           // done
}


/**
 *	\brief [private] Transmit data ready to send "data prompt" parser.
 *
 *  \param response [in] Character data recv'd from BGx to parse for task complete
 *  \param endptr [out] Char pointer to the char following parsed text
 * 
 *  \return HTTP style result code, 0 = not complete
 */
cmdParseRslt_t ATCMD_txDataPromptParser() 
{
    return ATCMD_readyPromptParser("> ");
}


/**
 *	\brief "CONNECT" prompt parser.
 *
 *  \param response [in] Character data recv'd from BGx to parse for task complete
 *  \param endptr [out] Char pointer to the char following parsed text
 * 
 *  \return HTTP style result code, 0 = not complete
 */
cmdParseRslt_t ATCMD_connectPromptParser()
{
    return ATCMD_readyPromptParser("CONNECT\r\n");
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
cmdParseRslt_t ATCMD_readyPromptParser(const char *rdyPrompt)
{
    char*endptr;

    endptr = strstr(g_lqLTEM.atcmd->response, rdyPrompt);
    if (endptr != NULL)
    {
        endptr += strlen(rdyPrompt);                   // point past data prompt
        return cmdParseRslt_success;
    }
    endptr = strstr(g_lqLTEM.atcmd->response, "ERROR");
    if (endptr != NULL)
    {
        return cmdParseRslt_error;
    }
    return cmdParseRslt_pending;
}

#pragma endregion  // completionParsers


#pragma region Static Function Definitions
/*-----------------------------------------------------------------------------------------------*/

// /**
//  *	\brief Copies response\result information at action conclusion. Designed as a diagnostic aid for failed AT actions.
//  */
// static void S__copyToDiagnostics()
// {
//     memset(g_lqLTEM.atcmd->lastActionError->cmdStr, 0, atcmd__historyBufferSz);
//     strncpy(g_lqLTEM.atcmd->lastActionError->cmdStr, g_lqLTEM.atcmd->cmdStr, atcmd__historyBufferSz-1);
//     memset(g_lqLTEM.atcmd->lastActionError->response, 0, atcmd__historyBufferSz);
//     strncpy(g_lqLTEM.atcmd->lastActionError->response, g_lqLTEM.atcmd->response, atcmd__historyBufferSz-1);
//     g_lqLTEM.atcmd->lastActionError->statusCode = g_lqLTEM.atcmd->resultCode;
//     g_lqLTEM.atcmd->lastActionError->duration = pMillis() - g_lqLTEM.atcmd->invokedAt;
// }

#pragma endregion
