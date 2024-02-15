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
#define lqLOG_LEVEL lqLOGLEVEL_DBG
//#define DISABLE_ASSERTS                                 // ASSERT/ASSERT_W enabled by default, can be disabled 
#define LQ_SRCFILE "ATC"                                // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT

#include <stdarg.h>
#include "ltemc-internal.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
extern ltemDevice_t g_lqLTEM;

/* Static Function Declarations
------------------------------------------------------------------------------------------------- */
static resultCode_t S__readResult();
static bool S__cleanRxBffr();
static bool lqStr_getToken(const char * source, char delimiter, uint8_t tokenIndx, char * destBffr, uint8_t destBffrSz);


#pragma region Public Functions
/*-----------------------------------------------------------------------------------------------*/

/**
 *	@brief Resets AT-CMD last execution result properties.
 */
void atcmd_resetPreInvoke()
{
    memset(g_lqLTEM.atcmd->cmdStr, 0, atcmd__cmdBufferSz);
    memset(g_lqLTEM.atcmd->rawResponse, 0, atcmd__respBufferSz);
    g_lqLTEM.atcmd->resultCode = 0;
    g_lqLTEM.atcmd->resultValue = 0;
    g_lqLTEM.atcmd->invokedAt = 0;
    g_lqLTEM.atcmd->execDuration = 0;
    g_lqLTEM.atcmd->response = g_lqLTEM.atcmd->rawResponse;                 // reset data component of response to full-response
}


/**
 *	@brief Resets AT-CMD next execution invoke properties.
 */
void atcmd_resetPostInvoke()
{
    g_lqLTEM.iop->txSrc = &g_lqLTEM.atcmd->cmdStr;                          // IOP pointer to current "talker" back to cmd dispatcher
    g_lqLTEM.iop->txPending = 0;

    g_lqLTEM.atcmd->dispatchReady = true;
    g_lqLTEM.atcmd->dRdyTimeout = atcmd__dRdyTimeoutDefault;
    g_lqLTEM.atcmd->dCmpltTimeout = atcmd__dCmpltTimeoutDefault;
    g_lqLTEM.atcmd->responseParserFunc = atcmd_defaultResponseParser;
    g_lqLTEM.atcmd->parserConfig.configSet = false;
}


/**
 * @brief Sets command wait for exclusive timeout for next invocation of a module command. 
 */
uint32_t atcmd_ovrrdDRdyTimeout(uint32_t newTimeout)
{
    uint32_t oldTimeout = g_lqLTEM.atcmd->dRdyTimeout;
    if (newTimeout > 0)
        g_lqLTEM.atcmd->dRdyTimeout = newTimeout;
    else
        g_lqLTEM.atcmd->dRdyTimeout = atcmd__dRdyTimeoutDefault;
    return oldTimeout;
}


/**
 * @brief Sets command execution timeout for next invocation of a module command. 
 */
uint32_t atcmd_ovrrdDCmpltTimeout(uint32_t newTimeout)
{
    uint32_t oldTimeout = g_lqLTEM.atcmd->dCmpltTimeout;
    if (newTimeout > 0)
        g_lqLTEM.atcmd->dCmpltTimeout = newTimeout;
    else
        g_lqLTEM.atcmd->dCmpltTimeout = atcmd__dCmpltTimeoutDefault;
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
    ASSERT(!preambleReqd || (preamble != NULL && preamble[0] != '\0'));
    ASSERT(delimiters != NULL && delimiters[0] != '\0');

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
    // if (g_lqLTEM.atcmd->isOpenLocked)
    //     return resultCode__locked;

    uint32_t dRdyWaitStart = lqMillis();
    do
    {
        if (IS_ELAPSED(dRdyWaitStart, g_lqLTEM.atcmd->dRdyTimeout))
        {
            return resultCode__gtwyTimeout;
        }
        lqDelay(100);                                                           // pause task, give access else where

    } while (!g_lqLTEM.atcmd->dispatchReady);                                   // wait for other task to release dispatcher

    g_lqLTEM.atcmd->dispatchReady = false;                                      // then we take it here
    atcmd_resetPreInvoke();                                                     // clear results props from ATCMD control structure

    va_list ap;
    va_start(ap, cmdTemplate);
    vsnprintf(g_lqLTEM.atcmd->cmdStr, sizeof(g_lqLTEM.atcmd->cmdStr), cmdTemplate, ap);
    strcat(g_lqLTEM.atcmd->cmdStr, "\r");

    // if (!atcmd_awaitLock(g_lqLTEM.atcmd->timeout)) // attempt to acquire new atCmd lock for this instance
    //     return false;

    if (S__cleanRxBffr())
    {
        lqLOG_WARN("(atcmd_dispatch) debris cleaned from RX buffer\r\n");
    }
    lqLOG_DBG(lqCYAN, "(atcmd_dispatch) cmd(%d):%s", strlen(g_lqLTEM.atcmd->cmdStr), g_lqLTEM.atcmd->cmdStr);
    lqLOG_DBG(lqCYAN, "\r\n");
    IOP_startTx(g_lqLTEM.atcmd->cmdStr, strlen(g_lqLTEM.atcmd->cmdStr));

    /* await result phase
     ----------------------------------------- */
    resultCode_t rslt = resultCode__unknown; // resultCode_t result;

    lqLOG_VRBS("(atcmd_dispatch) reading result...\r\n");
    g_lqLTEM.atcmd->invokedAt = lqMillis();

    do
    {
        if (bbffr_getOccupied(g_lqLTEM.iop->rxBffr) && !g_lqLTEM.atcmd->eventMgrInvoked)    // possible URC incoming
        {
            g_lqLTEM.atcmd->eventMgrInvoked = true;
            ltem_eventMgr();
            g_lqLTEM.atcmd->eventMgrInvoked = false;
        }

        lqLOG_VRBS("~");
        rslt = S__readResult();
        lqLOG_VRBS("-");
        if (g_lqLTEM.cancellationRequest)                                           // test for cancellation (RTOS or IRQ)
        {
            g_lqLTEM.atcmd->resultCode = resultCode__cancelled;
            break;
        }
        lqDelay(10);                                                                // give back control momentarily before next loop pass (lqDelay is non-blocking)
    } while (rslt == resultCode__unknown);

    lqLOG_DBG(lqCYAN, "(atcmd_dispatch) resp:%s\r\n", atcmd_getRawResponse());
    atcmd_resetPostInvoke();                                                        // cmd completed, reset next invoke options back to default values
    return g_lqLTEM.atcmd->resultCode;
}


// /**
//  *	@brief Invokes a BGx AT command using default option values (automatic locking).
//  */
// bool atcmd_tryInvoke(const char *cmdTemplate, ...)
// {
//     // if (g_lqLTEM.atcmd->isOpenLocked)
//     //     return false;

//     atcmd_resetPreInvoke();
//     // g_lqLTEM.atcmd->autoLock = atcmd__setLockModeAuto;                              // set automatic lock control mode

//     // char *cmdStr = g_lqLTEM.atcmd->cmdStr;
//     va_list ap;

//     va_start(ap, cmdTemplate);
//     vsnprintf(g_lqLTEM.atcmd->cmdStr, sizeof(g_lqLTEM.atcmd->cmdStr), cmdTemplate, ap);
//     strcat(g_lqLTEM.atcmd->cmdStr, "\r");

//     // if (!atcmd_awaitLock(g_lqLTEM.atcmd->timeout))                                    // attempt to acquire new atCmd lock for this instance
//     //     return false;

//     if (S__cleanRxBffr())
//         lqLOG_WARN("Debris cleaned from RX buffer\r\n");

//     lqLOG_VRBS("(atcmd_tryInvoke) cmd=%s\r\n", g_lqLTEM.atcmd->cmdStr);
//     IOP_startTx(g_lqLTEM.atcmd->cmdStr, strlen(g_lqLTEM.atcmd->cmdStr));
//     return true;
// }


// /**
//  *	@brief Invokes a BGx AT command without acquiring a lock, using previously set setOptions() values.
//  */
// void atcmd_invokeReuseLock(const char *cmdTemplate, ...)
// {
//     // ASSERT(g_lqLTEM.atcmd->isOpenLocked); // function assumes re-use of existing lock

//     atcmd_resetPreInvoke();
//     g_lqLTEM.atcmd->autoLock = atcmd__setLockModeManual;

//     char *cmdStr = g_lqLTEM.atcmd->cmdStr;
//     va_list ap;

//     va_start(ap, cmdTemplate);
//     vsnprintf(cmdStr, sizeof(g_lqLTEM.atcmd->cmdStr), cmdTemplate, ap);
//     strcat(g_lqLTEM.atcmd->cmdStr, "\r");

//     if (S__cleanRxBffr())
//         lqLOG_WARN("(atcmd_invokeReuseLock) debris cleaned from RX buffer\r\n");

//    lqLOG_VRBS("(atcmd_invokeReuseLock) cmd=%s\r\n", g_lqLTEM.atcmd->cmdStr);
//    IOP_startTx(g_lqLTEM.atcmd->cmdStr, strlen(g_lqLTEM.atcmd->cmdStr));
// }


/**
 *	@brief Closes (completes) a BGx AT command structure and frees action resource (release action lock).
 */
void atcmd_close()
{
    g_lqLTEM.atcmd->isOpenLocked = false;
    g_lqLTEM.atcmd->execDuration = lqMillis() - g_lqLTEM.atcmd->invokedAt;
}


// /**
//  *	@brief Waits for atcmd result, periodically checking recv buffer for valid response until timeout.
//  */
// resultCode_t atcmd_awaitResult()
// {
//     resultCode_t rslt = resultCode__unknown;                                    // resultCode_t result;
//     g_lqLTEM.atcmd->invokedAt = lqMillis();
//     do
//     {
//         rslt = S__readResult();

//         if (g_lqLTEM.cancellationRequest)                                       // test for cancellation (RTOS or IRQ)
//         {
//             g_lqLTEM.atcmd->resultCode = resultCode__cancelled;
//             break;
//         }
//         lqDelay(50);                                                            // lqDelay() incorporates yield to give application processing opportunity
//     } while (rslt == resultCode__unknown);

//     // reset options back to default values
//     g_lqLTEM.atcmd->timeout = atcmd__defaultTimeout;
//     g_lqLTEM.atcmd->responseParserFunc = atcmd_defaultResponseParser;
//     g_lqLTEM.atcmd->parserConfig.configSet = false;
//     return g_lqLTEM.atcmd->resultCode;
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
 *	@brief Returns the string captured from the last command response; between preamble and finale (excludes both)
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
 *	@brief Returns a token from the result of the last module command or NULL if not found
 */
char * atcmd_getToken(uint8_t tokenIndx)
{
    char * startPtr = memchr(g_lqLTEM.atcmd->rawResponse, ':', atcmd__respBufferSz);    // list of response values are prefixed by ": "
    if (startPtr == NULL)
        startPtr = g_lqLTEM.atcmd->rawResponse;                                         // assume the string to tokenize has no preamble
    else
        startPtr += 2;                                                                  // point past ": " to content

    lqLOG_VRBS("(atcmd_getToken) toParse:%s\r\n", startPtr);

    if (lqStr_getToken(startPtr, ',', tokenIndx, g_lqLTEM.atcmd->respToken, atcmd__respTokenSz))
    {
        lqLOG_VRBS("(atcmd_getToken) indx=%d returns:%s\r\n", tokenIndx, g_lqLTEM.atcmd->respToken);
        return g_lqLTEM.atcmd->respToken;
    }
    lqLOG_INFO("(atcmd_getToken) Empty source/insufficient tokens\r\n");
    return "";
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
    g_lqLTEM.atcmd->resultCode = resultCode__unknown;
    uint16_t peekedLen;

    if (bbffr_getOccupied(g_lqLTEM.iop->rxBffr) > 0)
    {
        // chk if current command services a stream and a data mode handler is registered
        if (g_lqLTEM.atcmd->dataMode.dataHndlr != NULL)
        {
            // looking for streamPrefix phrase
            if (BBFFR_ISFOUND(bbffr_find(g_lqLTEM.iop->rxBffr, g_lqLTEM.atcmd->dataMode.trigger, 0, 0, true)))
            {
                g_lqLTEM.atcmd->dataMode.dmState = dmState_triggered;

                lqLOG_VRBS("(S__readResult) trigger=%s fired, invoking handler\r\n", g_lqLTEM.atcmd->dataMode.trigger);
                lqLOG_VRBS("(S__readResult) txSz=%d, rxSz=%d\r\n", g_lqLTEM.atcmd->dataMode.txDataSz, g_lqLTEM.atcmd->dataMode.rxDataSz);

                /* *** invoke DM data handler ***
                ------------------------------------------- */
                resultCode_t dataRslt = (*g_lqLTEM.atcmd->dataMode.dataHndlr)(g_lqLTEM.atcmd->dataMode.streamCtrl);
                lqLOG_INFO("(S__readResult) dataHndlr:rslt=%d\r\n", dataRslt);
                /* --------------------------------------------
                *** invoke DM data handler *** */

                g_lqLTEM.atcmd->dataMode.dmState = dmState_idle;                                    // dataMode completed, go back to _idle
                if (dataRslt == resultCode__success)
                {
                    g_lqLTEM.atcmd->parserResult = cmdParseRslt_complete;
                    g_lqLTEM.atcmd->resultCode = dataRslt;
                    if (g_lqLTEM.atcmd->dataMode.runParserAfterDataMode)
                        g_lqLTEM.atcmd->parserResult = cmdParseRslt_pending;                        // revert to pending if specified
                }
                else if (dataRslt == resultCode__timeout)
                {
                    g_lqLTEM.atcmd->parserResult = cmdParseRslt_complete | cmdParseRslt_timeoutError;
                    g_lqLTEM.atcmd->resultCode = resultCode__timeout;
                }
                else
                {
                    g_lqLTEM.atcmd->parserResult = cmdParseRslt_complete | cmdParseRslt_generalError;
                    g_lqLTEM.atcmd->resultCode = dataRslt;
                }
            }
        }

        if (g_lqLTEM.atcmd->parserResult == cmdParseRslt_pending)
        {
            uint8_t respLen = strlen(g_lqLTEM.atcmd->rawResponse);                                      // response so far
            uint8_t popSz = MIN(atcmd__respBufferSz - respLen - 1, bbffr_getOccupied(g_lqLTEM.iop->rxBffr));
            ASSERT((respLen + popSz) < atcmd__respBufferSz);                                            // ensure don't overflow

            bbffr_pop(g_lqLTEM.iop->rxBffr, g_lqLTEM.atcmd->rawResponse + respLen, popSz);              // pop new into response buffer for parsing
            lqLOG_VRBS("(S__readResult) rawResponse:\"%s\"\r\n", g_lqLTEM.atcmd->rawResponse);

            /* *** parse for command response ***
             ------------------------------------------- */
            lqLOG_VRBS("(S__readResult) invoke parser\r\n");
            g_lqLTEM.atcmd->parserResult = (*g_lqLTEM.atcmd->responseParserFunc)();
            lqLOG_VRBS("(S__readResult) parser:pRslt=%d \r\n", g_lqLTEM.atcmd->parserResult);
            /* --------------------------------------------
            *** parse for command response *** */
        }
    }

    if (g_lqLTEM.atcmd->parserResult & cmdParseRslt_errorMask)                                          // check error bits
    {
        if (g_lqLTEM.atcmd->parserResult & cmdParseRslt_moduleError)                                    // BGx ERROR or CME/CMS
        {
            char * cmError = atcmd_getToken(0);
            ASSERT(cmError);
            g_lqLTEM.atcmd->resultCode = resultCode__extendedCodesBase + strtol(cmError, NULL, 10);
        }
        else if (g_lqLTEM.atcmd->parserResult & cmdParseRslt_countShort)                                // did not find expected tokens
        {
            g_lqLTEM.atcmd->resultCode = resultCode__notFound;
        }
        else
        {
            lqLOG_ERROR("(S__readResult) pErr=%d, resp:%s\r\n", g_lqLTEM.atcmd->parserResult, g_lqLTEM.atcmd->rawResponse);
            g_lqLTEM.atcmd->resultCode = resultCode__internalError;                                     // covering the unknown
        }
        atcmd_close();                                                                                  // close action to release action lock on any error
    }

    if (g_lqLTEM.atcmd->parserResult == cmdParseRslt_pending)                                           // still pending, check for timeout error
    {
        if (IS_ELAPSED(g_lqLTEM.atcmd->invokedAt, g_lqLTEM.atcmd->dCmpltTimeout))
        {
            g_lqLTEM.atcmd->resultCode = resultCode__timeout;
            g_lqLTEM.atcmd->isOpenLocked = false;                                                       // close action to release action lock
            g_lqLTEM.atcmd->execDuration = lqMillis() - g_lqLTEM.atcmd->invokedAt;

            if (ltem_getDeviceState() != deviceState_ready)                                             // if action timed-out, verify not a device wide failure
            {
                ltem_notifyApp(appEvent_fault_hardLogic, "LTEm Not Ready");
                ltem_notifyApp(appEvent_fault_hardLogic, g_lqLTEM.atcmd->cmdStr);
            }
            else if (!SC16IS7xx_ping())
            {
                ltem_notifyApp(appEvent_fault_softLogic, "LTEm SPI Fault");                             // UART bridge SPI not initialized correctly, IRQ not enabled
            }
            return resultCode__timeout;
        }
        return resultCode__unknown;
    }

    if (g_lqLTEM.atcmd->parserResult & cmdParseRslt_complete &&
        (g_lqLTEM.atcmd->parserResult & cmdParseRslt_errorMask) == 0)
    {
        if (g_lqLTEM.atcmd->autoLock)                                                                   // if the individual cmd is controlling lock state
            g_lqLTEM.atcmd->isOpenLocked = false;                                                       // equivalent to atcmd_close()
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
    /* to be implemented when all parser wrappers are converted
    */

    // char *pDelimeters; 
    // char *pFinale;
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
    //     char* pPreamble[0] = '\0';
    //     bool preambleReqd = g_lqLTEM.atcmd->parserConfig.preambleReqd;
    //     strcpy(pDelimeters, g_lqLTEM.atcmd->parserConfig.delimiters);
    //     uint8_t = g_lqLTEM.atcmd->parserConfig.tokensReqd;
    //     strcpy(pFinale, g_lqLTEM.atcmd->parserConfig.finale);
    //     uint16_t lengthReqd = g_lqLTEM.atcmd->parserConfig.lengthReqd;
    //     uint8_t valueIndx = 0;
    // }

    cmdParseRslt_t parseRslt = cmdParseRslt_pending;

    uint8_t preambleLen = pPreamble == NULL ? 0 : strlen(pPreamble);
    uint8_t reqdPreambleLen = preambleReqd ? preambleLen : 0;
    uint8_t finaleLen = pFinale == NULL ? 0 : strlen(pFinale);
    uint16_t responseLen = strlen(g_lqLTEM.atcmd->rawResponse);

    // always look for error, short-circuit result if CME/CMS
    char *pErrorLoctn;
    if ((pErrorLoctn = strstr(g_lqLTEM.atcmd->rawResponse, "+CM")) || (pErrorLoctn = strstr(g_lqLTEM.atcmd->rawResponse, "ERROR")))
    {
        return cmdParseRslt_complete | cmdParseRslt_moduleError;
    }

    bool lengthSatisfied = responseLen >= lengthReqd &&
                           responseLen >= (reqdPreambleLen + finaleLen);
    if (!lengthSatisfied)                                                               // still pending, haven't recv'd required cnt of chars
        return cmdParseRslt_pending;

    /* Response length has been satisfied (and no error detected).
     * Search response for preamble, finale, token count (tokensReqd/valueIndx)
     */

    while (g_lqLTEM.atcmd->response[0] < '!')                                           // skip past non-relevant characters
        g_lqLTEM.atcmd->response++;          

    bool preambleSatisfied = false;
    if (preambleLen)                                                                    // if pPreamble provided
    {
        char *pPreambleLoctn = strstr(g_lqLTEM.atcmd->rawResponse, pPreamble);          // find it in response
        if (pPreambleLoctn)
        {
            preambleSatisfied = true;
            g_lqLTEM.atcmd->preambleFound = true;
            g_lqLTEM.atcmd->response = pPreambleLoctn + preambleLen;                    // remove pPreamble from retResponse
        }
        // else if (preambleReqd)
        // {
        //     if (responseLen >= preambleLen)
        //         return cmdParseRslt_complete | cmdParseRslt_preambleMissing;         // require preamble missing, response is sufficient for match

        //     return cmdParseRslt_pending;                                             // keep waiting on response
        // }
        else if (preambleReqd)
        {
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
            parseRslt |= cmdParseRslt_complete | cmdParseRslt_countShort;               // set error and count short bits
    }

    if (!(parseRslt & cmdParseRslt_errorMask) &&                                        // check error bits
        preambleSatisfied &&
        finaleSatisfied &&
        tokenCntSatified)                                                               // and all component test conditions
    {
        parseRslt |= cmdParseRslt_complete;                                             // no error, preserve possible warnings (excess recv, etc.)
    }
    return parseRslt; // done
}


/**
 *	@brief Stardard TX (out) data handler used by dataMode.
 */
resultCode_t atcmd_txHndlrDefault()
{
    lqLOG_VRBS("(atcmd_txHndlrDefault) entered\r\n");
    uint8_t triggerSz = strlen(g_lqLTEM.atcmd->dataMode.trigger);
    bbffr_skipTail(g_lqLTEM.iop->rxBffr, triggerSz);                                    // clear out recv'd trigger from RX bffr

    IOP_startTx(g_lqLTEM.atcmd->dataMode.txDataLoc, g_lqLTEM.atcmd->dataMode.txDataSz); // send of datamode content

    uint32_t startTime = lqMillis();
    while (lqMillis() - startTime < g_lqLTEM.atcmd->dCmpltTimeout)
    {
        uint16_t trlrIndx = bbffr_find(g_lqLTEM.iop->rxBffr, "OK", 0, 0, false);
        if (BBFFR_ISFOUND(trlrIndx))
        {
            bbffr_pop(g_lqLTEM.iop->rxBffr, g_lqLTEM.atcmd->rawResponse, trlrIndx);
            bbffr_skipTail(g_lqLTEM.iop->rxBffr, OK_COMPLETED_LENGTH);                  // OK + line-end
            return resultCode__success;
        }
        trlrIndx = bbffr_find(g_lqLTEM.iop->rxBffr, "+CM", 0, 0, false);
        if (BBFFR_ISFOUND(trlrIndx))
        {
            bbffr_pop(g_lqLTEM.iop->rxBffr, g_lqLTEM.atcmd->rawResponse, trlrIndx);
            bbffr_skipTail(g_lqLTEM.iop->rxBffr, CMx_COMPLETED_LENGTH);                 // +CM? ERROR: ### + line-end
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
   
    bbffr_pop(g_lqLTEM.iop->rxBffr, wrkBffr, lengthEOLAt + 2);                                      // pop data length and EOL from RX buffer

    lqLOG_VRBS("(_rxHndlrWithLength) wrkBffr (w/header)=%s\r\n", wrkBffr);

    uint8_t triggerSz = strnlen(g_lqLTEM.atcmd->dataMode.trigger, sizeof(g_lqLTEM.atcmd->dataMode.trigger));
    ASSERT(wrkBffr + triggerSz);
    uint16_t readLen = lq_strToInt(wrkBffr + triggerSz, NULL, 10);
    g_lqLTEM.atcmd->dataMode.rxDataSz = readLen;                                                    // stash reported read length
    uint16_t streamId = g_lqLTEM.atcmd->dataMode.streamCtrl->dataCntxt;
    lqLOG_INFO("(atcmd_rxHndlrWithLength) streamId=%d readLen=%d\r\n",  streamId, readLen);

    uint32_t readStart = lqMillis();
    uint16_t bffrOccupiedCnt;
    do
    {
        bffrOccupiedCnt = bbffr_getOccupied(g_lqLTEM.iop->rxBffr);
        if (IS_ELAPSED(readStart, g_lqLTEM.atcmd->dCmpltTimeout))
        {
            g_lqLTEM.atcmd->dataMode.rxDataSz = 0;                                                  // report as failed read, length unknown
            lqLOG_WARN("(atcmd_rxHndlrWithLength) bffr timeout: %d rcvd\r\n", bffrOccupiedCnt);
            return resultCode__timeout;                                                             // return timeout waiting for bffr fill
        }
    } while (bffrOccupiedCnt < readLen + streams__contentLengthTrailerSz);
    
    do                                                                                              // *NOTE* depending on buffer wrap may take 2 ops
    {
        char* streamPtr;
        uint16_t blockSz = bbffr_popBlock(g_lqLTEM.iop->rxBffr, &streamPtr, readLen);               // get contiguous block size from rxBffr
        lqLOG_VRBS("(atcmd_rxHndlrWithLength) ptr=%p, bSz=%d, rSz=%d\r\n", streamPtr, blockSz, readLen);
        uint8_t streamId = g_lqLTEM.atcmd->dataMode.streamCtrl->dataCntxt;
        (*g_lqLTEM.fileCtrl->appRecvDataCB)(streamId, streamPtr, blockSz);                          // forward to application
        bbffr_popBlockFinalize(g_lqLTEM.iop->rxBffr, true);                                         // commit POP
        readLen -= blockSz;
    } while (readLen > 0);

    if (bbffr_getOccupied(g_lqLTEM.iop->rxBffr) >= streams__contentLengthTrailerSz)                 // cleanup, remove trailer
    {
        bbffr_skipTail(g_lqLTEM.iop->rxBffr, streams__contentLengthTrailerSz);
    }
    return resultCode__success;
}


/**
 * @brief Grab characters from RX (receive) buffer.
 * @warning GRAB is generally a diagnostic function note intended for general use. It changes the state of the RX
 * stream in potentially detrimental ways.
 * 
 * @param grabBffr Buffer to hold the grabbed characters.
 * @param grabBffrSz Specifies both the size of the buffer and the number of characters being requested.
 */
void ATCMD_GRABRX(char * grabBffr, uint8_t grabBffrSz)
{
    if (bbffr_getOccupied(g_lqLTEM.iop->rxBffr))
    {
        bbffr_pop(g_lqLTEM.iop->rxBffr, grabBffr, grabBffrSz);
    }
}

#pragma endregion // completion parsers/data handlers


#pragma region Static Function Definitions
/*-----------------------------------------------------------------------------------------------*/

/**
 * @brief Returns a token from the result of the last module command
 * @param [in] preamble Character phrase prefixing the section of the response to search
 * @param [in] tokenIndx The 0-based token index to return
 * @param [out] tokenBffr Char pointer to found token (will be returned null-terminated)
 * @param [in] tokenBffrSz Size of the application provided buffer to hold the returned token
 * @return true Token available and was returned in token to
 * @return false Input was empty or had fewer number of tokens than requested.
 */

static bool lqStr_getToken(const char * source, char delimiter, uint8_t tokenIndx, char * tokenBffr, uint8_t tokenBffrSz)
{
    ASSERT(tokenBffrSz >= 6);

    const char* delimPtr = source;                                                          // prepare for search
    memset(tokenBffr, 0, tokenBffrSz);                                                      // empty NULL terminated
    uint8_t tokenCnt = 0;

    char* endPtr = strchr(source, '\r');                                                    // end of array or EOL, only scan one line
    if (endPtr == NULL)
        endPtr = strchr(source, '\0');

    if (endPtr == source)                                                                   // empty source
        return false;

    do
    {
        delimPtr = strchr(source, delimiter);                                               // next delimiter
        delimPtr = (delimPtr == NULL) ? endPtr : delimPtr;                                  // no specified delim for last token, just end of source
        if (tokenCnt == tokenIndx)
        {
            strncpy(tokenBffr, source, MIN(delimPtr - source, tokenBffrSz));                // copy out vs. change response buffer, ensure no dest overflow
            return true;
        }
        source = delimPtr + 1;
        tokenCnt++;
    } while (source < endPtr);                                                              // while still input to scan
    return false;
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
//     //     lqLOG_INFO(PRNT_CYAN, "-p=sslURC");
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
//     // //     lqLOG_INFO(PRNT_CYAN, "-p=ipURC");
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
//     //     lqLOG_INFO(PRNT_CYAN, "-p=ipURC");
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
//     //     lqLOG_INFO(PRNT_CYAN, "-p=mqttR");
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
//     //     lqLOG_INFO(PRNT_CYAN, "-p=mqttS");
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
//     //     lqLOG_INFO(PRNT_CYAN, "-p=pdpD");
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
