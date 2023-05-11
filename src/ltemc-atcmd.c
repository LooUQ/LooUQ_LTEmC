/** ****************************************************************************
  \file 
  @brief LTEmC INTERNAL BGx AT command processor 
  \author Greg Terrell, LooUQ Incorporated

  \loouq

--------------------------------------------------------------------------------

    This project is released under the GPL-3.0 License.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 
***************************************************************************** */


#define SRCFILE "ATC"                           // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT
#include <stdarg.h>
#include "ltemc-internal.h"

#define _DEBUG 0                                // set to non-zero value for PRINTF debugging output, 
// debugging output options                     // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if _DEBUG > 0
    asm(".global _printf_float");               // forces build to link in float support for printf
    #if _DEBUG == 1
    #define SERIAL_DBG 1                        // enable serial port output using devl host platform serial, 1=wait for port
    #elif _DEBUG >= 2
    #include <jlinkRtt.h>                       // output debug PRINTF macros to J-Link RTT channel
    #define PRINTF(c_,f_,__VA_ARGS__...) do { rtt_printf(c_, (f_), ## __VA_ARGS__); } while(0)
    #endif
#else
#define PRINTF(c_, f_, ...) 
#endif


#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#define CTRL(x) (#x[0]-'a'+1)

extern ltemDevice_t g_lqLTEM;



/* Static Function Declarations
------------------------------------------------------------------------------------------------- */
static resultCode_t S__readResult();
static void S__rxParseForUrc();


#pragma region Public Functions
/*-----------------------------------------------------------------------------------------------*/

/**
 *	@brief Resets atCmd struct and optionally releases lock, a BGx AT command structure.
 */
void atcmd_reset(bool releaseLock)
{
    /* clearing req/resp buffers now for debug clarity, future likely just insert '\0' */

   // dataMode is not reset/cleared here, static S__resetDataMode is invoked after result
 
    // request side of action
    if (releaseLock)
        g_lqLTEM.atcmd->isOpenLocked = false;                       // reset current lock

    memset(g_lqLTEM.atcmd->cmdStr, 0, atcmd__cmdBufferSz);
    memset(g_lqLTEM.atcmd->rawResponse, 0, atcmd__respBufferSz);
    memset(g_lqLTEM.atcmd->errorDetail, 0, ltem__errorDetailSz);
    g_lqLTEM.atcmd->resultCode = 0;
    g_lqLTEM.atcmd->invokedAt = 0;
    g_lqLTEM.atcmd->retValue = 0;
    g_lqLTEM.atcmd->execDuration = 0;

    // command side
    g_lqLTEM.iop->txBffr = &g_lqLTEM.atcmd->cmdStr;                 // IOP pointer to current "talker"
    g_lqLTEM.iop->txPending = 0;

    // response side
    g_lqLTEM.atcmd->response = g_lqLTEM.atcmd->rawResponse;         // reset data component of response to full-response

    // restore defaults
    g_lqLTEM.atcmd->timeout = atcmd__defaultTimeout;
    g_lqLTEM.atcmd->responseParserFunc = ATCMD_okResponseParser;
}


/**
 *	@brief Setup automatic data mode switch/servicing.
 */
void atcmd_configDataMode(uint16_t contextKey, const char* trigger, dataRxHndlr_func dataHndlr, char *dataLoc, uint16_t dataSz, appRcvProto_func applRecvDataCB, bool skipParser)
{
    ASSERT(strlen(trigger) > 0);                                        // verify 3rd party setup (stream)
    ASSERT(dataHndlr != NULL);                                          // 

    memset(&g_lqLTEM.atcmd->dataMode, 0, sizeof(dataMode_t));

    g_lqLTEM.atcmd->dataMode.contextKey = contextKey;
    memcpy(g_lqLTEM.atcmd->dataMode.trigger, trigger, strlen(trigger));
    g_lqLTEM.atcmd->dataMode.dataHndlr = dataHndlr;
    g_lqLTEM.atcmd->dataMode.dataLoc = dataLoc;
    g_lqLTEM.atcmd->dataMode.dataSz = dataSz;
    g_lqLTEM.atcmd->dataMode.applRecvDataCB = applRecvDataCB;
    g_lqLTEM.atcmd->dataMode.skipParser = skipParser;
}


/**
 *	@brief Invokes a BGx AT command using default option values (automatic locking).
 */
bool atcmd_tryInvoke(const char *cmdTemplate, ...)
{
    if (g_lqLTEM.atcmd->isOpenLocked)
        return false;

    atcmd_reset(true);                                                  // clear atCmd control
    g_lqLTEM.atcmd->autoLock = atcmd__setLockModeAuto;                  // set automatic lock control mode

    //char *cmdStr = g_lqLTEM.atcmd->cmdStr;
    va_list ap;

    va_start(ap, cmdTemplate);
    vsnprintf(g_lqLTEM.atcmd->cmdStr, sizeof(g_lqLTEM.atcmd->cmdStr), cmdTemplate, ap);
    strcat(g_lqLTEM.atcmd->cmdStr, "\r");

    if (!ATCMD_awaitLock(g_lqLTEM.atcmd->timeout))          // attempt to acquire new atCmd lock for this instance
        return false;

    g_lqLTEM.atcmd->invokedAt = pMillis();

    // TEMPORARY
    memcpy(g_lqLTEM.atcmd->CMDMIRROR, g_lqLTEM.atcmd->cmdStr, strlen(g_lqLTEM.atcmd->cmdStr));

    IOP_startTx(g_lqLTEM.atcmd->cmdStr, strlen(g_lqLTEM.atcmd->cmdStr));
    return true;
}


/**
 *	@brief Invokes a BGx AT command without acquiring a lock, using previously set setOptions() values.
 */
void atcmd_invokeReuseLock(const char *cmdTemplate, ...)
{
    ASSERT(g_lqLTEM.atcmd->isOpenLocked);    // function assumes re-use of existing lock

    atcmd_reset(false);                                                         // clear out properties WITHOUT lock release
    g_lqLTEM.atcmd->autoLock = atcmd__setLockModeManual;

    char *cmdStr = g_lqLTEM.atcmd->cmdStr;
    va_list ap;

    va_start(ap, cmdTemplate);
    vsnprintf(cmdStr, sizeof(g_lqLTEM.atcmd->cmdStr), cmdTemplate, ap);
    strcat(g_lqLTEM.atcmd->cmdStr, "\r");

    g_lqLTEM.atcmd->invokedAt = pMillis();

    // TEMPORARY
    memcpy(g_lqLTEM.atcmd->CMDMIRROR, g_lqLTEM.atcmd->cmdStr, atcmd__cmdBufferSz);

    IOP_startTx(g_lqLTEM.atcmd->cmdStr, strlen(g_lqLTEM.atcmd->cmdStr));
}


/**
 *	@brief Closes (completes) a BGx AT command structure and frees action resource (release action lock).
 */
void atcmd_close()
{
    g_lqLTEM.atcmd->isOpenLocked = false;
    g_lqLTEM.atcmd->execDuration = pMillis() - g_lqLTEM.atcmd->invokedAt;
}


// /**
//  *	@brief Performs blind send data transfer to device.
//  */
// void atcmd_sendCmdData(const char *data, uint16_t dataSz)
// {
//     ASSERT(g_lqLTEM.atcmd->isOpenLocked);                       // verify inside command sequence

//     if (g_lqLTEM.atcmd->invokedAt == 0)
//         g_lqLTEM.atcmd->invokedAt = pMillis();

//     IOP_startTx(data, dataSz);

//     while (g_lqLTEM.iop->txPending > 0)
//     {
//         pDelay(1);
//         ASSERT(pMillis() - g_lqLTEM.atcmd->invokedAt < PERIOD_FROM_SECONDS(120));
//     }
//     atcmd_reset(false);                                         // restore atcmd as TX buffer source
// }


/**
 *	@brief Waits for atcmd result, periodically checking recv buffer for valid response until timeout.
 */
resultCode_t atcmd_awaitResult()
{
    resultCode_t rslt = resultCode__unknown;                                        // resultCode_t result;
    do
    {
        rslt = S__readResult();
        if (g_lqLTEM.cancellationRequest)                                           // test for cancellation (RTOS or IRQ)
        {
            g_lqLTEM.atcmd->resultCode = resultCode__cancelled;
            break;
        }
        pYield();                                                                   // give back control momentarily before next loop pass
    } while (rslt == resultCode__unknown);

    #if _DEBUG == 0                                                                 // debug for debris in rxBffr
    ASSERT_W(cbffr_getOccupied(g_lqLTEM.iop->rxBffr) == 0, "RxBffr Dirty");
    #else
    if (cbffr_getOccupied(g_lqLTEM.iop->rxBffr) > 0)
    {
        char dbg[81] = {0};
        cbffr_pop(g_lqLTEM.iop->rxBffr, dbg, 80);
        PRINTF(dbgColor__yellow, "*!* %s", dbg);
    }
    #endif

    g_lqLTEM.atcmd->timeout = atcmd__defaultTimeout;
    g_lqLTEM.atcmd->responseParserFunc = ATCMD_okResponseParser;

    return g_lqLTEM.atcmd->resultCode;
}


/**
 *	@brief Waits for atcmd result, periodically checking recv buffer for valid response until timeout.
 */
resultCode_t atcmd_awaitResultWithOptions(uint32_t timeoutMS, cmdResponseParser_func cmdResponseParser)
{
    // set options
    if (timeoutMS != atcmd__noTimeoutChange)
    {
        g_lqLTEM.atcmd->timeout = timeoutMS;
    }
    if (cmdResponseParser)                                          // caller can use atcmd__useDefaultOKCompletionParser
        g_lqLTEM.atcmd->responseParserFunc = cmdResponseParser;
    else
        g_lqLTEM.atcmd->responseParserFunc = ATCMD_okResponseParser;

    return atcmd_awaitResult();
}


/**
 *	@brief Returns the atCmd result code, 0xFFFF or cmdParseRslt_pending if command is pending completion
 */
resultCode_t atcmd_getResult()
{
    return g_lqLTEM.atcmd->resultCode;
}


/**
 *	@brief Returns the atCmd value response
 */
bool atcmd_getPreambleFound()
{
    return g_lqLTEM.atcmd->preambleFound;
}


/**
 *	@brief Returns the string captured from the last command response; between pPreamble and pFinale (excludes both)
 */
char* atcmd_getRawResponse()
{
    ASSERT(g_lqLTEM.atcmd->rawResponse != NULL);
    return g_lqLTEM.atcmd->rawResponse;
}


/**
 *	@brief Returns the string captured from the last command response with prefixing white-space and any preamble removed.
 *  @return Char pointer to the command response (note: this is stripped of preamble and finale strings)
 */
char *atcmd_getResponse()
{
    ASSERT(g_lqLTEM.atcmd->response != NULL);
    return g_lqLTEM.atcmd->response;
}


/**
 *	@brief Returns the atCmd value response
 */
int32_t atcmd_getValue()
{
    return g_lqLTEM.atcmd->retValue;
}


/**
 *	@brief Returns the atCmd last execution duration in milliseconds
 */
uint32_t atcmd_getDuration()
{
    return g_lqLTEM.atcmd->execDuration;
}


/**
 *	@brief Returns the atCmd parser result code, 0xFFFF or cmdParseRslt_pending if command is pending completion
 */
cmdParseRslt_t atcmd_getParserResult()
{
    return g_lqLTEM.atcmd->parserResult;
}


/**
 *	@brief Returns the BGx module error code or 0 if no error. Use this function to get details on a resultCode_t = 500
 */
char *atcmd_getErrorDetail()
{
    return &g_lqLTEM.atcmd->errorDetail;
}


/**
 *	@brief Returns the BGx reported CME/CMS error code as a numeric value.
 *  @return Numeric CM* error code, 999 otherwise.
 */
uint16_t atcmd_getErrorDetailCode()
{
    if (g_lqLTEM.atcmd->errorDetail[1] == 'C' && g_lqLTEM.atcmd->errorDetail[2] == 'M')
    {
        return strtol(g_lqLTEM.atcmd->errorDetail + 12, NULL, 10);
    }
    else
        return 999;
}


/**
 *	@brief Sends ESC character to ensure BGx is not in text mode (">" prompt awaiting ^Z/ESC, MQTT publish etc.).
 */
void atcmd_exitTextMode()
{
    IOP_startTx(CTRL(z), 1);
}


/**
 *	@brief Sends +++ sequence to transition BGx out of data mode to command mode.
 */
void atcmd_exitDataMode()
{
    lDelay(1000);
    IOP_startTx("+++", 3);         // send +++, gaurded by 1 second of quiet
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
                                                                    
    while (pMillis() - waitStart < timeoutMS)           // cannot set lock while... 
    {                                                       // can set new lock if...
        if (!g_lqLTEM.atcmd->isOpenLocked)                  // !not existing lock
        {
            g_lqLTEM.atcmd->isOpenLocked = true;            // acquired new lock
            return true;
        }
        pYield();                                           // call back to platform yield() in case there is work there that can be done
        ltem_eventMgr();                                    // process any new receives prior to starting cmd invoke
    }
    return false;                                           // timed out waiting for lock
}


/**
 *	@brief Returns the current atCmd lock state
 */
bool ATCMD_isLockActive()
{
    return g_lqLTEM.atcmd->isOpenLocked;
}


/**
 *	@brief Checks receive buffer for command response and sets atcmd structure data with result.
 */
static resultCode_t S__readResult()
{
    g_lqLTEM.atcmd->parserResult = cmdParseRslt_pending;
    g_lqLTEM.atcmd->resultCode = 0;
    uint16_t peekedLen;
    
    ltem_eventMgr();                                                                        // check for URC events preceeding cmd response

    if (cbffr_getOccupied(g_lqLTEM.iop->rxBffr) > 0)
    {
        // chk for current command services a stream and there is a recv handler registered
        if (g_lqLTEM.atcmd->dataMode.dataHndlr != NULL)
        {
            // looking for streamPrefix phrase 
            if (cbffr_find(g_lqLTEM.iop->rxBffr, g_lqLTEM.atcmd->dataMode.trigger, 0, 0, true) != CBFFR_NOFIND)
            {
                PRINTF(dbgColor__white, "%s:dataMode>\r", g_lqLTEM.atcmd->streamPrefix);                // entered stream data mode
                resultCode_t dataRslt = (*g_lqLTEM.atcmd->dataMode.dataHndlr)();
                if (dataRslt == resultCode__success)
                {
                    if (dataRslt != resultCode__success)
                    {
                        g_lqLTEM.atcmd->parserResult = cmdParseRslt_error;
                        g_lqLTEM.atcmd->resultCode = dataRslt;
                    }
                    else if (g_lqLTEM.atcmd->dataMode.skipParser)
                    {
                        g_lqLTEM.atcmd->parserResult = cmdParseRslt_success;
                        g_lqLTEM.atcmd->resultCode = dataRslt;
                    }
                }
                PRINTF(dbgColor__white, "Exit dataMode rslt=%d\r", dataRslt);
                memset(&g_lqLTEM.atcmd->dataMode, 0, sizeof(dataMode_t));                               // done with dataMode settings
            }
        }

        uint8_t respLen = strlen(g_lqLTEM.atcmd->rawResponse);                                          // response so far
        uint8_t popSz = MIN(atcmd__respBufferSz - respLen, cbffr_getOccupied(g_lqLTEM.iop->rxBffr));    
        ASSERT((respLen + popSz) < atcmd__respBufferSz);                                                // ensure don't overflow 

        if (g_lqLTEM.atcmd->parserResult == cmdParseRslt_pending)
        {
            cbffr_pop(g_lqLTEM.iop->rxBffr, g_lqLTEM.atcmd->rawResponse + respLen,  popSz);             // pop new into response buffer for parsing
            /* - */
            g_lqLTEM.atcmd->parserResult = (*g_lqLTEM.atcmd->responseParserFunc)();                     /* *** parse for command response *** */
            /* - */
            PRINTF(dbgColor__gray, "prsr=%d \r", g_lqLTEM.atcmd->parserResult);
        }
    }

    if (g_lqLTEM.atcmd->parserResult & cmdParseRslt_error)                                      // check error bit
    {
        if (g_lqLTEM.atcmd->parserResult & cmdParseRslt_moduleError)                            // BGx ERROR or CME/CMS
            g_lqLTEM.atcmd->resultCode = resultCode__cmError;

        else if (g_lqLTEM.atcmd->parserResult & cmdParseRslt_countShort)                        // did not find expected tokens
            g_lqLTEM.atcmd->resultCode = resultCode__notFound;

        else
            g_lqLTEM.atcmd->resultCode = resultCode__internalError;                             // covering the unknown

        atcmd_close();                                                                          // close action to release action lock on any error
    }

    if (g_lqLTEM.atcmd->parserResult == cmdParseRslt_pending)                                   // still pending, check for timeout error
    {
        if (pElapsed(g_lqLTEM.atcmd->invokedAt, g_lqLTEM.atcmd->timeout))
        {
            g_lqLTEM.atcmd->resultCode = resultCode__timeout;
            g_lqLTEM.atcmd->isOpenLocked = false;                                               // close action to release action lock
            g_lqLTEM.atcmd->execDuration = pMillis() - g_lqLTEM.atcmd->invokedAt;

            if (ltem_getDeviceState() != deviceState_appReady)                                  // if action timed-out, verify not a device wide failure
                ltem_notifyApp(appEvent_fault_hardLogic, "LTEm Not AppReady");
            else if (!SC16IS7xx_isAvailable())
                ltem_notifyApp(appEvent_fault_softLogic, "LTEm SPI Fault");                     // UART bridge SPI not initialized correctly, IRQ not enabled

            return resultCode__timeout;
        }
        return resultCode__unknown;
    }

    if (g_lqLTEM.atcmd->parserResult & cmdParseRslt_success)                                // success bit: parser completed with success (may have excessRecv warning)
    {
        if (g_lqLTEM.atcmd->autoLock)                                                       // if the individual cmd is controlling lock state
            g_lqLTEM.atcmd->isOpenLocked = false;                                           // equivalent to atcmd_close()
        g_lqLTEM.atcmd->execDuration = pMillis() - g_lqLTEM.atcmd->invokedAt;
        g_lqLTEM.atcmd->resultCode = resultCode__success;
        g_lqLTEM.metrics.cmdInvokes++;
    }
    return g_lqLTEM.atcmd->resultCode;
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
 *	@brief Validate the response ends in a BGxx OK value.
 */
cmdParseRslt_t ATCMD_okResponseParser()
{
    return atcmd_stdResponseParser("", false, "", 0, 0, "OK\r\n", 0);
}

// /**
//  *	@brief LTEmC internal testing parser to capture incoming response until timeout.
//  */
// cmdParseRslt_t ATCMD_testResponseTrace()
// {
//     return cmdParseRslt_pending;
// }


// /**
//  *	@brief [private] Transmit data ready to send "data prompt" parser.
//  *
//  *  @param response [in] Character data recv'd from BGx to parse for task complete
//  *  @param endptr [out] Char pointer to the char following parsed text
//  * 
//  *  @return HTTP style result code, 0 = not complete
//  */
// cmdParseRslt_t ATCMD_txDataPromptParser() 
// {
//     return ATCMD_readyPromptParser("> ");
// }


// /**
//  *	@brief "CONNECT" prompt parser.
//  *
//  *  @param response [in] Character data recv'd from BGx to parse for task complete
//  *  @param endptr [out] Char pointer to the char following parsed text
//  * 
//  *  @return HTTP style result code, 0 = not complete
//  */
// cmdParseRslt_t ATCMD_connectPromptParser()
// {
//     return ATCMD_readyPromptParser("CONNECT\r\n");
// }


// /**
//  *	@brief Response parser looking for "ready-to-proceed" prompt in order to send to network
//  *
//  *  @param response [in] The character string received from BGx (so far, may not be complete).
//  *  @param rdyPrompt [in] Prompt text to check for.
//  *  @param endptr [out] Pointer to the char after match.
//  * 
//  *  @return Result code enum value (http status code)
//  */
// cmdParseRslt_t ATCMD_readyPromptParser(const char *rdyPrompt)
// {
//     char*endptr;

//     endptr = strstr(g_lqLTEM.atcmd->rawResponse, rdyPrompt);
//     if (endptr != NULL)
//     {
//         g_lqLTEM.atcmd->response = endptr + strlen(rdyPrompt);                   // point past data prompt
//         return cmdParseRslt_success;
//     }
//     endptr = strstr(g_lqLTEM.atcmd->rawResponse, "ERROR");
//     if (endptr != NULL)
//     {
//         return cmdParseRslt_error;
//     }
//     return cmdParseRslt_pending;
// }

/**
 *	@brief Stardard TX (out) data handler used by dataMode.
 */
resultCode_t atcmd_stdTxDataHndlr()
{
    IOP_startTx(g_lqLTEM.atcmd->dataMode.dataLoc, g_lqLTEM.atcmd->dataMode.dataSz);

    uint32_t startTime = pMillis();

    while (pMillis() - startTime < g_lqLTEM.atcmd->timeout)
    {
        uint16_t trlrIndx = cbffr_find(g_lqLTEM.iop->rxBffr, "OK", 0, 0, true);
        if(CBFFR_FOUND(trlrIndx))
        {
            cbffr_skipTail(g_lqLTEM.iop->rxBffr, OK_COMPLETED_LENGTH);                  // OK + line-end
            return resultCode__success;
        }
        pDelay(1);
    }
    return resultCode__timeout;
}


/**
 *	@brief Stardard RX (in) data handler used by dataMode.
 */
resultCode_t atcmd_stdRxDataHndlr()
{
}

/**
 *	@brief Stardard atCmd response parser, flexible response pattern match and parse. 
 */
cmdParseRslt_t atcmd_stdResponseParser(const char *pPreamble, bool preambleReqd, const char *pDelimeters, uint8_t tokensReqd, uint8_t valueIndx, const char *pFinale, uint16_t lengthReqd)
{
    cmdParseRslt_t parseRslt = cmdParseRslt_pending;

    ASSERT(!(preambleReqd && STREMPTY(pPreamble)));                                     // if preamble required, cannot be empty
    ASSERT(pPreamble != NULL && pDelimeters != NULL && pFinale != NULL);                // char params are not NULL, must be valid empty char arrays
    ASSERT((!(tokensReqd || valueIndx) || strlen(pDelimeters) > 0));                    // if tokens count or value return, need delimiter

    uint8_t preambleLen = strlen(pPreamble);
    uint8_t reqdPreambleLen = preambleReqd ? preambleLen : 0;
    uint8_t finaleLen = strlen(pFinale);
    uint16_t responseLen = strlen(g_lqLTEM.atcmd->rawResponse);

    // always look for error, short-circuit result if CME/CMS
    char *pErrorLoctn;
    if ((pErrorLoctn = strstr(g_lqLTEM.atcmd->rawResponse, "+CM")) || (pErrorLoctn = strstr(g_lqLTEM.atcmd->rawResponse, "ERROR")))
    {
        for (size_t i = 0; i < ltem__errorDetailSz; i++)                                // copy raw chars: unknown incoming format, stop at line end
        {
            if ((pErrorLoctn[i] == '\r' || pErrorLoctn[i] == '\n'))
                break;;
            g_lqLTEM.atcmd->errorDetail[i] = pErrorLoctn[i];
        }
        return cmdParseRslt_error | cmdParseRslt_moduleError;
    }

    bool lengthSatisfied = responseLen >= lengthReqd && 
                           responseLen >= (reqdPreambleLen + finaleLen);
    if (!lengthSatisfied)                                                               // still pending, haven't recv'd required cnt of chars
        return cmdParseRslt_pending;

    /* Response length has been satisfied (and no error detected).
     * Search response for preamble, finale, token count (tokensReqd/valueIndx) 
    */
    
    while (g_lqLTEM.atcmd->response[0] == '\r' || g_lqLTEM.atcmd->response[0] == '\n')
    {
        g_lqLTEM.atcmd->response++;                                                     // skip past prefixing line terminators
    }
    
    bool preambleSatisfied = false;
    if (preambleLen)                                                                    // if pPreamble provided
    {
        char *pPreambleLoctn = strstr(g_lqLTEM.atcmd->rawResponse, pPreamble);             // find it in response
        if (pPreambleLoctn)
        {
            preambleSatisfied = true;
            g_lqLTEM.atcmd->preambleFound = true;
            g_lqLTEM.atcmd->response = pPreambleLoctn + preambleLen;                // remove pPreamble from retResponse
        }
        else if (preambleReqd)
        {
            if (responseLen >= preambleLen)
                return cmdParseRslt_error | cmdParseRslt_preambleMissing;               // require preamble missing, response is sufficient for match

            return cmdParseRslt_pending;                                                // keep waiting on response
        }
    }
    else
    {
        preambleSatisfied = true;
        g_lqLTEM.atcmd->preambleFound = false;
    }

    /*  Parse for finale string in response
     *  Start search after preamble or the start of response (if preamble satisfied without finding a preamble)
     */
    uint8_t tokenCnt = 0;
    uint32_t retValue = 0;

    char *pFinaleLoctn = NULL;
    bool finaleSatisfied = false;
    if (preambleSatisfied)
    {
        if (STREMPTY(pFinale))
            finaleSatisfied = true;
        else
        {
            pFinaleLoctn = strstr(g_lqLTEM.atcmd->response, pFinale);
            if (pFinaleLoctn)
            {
                finaleSatisfied = true;
                //pFinaleLoctn[0] = '\0';
            }
        }
    }

    /*  Parse content between pPreamble/response start and pFinale for tokens (reqd cnt) and value extraction
     *  Only supporting one delimiter for now; support for optional delimeter list in future won't require API change
     */
    bool tokenCntSatified = !(tokensReqd || valueIndx);
    if (finaleSatisfied && !tokenCntSatified)                                           // count tokens to service value return or validate required token count
    {
        tokenCnt = 1;
        char *pDelimeterAt;
        char *pTokenAt = g_lqLTEM.atcmd->response;
        do
        {
            if (tokenCnt == valueIndx)                                                  // grab value, this is what is requested
                g_lqLTEM.atcmd->retValue = strtol(pTokenAt, NULL, 0);

            pDelimeterAt = strpbrk(pTokenAt, pDelimeters);                              // look for delimeter/next token
            if (tokenCnt >= tokensReqd && tokenCnt >= valueIndx)                        // at/past required token = done
            {
                tokenCntSatified = true;
                break;
            }
            if (pDelimeterAt)
            {
                tokenCnt++;
                pTokenAt = pDelimeterAt + 1;
            }
        } while (pDelimeterAt);

        if (!tokenCntSatified)
            parseRslt |= cmdParseRslt_error | cmdParseRslt_countShort;                  // set error and count short bits
    }

    if (!(parseRslt & cmdParseRslt_error) &&                                            // check error bit
        preambleSatisfied &&
        finaleSatisfied &&
        tokenCntSatified)                                                               // and all component test conditions
    {
        parseRslt |= cmdParseRslt_success;                                              // no error, preserve possible warnings (excess recv, etc.)
    }
    return parseRslt;                                                                   // done
}


#pragma endregion  // completionParsers


#pragma region Static Function Definitions
/*-----------------------------------------------------------------------------------------------*/


// /**
//  *	@brief register a stream peer with IOP to control communications. Typically performed by protocol open.
//  */
// void ATCMD_registerStream(uint8_t streamIndx, iopStreamCtrl_t *streamCtrl)
// {
//     g_lqLTEM.atcmd->streamPeers[streamIndx] = streamCtrl;
// }


// /**
//  *  @brief Parse recv'd data (in command RX buffer) for async event preambles that need to be handled immediately.
//  *  @details AT cmd uses this to examine new buffer contents for +URC events that may arrive in command response
//  *           Declared in ltemc-internal.h
//  */
// void S__rxParseForUrc(const char* urcTarget)
// {
//     char *foundAt;
//     // /* SSL/TLS data received
//     // */
//     // if (g_lqLTEM.iop->scktMap > 0 && (foundAt = strstr(urcBffr, "+QSSLURC: \"recv\",")))         // shortcircuit if no sockets
//     // {
//     //     PRINTF(dbgColor__cyan, "-p=sslURC");
//     //     uint8_t urcLen = strlen("+QSSLURC: \"recv\",");
//     //     char *cntxtIdPtr = urcBffr + urcLen;
//     //     char *endPtr = NULL;
//     //     uint8_t cntxtId = (uint8_t)strtol(cntxtIdPtr, &endPtr, 10);
//     //     // action
//     //     ((scktCtrl_t *)g_lqLTEM.iop->streamPeers[cntxtId])->dataPending = true;
//     //     // clean up
//     //     g_lqLTEM.iop->rxCBuffer->head - (endPtr - urcBffr);                                     // remove URC from rxBuffer
//     //     memset(g_lqLTEM.iop->urcDetectBuffer, 0, IOP__urcDetectBufferSz);
//     // }

//     // // preserve temporarily Nov29-2022
//     // // else if (g_lqLTEM.iop->scktMap && memcmp("+QIURC: \"recv", urcStartPtr, strlen("+QIURC: \"recv")) == 0)         // shortcircuit if no sockets
//     // // {
//     // //     PRINTF(dbgColor__cyan, "-p=ipURC");
//     // //     char *cntxIdPtr = g_lqLTEM.iop->rxCBuffer->prevHead + strlen("+QIURC: \"recv");
//     // //     char *endPtr = NULL;
//     // //     uint8_t cntxId = (uint8_t)strtol(cntxIdPtr, &endPtr, 10);
//     // //     ((scktCtrl_t *)g_lqLTEM.iop->streamPeers[cntxId])->dataPending = true;
//     // //     // discard this chunk, processed here
//     // //     g_lqLTEM.iop->rxCBuffer->head = g_lqLTEM.iop->rxCBuffer->prevHead;
//     // // }

//     // /* TCP/UDP data received
//     // */
//     // else if (g_lqLTEM.iop->scktMap && (foundAt = strstr(urcBffr, "+QIURC: \"recv\",")))         // shortcircuit if no sockets
//     // {
//     //     PRINTF(dbgColor__cyan, "-p=ipURC");
//     //     uint8_t urcLen = strlen("+QIURC: \"recv\",");
//     //     char *cntxtIdPtr = urcBffr + urcLen;
//     //     char *endPtr = NULL;
//     //     uint8_t cntxtId = (uint8_t)strtol(cntxtIdPtr, &endPtr, 10);
//     //     // action
//     //     ((scktCtrl_t *)g_lqLTEM.iop->streamPeers[cntxtId])->dataPending = true;
//     //     // clean up
//     //     g_lqLTEM.iop->rxCBuffer->head - (endPtr - urcBffr);                                     // remove URC from rxBuffer
//     //     memset(g_lqLTEM.iop->urcDetectBuffer, 0, IOP__urcDetectBufferSz);
//     // }

//     // /* MQTT message receive
//     // */
//     // else if (g_lqLTEM.iop->mqttMap && (foundAt = strstr(urcBffr, "+QMTRECV: ")))
//     // {
//     //     PRINTF(dbgColor__cyan, "-p=mqttR");
//     //     uint8_t urcLen = strlen("+QMTRECV: ");
//     //     char *cntxtIdPtr = urcBffr + urcLen;
//     //     char *endPtr = NULL;
//     //     uint8_t cntxtId = (uint8_t)strtol(cntxtIdPtr, &endPtr, 10);

//     //     // action
//     //     ASSERT(g_lqLTEM.iop->rxStreamCtrl == NULL);                                // ASSERT: not inside another stream recv

//     //     /* this chunk, contains both meta data for receive followed by actual data, need to copy the data chunk to start of rxDataBuffer for this context */

//     //     g_lqLTEM.iop->rxStreamCtrl = g_lqLTEM.iop->streamPeers[cntxtId];                                // put IOP in datamode for context 
//     //     rxDataBufferCtrl_t *dBufPtr = &g_lqLTEM.iop->rxStreamCtrl->recvBufCtrl;                         // get reference to context specific data RX buffer
        
//     //     /* need to fixup core/cmd and data buffers for mixed content in receive
//     //      * moving post prefix received from core/cmd buffer to context data buffer
//     //      * preserving prefix text for overflow detection (prefix & trailer text must be in same buffer)
//     //      */
//     //     char *urcStartPtr = memchr(g_lqLTEM.iop->rxCBuffer->prevHead, '+', g_lqLTEM.iop->rxCBuffer->head - g_lqLTEM.iop->rxCBuffer->prevHead);  
//     //     memcpy(dBufPtr->pages[dBufPtr->iopPg]._buffer, urcStartPtr, g_lqLTEM.iop->rxCBuffer->head - urcStartPtr);
//     //     dBufPtr->pages[dBufPtr->iopPg].head += g_lqLTEM.iop->rxCBuffer->head - urcStartPtr;

//     //     // clean-up
//     //     g_lqLTEM.iop->rxCBuffer->head = urcStartPtr;                                                    // drop recv'd from cmd\core buffer, processed here
//     //     memset(g_lqLTEM.iop->urcDetectBuffer, 0, IOP__urcDetectBufferSz);
//     // }

//     // /* MQTT connection reset by server
//     // */
//     // else if (g_lqLTEM.iop->mqttMap &&
//     //          (foundAt = MAX(strstr(urcBffr, "+QMTSTAT: "), strstr(urcBffr, "+QMTDISC: "))))
//     // {
//     //     PRINTF(dbgColor__cyan, "-p=mqttS");
//     //     uint8_t urcLen = MAX(strlen("+QMTSTAT: "), strlen("+QMTDISC: "));
//     //     char *cntxtIdPtr = urcBffr + urcLen;
//     //     char *endPtr = NULL;
//     //     uint8_t cntxId = (uint8_t)strtol(cntxtIdPtr, &endPtr, 10);
//     //     // action
//     //     g_lqLTEM.iop->mqttMap &= ~(0x01 << cntxId);
//     //     ((mqttCtrl_t *)g_lqLTEM.iop->streamPeers[cntxId])->state = mqttState_closed;
//     //     // clean up
//     //     g_lqLTEM.iop->rxCBuffer->head - (endPtr - urcBffr);                                     // remove URC from rxBuffer
//     //     memset(g_lqLTEM.iop->urcDetectBuffer, 0, IOP__urcDetectBufferSz);
//     // }

//     // /* PDP context closed by network
//     // */
//     // else if ((foundAt = strstr(urcBffr, "+QIURC: \"pdpdeact\",")))
//     // {
//     //     PRINTF(dbgColor__cyan, "-p=pdpD");
//     //     uint8_t urcLen = strlen("+QIURC: \"pdpdeact\",");
//     //     char *pdpCntxtIdPtr = urcBffr + urcLen;
//     //     char *endPtr = NULL;
//     //     uint8_t contextId = (uint8_t)strtol(pdpCntxtIdPtr, &endPtr, 10);
//     //     // action
//     //     for (size_t i = 0; i <  sizeof(g_lqLTEM.providerInfo->networks) / sizeof(providerInfo_t); i++)
//     //     {
//     //         if (g_lqLTEM.providerInfo->networks[i].pdpContextId == contextId)
//     //         {
//     //             g_lqLTEM.providerInfo->networks[i].pdpContextId = 0;
//     //             g_lqLTEM.providerInfo->networks[i].ipAddress[0] = 0;
//     //             break;
//     //         }
//     //     }
//     //     // clean-up
//     //     g_lqLTEM.iop->rxCBuffer->head - (endPtr - urcBffr);                                     // remove URC from rxBuffer
//     // }
// }



#pragma endregion

