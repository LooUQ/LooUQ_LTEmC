/** ***************************************************************************
  @file 
  @brief Modem command/response and data transfer functions.

  @author Greg Terrell, LooUQ Incorporated

  \loouq
-------------------------------------------------------------------------------

LooUQ-LTEmC // Software driver for the LooUQ LTEm series cellular modems.
Copyright (C) 2017-2023 LooUQ Incorporated

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
Also add information on how to contact you by electronic and paper mail.

**************************************************************************** */

#include <lq-embed.h>
#define LOG_LEVEL LOGLEVEL_DBG
//#define DISABLE_ASSERTS                                   // ASSERT/ASSERT_W enabled by default, can be disabled 
#define LQ_SRCFILE "ATC"                       // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT

#define ENABLE_DIAGPRINT                    // expand DPRINT into debug output
// #define ENABLE_DIAGPRINT_VERBOSE            // expand DPRINT and DPRINT_V into debug output
#define ENABLE_ASSERT
// #include <lqdiag.h>

#include <stdarg.h>
#include "ltemc-internal.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
extern ltemDevice_t g_lqLTEM;

/* Static Function Declarations
------------------------------------------------------------------------------------------------- */
static void lqStr_getToken(const char * source, char delimiter, uint8_t tokenIndx, char * destBffr, uint8_t destBffrSz);
static resultCode_t S__readResult();
static bool S__cleanRxBffr();
//static void S__rxParseForUrc();

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
        g_lqLTEM.atcmd->isOpenLocked = false; // reset current lock

    memset(g_lqLTEM.atcmd->cmdStr, 0, atcmd__cmdBufferSz);
    memset(g_lqLTEM.atcmd->rawResponse, 0, atcmd__respBufferSz);
    memset(g_lqLTEM.atcmd->errorDetail, 0, ltem__errorDetailSz);
    g_lqLTEM.atcmd->resultCode = 0;
    g_lqLTEM.atcmd->resultValue = 0;
    g_lqLTEM.atcmd->invokedAt = 0;
    g_lqLTEM.atcmd->execDuration = 0;
    g_lqLTEM.atcmd->parserConfig.configSet = false;


    // command side
    g_lqLTEM.iop->txSrc = &g_lqLTEM.atcmd->cmdStr;                          // IOP pointer to current "talker"
    g_lqLTEM.iop->txPending = 0;

    // response side
    g_lqLTEM.atcmd->response = g_lqLTEM.atcmd->rawResponse;                 // reset data component of response to full-response

    // restore defaults
    g_lqLTEM.atcmd->timeout = atcmd__defaultTimeout;
    g_lqLTEM.atcmd->responseParserFunc = atcmd_defaultResponseParser;
}


/**
 *	@brief Resets AT-CMD next execution invoke properties.
 */
void atcmd_resetInvoke()
{
    g_lqLTEM.iop->txSrc = &g_lqLTEM.atcmd->cmdStr;                          // IOP pointer to current "talker"
    g_lqLTEM.iop->txPending = 0;

    g_lqLTEM.atcmd->timeout = atcmd__defaultTimeout;
    g_lqLTEM.atcmd->responseParserFunc = atcmd_defaultResponseParser;
}


/**
 *	@brief Resets AT-CMD last execution result properties.
 */
void atcmd_resetResults()
{
    memset(g_lqLTEM.atcmd->cmdStr, 0, atcmd__cmdBufferSz);
    memset(g_lqLTEM.atcmd->rawResponse, 0, atcmd__respBufferSz);
    memset(g_lqLTEM.atcmd->errorDetail, 0, ltem__errorDetailSz);
    g_lqLTEM.atcmd->resultCode = 0;
    g_lqLTEM.atcmd->resultValue = 0;
    g_lqLTEM.atcmd->invokedAt = 0;
    g_lqLTEM.atcmd->execDuration = 0;
    g_lqLTEM.atcmd->response = g_lqLTEM.atcmd->rawResponse;                 // reset data component of response to full-response
}



/**
 * @brief Sets command timeout for next invocation of a BGx AT command. 
 */
uint16_t atcmd_ovrrdTimeout(uint16_t newTimeout)
{
    uint16_t oldTimeout = g_lqLTEM.atcmd->timeout;
    if (newTimeout > 0)
        g_lqLTEM.atcmd->timeout = newTimeout;
    else
        g_lqLTEM.atcmd->timeout = atcmd__defaultTimeout;
    return oldTimeout;
}


/**
 * @brief Sets response parser for next invocation of a BGx AT command. 
 */
cmdResponseParser_func atcmd_ovrrdParser(cmdResponseParser_func newParser)
{
    cmdResponseParser_func oldParser = g_lqLTEM.atcmd->responseParserFunc;
    g_lqLTEM.atcmd->responseParserFunc = newParser;
    return oldParser;
}


/**
 * @brief Configure default ATCMD response parser for a specific command response.
 */
void atcmd_configParser(const char *preamble, bool preambleReqd, const char *delimiters, uint8_t tokensReqd, const char *finale, uint16_t lengthReqd)
{
    // parserConfig (parserConfig_t) struct holds settings

    g_lqLTEM.atcmd->parserConfig.configSet = true;

    strncpy(g_lqLTEM.atcmd->parserConfig.preamble, preamble, atcmd__parserConfigPreambleSz);
    g_lqLTEM.atcmd->parserConfig.preambleReqd = preambleReqd;
    strncpy(g_lqLTEM.atcmd->parserConfig.delimiters, delimiters, atcmd__parserConfigDelimetersSz);
    g_lqLTEM.atcmd->parserConfig.tokensReqd = tokensReqd;
    strncpy(g_lqLTEM.atcmd->parserConfig.finale, finale, atcmd__parserConfigFinaleSz);
    g_lqLTEM.atcmd->parserConfig.lengthReqd = lengthReqd;
}


/**
 *	@brief Setup automatic data mode switch/servicing.
 */
void atcmd_configDataMode(streamCtrl_t * streamCtrl, const char * trigger, dataHndlr_func dataHndlr, const char * dataPtr, uint16_t dataSz, appRcvr_func applRcvrCB, bool runParser)
{
    ASSERT(strlen(trigger) > 0); // verify 3rd party setup (stream)
    ASSERT(dataHndlr != NULL); //

    memset(&g_lqLTEM.atcmd->dataMode, 0, sizeof(dataMode_t));

    g_lqLTEM.atcmd->dataMode.dmState = dmState_enabled;
    g_lqLTEM.atcmd->dataMode.streamCtrl = streamCtrl;
    memcpy(g_lqLTEM.atcmd->dataMode.trigger, trigger, strlen(trigger));
    g_lqLTEM.atcmd->dataMode.dataHndlr = dataHndlr;
    g_lqLTEM.atcmd->dataMode.txDataLoc = dataPtr;
    g_lqLTEM.atcmd->dataMode.txDataSz = dataSz;
    // g_lqLTEM.atcmd->dataMode.rxDataSz = 0;                               // automatic from memset
    g_lqLTEM.atcmd->dataMode.applRcvrCB = applRcvrCB;
    g_lqLTEM.atcmd->dataMode.runParserAfterDataMode = runParser;
}


void atcmd_setDataModeEot(uint8_t eotChar)
{
    g_lqLTEM.iop->txEot = (char)eotChar;
}


/**
 *	@brief Invokes a BGx AT command using default option values (automatic locking).
 */
resultCode_t atcmd_dispatch(const char *cmdTemplate, ...)
{
    /* invoke phase
     ----------------------------------------- */
    if (g_lqLTEM.atcmd->isOpenLocked)
        return resultCode__locked;

    atcmd_resetResults();                                                           // clear results props from ATCMD control structure
    g_lqLTEM.atcmd->autoLock = atcmd__setLockModeAuto;                              // set automatic lock control mode

    // char *cmdStr = g_lqLTEM.atcmd->cmdStr;
    va_list ap;

    va_start(ap, cmdTemplate);
    vsnprintf(g_lqLTEM.atcmd->cmdStr, sizeof(g_lqLTEM.atcmd->cmdStr), cmdTemplate, ap);
    strcat(g_lqLTEM.atcmd->cmdStr, "\r");

    if (!atcmd_awaitLock(g_lqLTEM.atcmd->timeout)) // attempt to acquire new atCmd lock for this instance
        return false;

    // TEMPORARY SPI alters send buffer duplicates 
    //memcpy(g_lqLTEM.atcmd->CMDMIRROR, g_lqLTEM.atcmd->cmdStr, strlen(g_lqLTEM.atcmd->cmdStr));

    DPRINT_V(0, "(atcmd_tryInvoke) cmd=%s\r\n", g_lqLTEM.atcmd->cmdStr);
    IOP_startTx(g_lqLTEM.atcmd->cmdStr, strlen(g_lqLTEM.atcmd->cmdStr));

    /* await result phase
     ----------------------------------------- */
    resultCode_t rslt = resultCode__unknown; // resultCode_t result;
    g_lqLTEM.atcmd->invokedAt = lqMillis();
    do
    {
        rslt = S__readResult();
        if (g_lqLTEM.cancellationRequest) // test for cancellation (RTOS or IRQ)
        {
            g_lqLTEM.atcmd->resultCode = resultCode__cancelled;
            break;
        }
        pYield(); // give back control momentarily before next loop pass
    } while (rslt == resultCode__unknown);

    #if _DEBUG == 0 // debug for debris in rxBffr
    ASSERT_W(bbffr_getOccupied(g_lqLTEM.iop->rxBffr) == 0, "RxBffr Dirty");
    #else
    if (bbffr_getOccupied(g_lqLTEM.iop->rxBffr) > 0)
    {
        char dbg[81] = {0};
        bbffr_pop(g_lqLTEM.iop->rxBffr, dbg, 80);
        DPRINT(PRNT_YELLOW, "*!* %s", dbg);
    }
    #endif

    // reset options back to default values
    atcmd_resetInvoke();

    return g_lqLTEM.atcmd->resultCode;
}


/**
 *	@brief Invokes a BGx AT command using default option values (automatic locking).
 */
bool atcmd_tryInvoke(const char *cmdTemplate, ...)
{
    // if (g_lqLTEM.atcmd->isOpenLocked)
    //     return false;

    atcmd_reset(true);                                 // clear atCmd control
    g_lqLTEM.atcmd->autoLock = atcmd__setLockModeAuto; // set automatic lock control mode

    // char *cmdStr = g_lqLTEM.atcmd->cmdStr;
    va_list ap;

    va_start(ap, cmdTemplate);
    vsnprintf(g_lqLTEM.atcmd->cmdStr, sizeof(g_lqLTEM.atcmd->cmdStr), cmdTemplate, ap);
    strcat(g_lqLTEM.atcmd->cmdStr, "\r");

    if (!atcmd_awaitLock(g_lqLTEM.atcmd->timeout)) // attempt to acquire new atCmd lock for this instance
        return false;


    // TEMPORARY
    // memcpy(g_lqLTEM.atcmd->CMDMIRROR, g_lqLTEM.atcmd->cmdStr, strlen(g_lqLTEM.atcmd->cmdStr));
    DPRINT_V(0, "<atcmd_tryInvoke> cmd=%s\r\n", g_lqLTEM.atcmd->cmdStr);

    if (S__cleanRxBffr())
        DPRINT_V(PRNT_WARN, "Debris cleaned from RX buffer\r\n");
    IOP_startTx(g_lqLTEM.atcmd->cmdStr, strlen(g_lqLTEM.atcmd->cmdStr));
    return true;
}


/**
 *	@brief Invokes a BGx AT command without acquiring a lock, using previously set setOptions() values.
 */
void atcmd_invokeReuseLock(const char *cmdTemplate, ...)
{
    // ASSERT(g_lqLTEM.atcmd->isOpenLocked); // function assumes re-use of existing lock

    atcmd_reset(false); // clear out properties WITHOUT lock release
    g_lqLTEM.atcmd->autoLock = atcmd__setLockModeManual;

    char *cmdStr = g_lqLTEM.atcmd->cmdStr;
    va_list ap;

    va_start(ap, cmdTemplate);
    vsnprintf(cmdStr, sizeof(g_lqLTEM.atcmd->cmdStr), cmdTemplate, ap);
    strcat(g_lqLTEM.atcmd->cmdStr, "\r");

    // TEMPORARY
    // memcpy(g_lqLTEM.atcmd->CMDMIRROR, g_lqLTEM.atcmd->cmdStr, atcmd__cmdBufferSz);

    if (S__cleanRxBffr())
        DPRINT_V(PRNT_WARN, "Debris cleaned from RX buffer\r\n");
   IOP_startTx(g_lqLTEM.atcmd->cmdStr, strlen(g_lqLTEM.atcmd->cmdStr));
}


/**
 *	@brief Closes (completes) a BGx AT command structure and frees action resource (release action lock).
 */
void atcmd_close()
{
    g_lqLTEM.atcmd->isOpenLocked = false;
    g_lqLTEM.atcmd->execDuration = lqMillis() - g_lqLTEM.atcmd->invokedAt;
}


/**
 *	@brief Waits for atcmd result, periodically checking recv buffer for valid response until timeout.
 */
resultCode_t atcmd_awaitResult()
{
    resultCode_t rslt = resultCode__unknown;                                    // resultCode_t result;
    g_lqLTEM.atcmd->invokedAt = lqMillis();
    do
    {
        rslt = S__readResult();
        //DPRINT_V(0, "(atcmd_awaitResult) readResult=%d\r\n", rslt);

        if (g_lqLTEM.cancellationRequest)                                       // test for cancellation (RTOS or IRQ)
        {
            g_lqLTEM.atcmd->resultCode = resultCode__cancelled;
            break;
        }
        // pYield(); // give back control momentarily before next loop pass
        lqDelay(50);
    } while (rslt == resultCode__unknown);

    // reset options back to default values
    g_lqLTEM.atcmd->timeout = atcmd__defaultTimeout;
    g_lqLTEM.atcmd->responseParserFunc = atcmd_defaultResponseParser;
    return g_lqLTEM.atcmd->resultCode;
}


// /**
//  *	@brief Waits for atcmd result, periodically checking recv buffer for valid response until timeout.
//  */
// resultCode_t atcmd_awaitResultWithOptions(uint32_t timeoutMS, cmdResponseParser_func cmdResponseParser)
// {
//     DPRINT_V(0, "(atcmd_awaitResultWithOptions) entered\r\n");

//     // set options
//     if (timeoutMS != atcmd__noTimeoutChange)
//     {
//         g_lqLTEM.atcmd->timeout = timeoutMS;
//     }
//     if (cmdResponseParser) // caller can use atcmd__useDefaultOKCompletionParser
//         g_lqLTEM.atcmd->responseParserFunc = cmdResponseParser;
//     else
//         g_lqLTEM.atcmd->responseParserFunc = atcmd_defaultResponseParser();

//     return atcmd_awaitResult();
// }


/**
 *	@brief Returns the last ATCMD dispatched.
 */
const char* atcmd_getCommand()
{
    return g_lqLTEM.atcmd->cmdStr;
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
char* atcmd_getResponse()
{
    ASSERT(g_lqLTEM.atcmd->response != NULL);
    return g_lqLTEM.atcmd->response;
}


// /**
//  *	@brief Returns the atCmd value response
//  */
// int32_t atcmd_getValue()
// {
//     return g_lqLTEM.atcmd->retValue;
// }


/**
 *	@brief Returns the last dataMode RX read size.
 */
uint16_t atcmd_getRxLength()
{
    return g_lqLTEM.atcmd->dataMode.rxDataSz;
}


/**
 *	@brief Returns a token from the result of the last module command
 */
char* atcmd_getToken(uint8_t tokenIndx)
{
    char * startPtr = memchr(g_lqLTEM.atcmd->response, ':', atcmd__respBufferSz);   // list of response values are prefixed by ": "
    if (startPtr == NULL)
        startPtr = g_lqLTEM.atcmd->response;                                        // assume the string to tokenize is clean
    else
        startPtr += 2;

    lqStr_getToken(startPtr, ',', tokenIndx, g_lqLTEM.atcmd->respToken, atcmd__respTokenSz);
    return g_lqLTEM.atcmd->respToken;
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
 *	@brief Sends ^Z character to ensure BGx is not in text mode.
 */
void atcmd_exitTextMode()
{
    char ctrlZ[] = {0x1A};
    IOP_startTx(ctrlZ, sizeof(ctrlZ));
}


/**
 *	@brief Sends break sequence to transition BGx out of fixed-size data mode to command mode (up to 1500 char).
 */
void atcmd_exitDataMode()
{
}


/**
 *	@brief Sends +++ sequence to transition BGx out of transparent data mode to command mode.
 */
void atcmd_exitTransparentMode()
{
    lqDelay(1000);
    IOP_startTx("+++", 3); // send +++, gaurded by 1 second of quiet
    lqDelay(1000);
}


#pragma endregion

#pragma region LTEmC Internal Functions
/*-----------------------------------------------------------------------------------------------*/

/**
 *  @brief Awaits exclusive access to QBG module command interface.
 */
bool atcmd_awaitLock(uint16_t timeoutMS)
{
    return true;

    // uint32_t waitStart = lqMillis();
    // do
    // {
    //     if (!g_lqLTEM.atcmd->isOpenLocked) // if not existing lock
    //     {
    //         g_lqLTEM.atcmd->isOpenLocked = true; // ...acquired new lock
    //         return true;
    //     }
    //     pYield();        // call back to platform yield() in case there is work there that can be done
    //     ltem_eventMgr(); // process any new receives prior to starting cmd invoke

    // } while (lqMillis() - waitStart < timeoutMS); // until timed out
    // return false;                                // timed out waiting for lock
}

/**
 *	@brief Returns the current atCmd lock state
 */
// bool atcmd_isLockActive()
// {
//     return g_lqLTEM.atcmd->isOpenLocked;
// }

/**
 *	@brief Checks receive buffer for command response and sets atcmd structure data with result.
 */
static resultCode_t S__readResult()
{
    g_lqLTEM.atcmd->parserResult = cmdParseRslt_pending;
    g_lqLTEM.atcmd->resultCode = 0;
    uint16_t peekedLen;

    ltem_eventMgr(); // check for URC events preceeding cmd response

    if (bbffr_getOccupied(g_lqLTEM.iop->rxBffr) > 0)
    {
        // chk if current command services a stream and a data mode handler is registered
        if (g_lqLTEM.atcmd->dataMode.dataHndlr != NULL)
        {
            // looking for streamPrefix phrase
            if (BBFFR_ISFOUND(bbffr_find(g_lqLTEM.iop->rxBffr, g_lqLTEM.atcmd->dataMode.trigger, 0, 0, true)))
            {
                g_lqLTEM.atcmd->dataMode.dmState = dmState_triggered;

                DPRINT_V(PRNT_MAGENTA, "(S__readResult) trigger=%s fired, invoking handler\r\n", g_lqLTEM.atcmd->dataMode.trigger);
                // memset(peekBffr, 0, sizeof(peekBffr));
                // bbffr_peek(g_lqLTEM.iop->rxBffr, peekBffr, 14);
                // DPRINT_V(PRNT_MAGENTA, "(S__readResult) [peek] post trigger=%s\r\n", peekBffr);


                /* *** invoke dataMode handler ***
                ------------------------------------------- */

                DPRINT_V(PRNT_MAGENTA, "(S__readResult) invoke dataHndlr\r\n");
                resultCode_t dataRslt = (*g_lqLTEM.atcmd->dataMode.dataHndlr)(g_lqLTEM.atcmd->dataMode.streamCtrl);
                DPRINT_V(PRNT_MAGENTA, "(S__readResult) dataHndlr:rslt=%d\r\n", dataRslt);
                g_lqLTEM.atcmd->dataMode.dmState = dmState_idle;                                    // dataMode completed, go back to _idle

                if (dataRslt == resultCode__success)
                {
                    g_lqLTEM.atcmd->parserResult = cmdParseRslt_success;
                    g_lqLTEM.atcmd->resultCode = dataRslt;
                    if (g_lqLTEM.atcmd->dataMode.runParserAfterDataMode)
                        g_lqLTEM.atcmd->parserResult = cmdParseRslt_pending;
                }
                else if (dataRslt == resultCode__timeout)
                {
                    g_lqLTEM.atcmd->parserResult = cmdParseRslt_error & cmdParseRslt_timeoutError;
                    g_lqLTEM.atcmd->resultCode = resultCode__timeout;
                }
                else
                {
                    g_lqLTEM.atcmd->parserResult = cmdParseRslt_error;
                    g_lqLTEM.atcmd->resultCode = dataRslt;
                }
            }
        }

        uint8_t respLen = strlen(g_lqLTEM.atcmd->rawResponse);                                      // response so far
        uint8_t popSz = MIN(atcmd__respBufferSz - respLen - 1, bbffr_getOccupied(g_lqLTEM.iop->rxBffr));
        ASSERT((respLen + popSz) < atcmd__respBufferSz);                                            // ensure don't overflow

        if (g_lqLTEM.atcmd->parserResult == cmdParseRslt_pending)
        {
            bbffr_pop(g_lqLTEM.iop->rxBffr, g_lqLTEM.atcmd->rawResponse + respLen, popSz);          // pop new into response buffer for parsing
            DPRINT_V(PRNT_GRAY, "(S__readResult) rawResponse:\"%s\"\r\n", g_lqLTEM.atcmd->rawResponse);

            /* *** parse for command response ***
             ------------------------------------------- */
            DPRINT_V(PRNT_GRAY, "(S__readResult) invoke parser\r\n", g_lqLTEM.atcmd->parserResult);
            g_lqLTEM.atcmd->parserResult = (*g_lqLTEM.atcmd->responseParserFunc)();
            DPRINT_V(PRNT_GRAY, "(S__readResult) parser:pRslt=%d \r\n", g_lqLTEM.atcmd->parserResult);
        }
    }

    if (g_lqLTEM.atcmd->parserResult & cmdParseRslt_error)                                          // check error bit
    {
        if (g_lqLTEM.atcmd->parserResult & cmdParseRslt_moduleError)                                // BGx ERROR or CME/CMS
        {
            g_lqLTEM.atcmd->resultCode = resultCode__extendedCodesBase + strtol(atcmd_getToken(0), NULL, 10);
        }
        else if (g_lqLTEM.atcmd->parserResult & cmdParseRslt_countShort)                            // did not find expected tokens
        {
            g_lqLTEM.atcmd->resultCode = resultCode__notFound;
        }
        else
        {
            g_lqLTEM.atcmd->resultCode = resultCode__internalError;                                 // covering the unknown
        }
        atcmd_close();                                                                              // close action to release action lock on any error
    }

    if (g_lqLTEM.atcmd->parserResult == cmdParseRslt_pending)                                       // still pending, check for timeout error
    {
        if (IS_ELAPSED(g_lqLTEM.atcmd->invokedAt, g_lqLTEM.atcmd->timeout))
        {
            g_lqLTEM.atcmd->resultCode = resultCode__timeout;
            g_lqLTEM.atcmd->isOpenLocked = false;                                                   // close action to release action lock
            g_lqLTEM.atcmd->execDuration = lqMillis() - g_lqLTEM.atcmd->invokedAt;

            if (ltem_getDeviceState() != deviceState_ready)                                         // if action timed-out, verify not a device wide failure
            {
                ltem_notifyApp(appEvent_fault_hardLogic, "LTEm Not Ready");
                ltem_notifyApp(appEvent_fault_hardLogic, g_lqLTEM.atcmd->cmdStr);
            }
            else if (!SC16IS7xx_ping())
            {
                ltem_notifyApp(appEvent_fault_softLogic, "LTEm SPI Fault");                         // UART bridge SPI not initialized correctly, IRQ not enabled
            }
            return resultCode__timeout;
        }
        return resultCode__unknown;
    }

    if (g_lqLTEM.atcmd->parserResult & cmdParseRslt_success)                        // success bit: parser completed with success (may have excessRecv warning)
    {
        if (g_lqLTEM.atcmd->autoLock)                                               // if the individual cmd is controlling lock state
            g_lqLTEM.atcmd->isOpenLocked = false;                                   // equivalent to atcmd_close()
        g_lqLTEM.atcmd->execDuration = lqMillis() - g_lqLTEM.atcmd->invokedAt;
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
#define CMx_COMPLETED_LENGTH 17
#define ERROR_COMPLETED_STRING "ERROR\r\n"
#define ERROR_VALUE_OFFSET 7
#define FAIL_COMPLETED_STRING "FAIL\r\n"
#define FAIL_VALUE_OFFSET 6
#define NOCARRIER_COMPLETED_STRING "NO CARRIER\r\n"
#define NOCARRIER_VALUE_OFFSET 12
#define CME_PREABLE "+CME ERROR:"
#define CME_PREABLE_SZ 11

// /**
//  *	@brief Validate the response ends in a BGxx OK value.
//  */
// cmdParseRslt_t atcmd_okResponseParser()
// {
//     return atcmd_stdResponseParser("", false, "", 0, 0, "OK\r\n", 0);
// }


cmdParseRslt_t atcmd_defaultResponseParser()
{
    if (g_lqLTEM.atcmd->parserConfig.configSet)
    {
        return atcmd_stdResponseParser(
            g_lqLTEM.atcmd->parserConfig.preamble,
            g_lqLTEM.atcmd->parserConfig.preambleReqd, 
            g_lqLTEM.atcmd->parserConfig.delimiters,
            g_lqLTEM.atcmd->parserConfig.tokensReqd, 
            0,
            g_lqLTEM.atcmd->parserConfig.finale,
            g_lqLTEM.atcmd->parserConfig.lengthReqd);
    }
    else
        return atcmd_stdResponseParser("", false, "", 0, 0, "OK\r\n", 0);

}


/**
 *	@brief Stardard atCmd response parser, flexible response pattern match and parse.
 */
// cmdParseRslt_t atcmd_stdResponseParser(const char *pPreamble, bool preambleReqd, const char *pDelimeters, uint8_t tokensReqd, uint8_t valueIndx, const char *pFinale, uint16_t lengthReqd)
cmdParseRslt_t atcmd_stdResponseParser(const char *pPreamble, bool preambleReqd, const char *pDelimeters, uint8_t tokensReqd, uint8_t valueIndx, const char *pFinale, uint16_t lengthReqd)
{
    // char *pPreamble;
    // bool preambleReqd;
    // char *pDelimeters; 
    // uint8_t tokensReqd; 
    // uint8_t valueIndx;
    // char *pFinale;
    // uint16_t lengthReqd;

    // if (g_lqLTEM.atcmd->parserConfig.configSet)
    // {
    //     strcpy(pPreamble, g_lqLTEM.atcmd->parserConfig.preamble);
    //     preambleReqd = g_lqLTEM.atcmd->parserConfig.preambleReqd;
    //     strcpy(pDelimeters, g_lqLTEM.atcmd->parserConfig.delimiters);
    //     tokensReqd = g_lqLTEM.atcmd->parserConfig.tokensReqd;
    //     valueIndx = 0;
    //     strcpy(pFinale, g_lqLTEM.atcmd->parserConfig.finale);
    //     lengthReqd = g_lqLTEM.atcmd->parserConfig.lengthReqd;
    // }
    // else
    // {
    //     return atcmd_stdResponseParser("", false, "", 0, 0, "OK\r\n", 0);

    //     pPreamble[0] = '\0';
    //     preambleReqd = g_lqLTEM.atcmd->parserConfig.preambleReqd;
    //     strcpy(pDelimeters, g_lqLTEM.atcmd->parserConfig.delimiters);
    //     tokensReqd = g_lqLTEM.atcmd->parserConfig.tokensReqd;
    //     valueIndx = 0;
    //     strcpy(pFinale, g_lqLTEM.atcmd->parserConfig.finale);
    //     lengthReqd = g_lqLTEM.atcmd->parserConfig.lengthReqd;

    // }

    cmdParseRslt_t parseRslt = cmdParseRslt_pending;

    ASSERT(!(preambleReqd && STREMPTY(pPreamble)));                             // if preamble required, cannot be empty
    ASSERT(pPreamble != NULL && pDelimeters != NULL && pFinale != NULL);        // char params are not NULL, must be valid empty char arrays
    ASSERT((!(tokensReqd || valueIndx) || strlen(pDelimeters) > 0));            // if tokens count or value return, need delimiter

    uint8_t preambleLen = strlen(pPreamble);
    uint8_t reqdPreambleLen = preambleReqd ? preambleLen : 0;
    uint8_t finaleLen = strlen(pFinale);
    uint16_t responseLen = strlen(g_lqLTEM.atcmd->rawResponse);

    // always look for error, short-circuit result if CME/CMS
    char *pErrorLoctn;
    if ((pErrorLoctn = strstr(g_lqLTEM.atcmd->rawResponse, "+CM")) || (pErrorLoctn = strstr(g_lqLTEM.atcmd->rawResponse, "ERROR")))
    {
        for (size_t i = 0; i < ltem__errorDetailSz; i++)                        // copy raw chars: unknown incoming format, stop at line end
        {
            if ((pErrorLoctn[i] == '\r' || pErrorLoctn[i] == '\n'))
                break;
            ;
            g_lqLTEM.atcmd->errorDetail[i] = pErrorLoctn[i];
        }
        return cmdParseRslt_error | cmdParseRslt_moduleError;
    }

    bool lengthSatisfied = responseLen >= lengthReqd &&
                           responseLen >= (reqdPreambleLen + finaleLen);
    if (!lengthSatisfied)                                                       // still pending, haven't recv'd required cnt of chars
        return cmdParseRslt_pending;

    /* Response length has been satisfied (and no error detected).
     * Search response for preamble, finale, token count (tokensReqd/valueIndx)
     */

    while (g_lqLTEM.atcmd->response[0] == '\r' || g_lqLTEM.atcmd->response[0] == '\n')
    {
        g_lqLTEM.atcmd->response++;                                             // skip past prefixing line terminators
    }

    bool preambleSatisfied = false;
    if (preambleLen) // if pPreamble provided
    {
        char *pPreambleLoctn = strstr(g_lqLTEM.atcmd->rawResponse, pPreamble);  // find it in response
        if (pPreambleLoctn)
        {
            preambleSatisfied = true;
            g_lqLTEM.atcmd->preambleFound = true;
            g_lqLTEM.atcmd->response = pPreambleLoctn + preambleLen;            // remove pPreamble from retResponse
        }
        else if (preambleReqd)
        {
            if (responseLen >= preambleLen)
                return cmdParseRslt_error | cmdParseRslt_preambleMissing; // require preamble missing, response is sufficient for match

            return cmdParseRslt_pending; // keep waiting on response
        }
        else  // !preambleReqd
        {
            if (strstr(g_lqLTEM.atcmd->rawResponse, pFinale))
            {
                preambleSatisfied = true;
                g_lqLTEM.atcmd->preambleFound = false;
                return cmdParseRslt_success;
            }
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
                // pFinaleLoctn[0] = '\0';
            }
        }
    }

    /*  Parse content between pPreamble/response start and pFinale for tokens (reqd cnt) and value extraction
     *  Only supporting one delimiter for now; support for optional delimeter list in future won't require API change
     */
    bool tokenCntSatified = !(tokensReqd || valueIndx);
    if (finaleSatisfied && !tokenCntSatified) // count tokens to service value return or validate required token count
    {
        tokenCnt = 1;
        char *pDelimeterAt;
        char *pTokenAt = g_lqLTEM.atcmd->response;
        do
        {
            // if (tokenCnt == valueIndx) // grab value, this is what is requested
            //     g_lqLTEM.atcmd->retValue = strtol(pTokenAt, NULL, 0);

            pDelimeterAt = strpbrk(pTokenAt, pDelimeters);       // look for delimeter/next token
            if (tokenCnt >= tokensReqd && tokenCnt >= valueIndx) // at/past required token = done
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
            parseRslt |= cmdParseRslt_error | cmdParseRslt_countShort; // set error and count short bits
    }

    if (!(parseRslt & cmdParseRslt_error) && // check error bit
        preambleSatisfied &&
        finaleSatisfied &&
        tokenCntSatified) // and all component test conditions
    {
        parseRslt |= cmdParseRslt_success; // no error, preserve possible warnings (excess recv, etc.)
    }
    return parseRslt; // done
}


/**
 *	@brief Stardard TX (out) data handler used by dataMode.
 */
resultCode_t atcmd_txHndlrDefault()
{
    DPRINT_V(PRNT_CYAN, "(atcmd_txHndlrDefault) entered\r\n");
    uint8_t triggerSz = strlen(g_lqLTEM.atcmd->dataMode.trigger);
    bbffr_skipTail(g_lqLTEM.iop->rxBffr, triggerSz);                                        // clear out recv'd trigger from RX bffr

    IOP_startTx(g_lqLTEM.atcmd->dataMode.txDataLoc, g_lqLTEM.atcmd->dataMode.txDataSz);     // send of datamode content

    uint32_t startTime = lqMillis();
    while (lqMillis() - startTime < g_lqLTEM.atcmd->timeout)
    {
        uint16_t trlrIndx = bbffr_find(g_lqLTEM.iop->rxBffr, "OK", 0, 0, false);
        if (BBFFR_ISFOUND(trlrIndx))
        {
            bbffr_pop(g_lqLTEM.iop->rxBffr, g_lqLTEM.atcmd->rawResponse, trlrIndx);
            bbffr_skipTail(g_lqLTEM.iop->rxBffr, OK_COMPLETED_LENGTH);                      // OK + line-end
            return resultCode__success;
        }
        trlrIndx = bbffr_find(g_lqLTEM.iop->rxBffr, "+CM", 0, 0, false);
        if (BBFFR_ISFOUND(trlrIndx))
        {
            bbffr_pop(g_lqLTEM.iop->rxBffr, g_lqLTEM.atcmd->rawResponse, trlrIndx);
            bbffr_skipTail(g_lqLTEM.iop->rxBffr, CMx_COMPLETED_LENGTH);                     // +CM? ERROR: ### + line-end
            return resultCode__partialContent;
        }
        lqDelay(1);
    }
    return resultCode__timeout;
}

/**
 * @brief Stream RX data handler accepting data length at RX buffer tail.
 */
resultCode_t atcmd_rxHndlrWithLength()
{
    char wrkBffr[32] = {0};
    uint16_t lengthEOLAt;
    
    uint32_t trailerWaitStart = lqMillis();
    do
    {
        lengthEOLAt = bbffr_find(g_lqLTEM.iop->rxBffr, "\r", 0, streams__dataModeMaxPreambleSz, false);     // find EOL from CONNECT response
        if (IS_ELAPSED(trailerWaitStart, streams__lengthWaitDuration))
            return resultCode__timeout;

    } while (!BBFFR_ISFOUND(lengthEOLAt));
   
    bbffr_pop(g_lqLTEM.iop->rxBffr, wrkBffr, lengthEOLAt + 2);                                              // pop data length and EOL from RX buffer


    DPRINT_V(PRNT_MAGENTA, "(_rxHndlrWithLength) wrkBffr (w/header)=%s\r\n", wrkBffr);

    uint8_t triggerSz = strnlen(g_lqLTEM.atcmd->dataMode.trigger, sizeof(g_lqLTEM.atcmd->dataMode.trigger));
    uint16_t readLen = strtol(wrkBffr + triggerSz, NULL, 10);
    g_lqLTEM.atcmd->dataMode.rxDataSz = readLen;                                                            // stash reported read length
    uint16_t streamId = g_lqLTEM.atcmd->dataMode.streamCtrl->dataCntxt;
    DPRINT_V(PRNT_CYAN, "(atcmd_rxHndlrWithLength) streamId=%d readLen=%d\r\n",  streamId, readLen);

    uint32_t readStart = lqMillis();
    uint16_t bffrOccupiedCnt;
    do
    {
        bffrOccupiedCnt = bbffr_getOccupied(g_lqLTEM.iop->rxBffr);
        if (IS_ELAPSED(readStart, g_lqLTEM.atcmd->timeout))
        {
            g_lqLTEM.atcmd->dataMode.rxDataSz = 0;                                                          // report as failed read, length unknown
            DPRINT(PRNT_WARN, "(atcmd_rxHndlrWithLength) bffr timeout: %d rcvd\r\n", bffrOccupiedCnt);
            return resultCode__timeout;                                                                     // return timeout waiting for bffr fill
        }
    } while (bffrOccupiedCnt < readLen + streams__contentLengthTrailerSz);
    
    do                                                                                                      // *NOTE* depending on buffer wrap may take 2 ops
    {
        char* streamPtr;
        uint16_t blockSz = bbffr_popBlock(g_lqLTEM.iop->rxBffr, &streamPtr, readLen);                       // get contiguous block size from rxBffr
        DPRINT_V(PRNT_CYAN, "(atcmd_rxHndlrWithLength) ptr=%p, bSz=%d, rSz=%d\r\n", streamPtr, blockSz, readLen);
        uint8_t streamId = g_lqLTEM.atcmd->dataMode.streamCtrl->dataCntxt;
        (*g_lqLTEM.fileCtrl->appRecvDataCB)(streamId, streamPtr, blockSz);                                  // forward to application
        bbffr_popBlockFinalize(g_lqLTEM.iop->rxBffr, true);                                                 // commit POP
        readLen -= blockSz;
    } while (readLen > 0);

    if (bbffr_getOccupied(g_lqLTEM.iop->rxBffr) >= streams__contentLengthTrailerSz)                         // cleanup, remove trailer
    {
        bbffr_skipTail(g_lqLTEM.iop->rxBffr, streams__contentLengthTrailerSz);
    }
    return resultCode__success;
}


#pragma endregion // completion parsers/data handlers


#pragma region Static Function Definitions
/*-----------------------------------------------------------------------------------------------*/

/**
 *	@brief Returns a token from the result of the last module command
 *  @param [in] preamble Character phrase prefixing the section of the response to search
 *  @param [in] tokenIndx The 0-based token index to return
 *  @param [out] token Char pointer to found token (will be returned null-terminated)
 *  @param [in] tkBffrLen Size of the application provided buffer to hold the returned token
 */
static void lqStr_getToken(const char * source, char delimiter, uint8_t tokenIndx, char * destBffr, uint8_t destBffrSz)
{
    const char* delimPtr = source;                                                                // prepare for search
    memset(destBffr, 0, destBffrSz);
    uint8_t tokenCnt = 0;

    char* endPtr = strchr(source, '\r');
    if (endPtr == NULL)
        endPtr = strchr(source, '\0');

    do
    {
        delimPtr = strchr(source, delimiter);                                               // next delimiter
        delimPtr = (delimPtr == NULL) ? endPtr : delimPtr;
        if (tokenCnt == tokenIndx)
        {
            strncpy(destBffr, source, MIN(delimPtr - source, destBffrSz));                  // copy out vs. change response buffer, ensure no dest overflow
            return;
        }
        source = delimPtr + 1;
        tokenCnt++;
    } while (source < endPtr);
}


/**
 * @brief Clean out any prior command debris from rx buffer. 
 * @details Targets typical patterns for removal, rather than reseting (clearing) the RX buffer.
 */
static bool S__cleanRxBffr()
{
    if (BBFFR_ISFOUND(bbffr_find(g_lqLTEM.iop->rxBffr, "OK\r\n", 0, 0, true)))                  // likely timeout from previous cmd
    {
        bbffr_skipTail(g_lqLTEM.iop->rxBffr, 4);
        return true;
    }

    // potential other cleaning actions
    return false;
}


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
//     //     DPRINT(PRNT_CYAN, "-p=sslURC");
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
//     // //     DPRINT(PRNT_CYAN, "-p=ipURC");
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
//     //     DPRINT(PRNT_CYAN, "-p=ipURC");
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
//     //     DPRINT(PRNT_CYAN, "-p=mqttR");
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
//     //     DPRINT(PRNT_CYAN, "-p=mqttS");
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
//     //     DPRINT(PRNT_CYAN, "-p=pdpD");
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
