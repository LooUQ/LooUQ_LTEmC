/** ***************************************************************************
  @file ltemc-atcmd.c
  @brief Modem command/response and data transfer functions.

  @author Greg Terrell, LooUQ Incorporated

  \loouq

  @warning Internal dependencies, changes only as directed by LooUQ staff.

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


#define LQ_SRCFILE "ATC"                        // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT

// #define ENABLE_DIAGPRINT                    // expand DPRINT into debug output
// #define ENABLE_DIAGPRINT_VERBOSE            // expand DPRINT and DPRINT_V into debug output
#define ENABLE_ASSERT

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "ltemc-iTypes.h"
#include "ltemc-nxp-sc16is.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))


/* Static Function Declarations
------------------------------------------------------------------------------------------------- */
static inline resultCode_t S__readResult();
static inline void S__restoreCmdDefaultOptions();
static inline void S__clearCmdOptions();
static void S__rxParseForUrc();
static void S__getToken(const char* preamble, uint8_t tokenIndx, char* token, uint8_t tkBffrLen);


#pragma region Public Functions
/*-----------------------------------------------------------------------------------------------*/

/**
 * @brief Resets atCmd struct and optionally releases lock, a BGx AT command structure.
 */
void ATCMD_reset(bool releaseLock)
{
    /* clearing req/resp buffers now for debug clarity, future likely just insert '\0' */

    // dataMode is not reset/cleared here, static S__resetDataMode is invoked after result

    // request side of action
    if (releaseLock)
        lqMutexGive(mutexTableIndex_atcmd);
        // g_lqLTEM.atcmd->isOpenLocked = false; // reset current lock

    memset(g_lqLTEM.atcmd->cmdStr, 0, atcmd__cmdBufferSz);
    memset(g_lqLTEM.atcmd->rawResponse, 0, atcmd__respBufferSz);
    memset(g_lqLTEM.atcmd->errorDetail, 0, ltemSz__errorDetailSz);
    g_lqLTEM.atcmd->resultCode = 0;
    g_lqLTEM.atcmd->execStart = 0;
    // g_lqLTEM.atcmd->retValue = 0;
    g_lqLTEM.atcmd->execDuration = 0;

    // command side
    g_lqLTEM.iop->txSrc = &g_lqLTEM.atcmd->cmdStr;                              // IOP pointer to current "talker"
    g_lqLTEM.iop->txPendingCnt = 0;

    // response side
    // g_lqLTEM.atcmd->response = g_lqLTEM.atcmd->rawResponse;                  // reset data component of response to full-response
}


/**
 * @brief Setup automatic data mode switch/servicing.
 */
void ATCMD_configDataMode(uint16_t contextKey, const char *trigger, dataHndlr_func dataHndlr, char *dataLoc, uint16_t dataSz, appGenRcvr_func appRecvCB, bool runParser)
{
    ASSERT(strlen(trigger) > 0);                                        // verify 3rd party setup (stream)
    ASSERT(dataHndlr != NULL);                                          // data handler is required, appRecvCB is optional

    memset(&g_lqLTEM.atcmd->dataMode, 0, sizeof(dataMode_t));

    g_lqLTEM.atcmd->dataMode.dmState = dmState_enabled;
    g_lqLTEM.atcmd->dataMode.contextKey = contextKey;
    memcpy(g_lqLTEM.atcmd->dataMode.trigger, trigger, strlen(trigger));
    g_lqLTEM.atcmd->dataMode.dataHndlr = dataHndlr;
    g_lqLTEM.atcmd->dataMode.txDataLoc = dataLoc;
    g_lqLTEM.atcmd->dataMode.txDataSz = dataSz;
    g_lqLTEM.atcmd->dataMode.appRecvCB = appRecvCB;
    g_lqLTEM.atcmd->dataMode.runParserAfterDataMode = runParser;
}


// void ATCMD_setDataModeEot(uint8_t eotChar)
// {
//     g_lqLTEM.iop->txEot = (char)eotChar;
// }

/**
 * @brief Sets command timeout for next invocation of a BGx AT command. 
 */
uint16_t ATCMD_ovrrdTimeout(uint16_t newTimeout)
{
    uint16_t oldTimeout = g_lqLTEM.atcmd->timeout;
    if (newTimeout > 0)
        g_lqLTEM.atcmd->timeout = newTimeout;
    return oldTimeout;
}


/**
 * @brief Sets response parser for next invocation of a BGx AT command. 
 */
cmdResponseParser_func ATCMD_ovrrdParser(cmdResponseParser_func newParser)
{
    cmdResponseParser_func oldParser = g_lqLTEM.atcmd->responseParserFunc;
    g_lqLTEM.atcmd->responseParserFunc = newParser;
    return oldParser;
}


void ATCMD_configureResponseParser(const char *preamble, bool preambleReqd, const char *delimeters, uint8_t tokensReqd, const char *finale, uint16_t lengthReqd)
{
    memset(&g_lqLTEM.atcmd->parserConfig, 0, sizeof(atcmdParserConfig_t));
    g_lqLTEM.atcmd->parserConfig.preamble = preamble;
    g_lqLTEM.atcmd->parserConfig.preambleReqd = preambleReqd;
    g_lqLTEM.atcmd->parserConfig.delimeters = delimeters;
    g_lqLTEM.atcmd->parserConfig.tokensReqd = tokensReqd;
    g_lqLTEM.atcmd->parserConfig.finale = finale;
    g_lqLTEM.atcmd->parserConfig.lengthReqd = lengthReqd;
}



/**
 * @brief Dispatch an AT command to module and await its completion.
 */
resultCode_t atcmd_dispatch(const char *cmdTemplate, ...)
{
    /* invoke phase
     ----------------------------------------- */
    // if (g_lqLTEM.atcmd->isOpenLocked)
    //     return resultCode__locked;

    // atcmd_reset(true);                                 // clear atCmd control
    // g_lqLTEM.atcmd->autoLock = atcmd__setLockModeAuto; // set automatic lock control mode

    // char *cmdStr = g_lqLTEM.atcmd->cmdStr;
    va_list ap;

    va_start(ap, cmdTemplate);
    vsnprintf(g_lqLTEM.atcmd->cmdStr, sizeof(g_lqLTEM.atcmd->cmdStr), cmdTemplate, ap);
    strcat(g_lqLTEM.atcmd->cmdStr, "\r");

    // if (!ATCMD_awaitLock(g_lqLTEM.atcmd->timeout)) // attempt to acquire new atCmd lock for this instance
    //     return false;

    // TEMPORARY SPI alters send buffer duplicates 
    memcpy(g_lqLTEM.atcmd->CMDMIRROR, g_lqLTEM.atcmd->cmdStr, strlen(g_lqLTEM.atcmd->cmdStr));

    DPRINT_V(0, "<atcmd_tryInvoke> cmd=%s\r\n", g_lqLTEM.atcmd->cmdStr);
    IOP_startTx(g_lqLTEM.atcmd->cmdStr, strlen(g_lqLTEM.atcmd->cmdStr));

    /* await result phase
     ----------------------------------------- */
    resultCode_t rslt = resultCode__unknown; // resultCode_t result;
    g_lqLTEM.atcmd->execStart = pMillis();
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
    g_lqLTEM.atcmd->timeout = atcmd__defaultTimeout;
    g_lqLTEM.atcmd->responseParserFunc = ATCMD_okResponseParser;

    return g_lqLTEM.atcmd->resultCode;
}




/**
 * @brief Invokes a BGx AT command using default option values (automatic locking).
 */
bool ATCMD_tryInvoke(const char *cmdTemplate, ...)
{
    if (g_lqLTEM.atcmd->ownerTaskKey > 0 &&                                     // LTEm owner priority exists
        g_lqLTEM.atcmd->ownerTaskKey != lqGetTaskHandle()                        // current task is not owner
        && !g_lqLTEM.atcmd->ownerLTEmBackground)                                // current task is not performing a LTEm background process
    {
        g_lqLTEM.atcmd->resultCode = resultCode__locked;
        return false;
    }
    if (!lqMutexTake(mutexTableIndex_atcmd, g_lqLTEM.atcmd->timeout))            // non-owner priority, command scope LTEm lock (exclusive through command complete)
    {
        g_lqLTEM.atcmd->resultCode = resultCode__locked;
        return false;
    }
    ATCMD_reset(true);                                                          // clear atCmd control

    va_list ap;
    va_start(ap, cmdTemplate);
    vsnprintf(g_lqLTEM.atcmd->cmdStr, sizeof(g_lqLTEM.atcmd->cmdStr), cmdTemplate, ap);
    strcat(g_lqLTEM.atcmd->cmdStr, "\r");

    if (lqMutexTake(mutexTableIndex_atcmd, g_lqLTEM.atcmd->timeout))             // attempt to acquire new atCmd lock for this instance
        return false;

    g_lqLTEM.atcmd->execStart = pMillis();

    // TEMPORARY
    memcpy(g_lqLTEM.atcmd->CMDMIRROR, g_lqLTEM.atcmd->cmdStr, strlen(g_lqLTEM.atcmd->cmdStr));
    DPRINT_V(0, "<ATCMD_tryInvoke> cmd=%s\r\n", g_lqLTEM.atcmd->cmdStr);

    IOP_startTx(g_lqLTEM.atcmd->cmdStr, strlen(g_lqLTEM.atcmd->cmdStr));
    g_lqLTEM.atcmd->resultCode = resultCode__invoked;
    return true;
}


/**
 * @brief Closes (completes) a BGx AT command structure and frees action resource (release action lock).
 */
void ATCMD_close()
{
    lqMutexGive(mutexTableIndex_atcmd);
    // g_lqLTEM.atcmd->isOpenLocked = false;
    g_lqLTEM.atcmd->execDuration = pMillis() - g_lqLTEM.atcmd->execStart;
}


/**
 * @brief Waits for atcmd result, periodically checking recv buffer for valid response until timeout.
 */
resultCode_t ATCMD_awaitResult()
{
    S__restoreCmdDefaultOptions();                                              // restore default options if not overriden
    g_lqLTEM.atcmd->resultCode = resultCode__unknown;                           // resultCode_t result;

    do
    {
        S__readResult();
        if (g_lqLTEM.cancellationRequest)                                       // test for cancellation (RTOS or IRQ)
        {
            g_lqLTEM.atcmd->resultCode = resultCode__cancelled;
            break;
        }
        pYield();                                                               // give back control momentarily before next loop pass
    } while (g_lqLTEM.atcmd->resultCode == resultCode__unknown);

    #if _DEBUG == 0                                                             // debug for debris in rxBffr
    ASSERT_W(bbffr_getOccupied(g_lqLTEM.iop->rxBffr) == 0, "RxBffr Dirty");
    #else
    if (bbffr_getOccupied(g_lqLTEM.iop->rxBffr) > 0)
    {
        char dbg[81] = {0};
        bbffr_pop(g_lqLTEM.iop->rxBffr, dbg, 80);
        DPRINT(PRNT_YELLOW, "*!* %s", dbg);
    }
    #endif

    // reset options to cleared state 
    S__clearCmdOptions();
    // g_lqLTEM.atcmd->timeout = atcmd__defaultTimeout;
    // g_lqLTEM.atcmd->responseParserFunc = ATCMD_okResponseParser;
    
    return g_lqLTEM.atcmd->resultCode;
}

/**
 * @brief Set task/thread owner for exclusive use of LTEm (public command actions)
 * 
 * @param ownerHandle Task/thread owning LTEm resources until owner cleared
 */
void ATCMD_setOwner(uint32_t ownerHandle)
{
    g_lqLTEM.atcmd->ownerTaskKey = ownerHandle;
    g_lqLTEM.atcmd->ownerLTEmBackground = false;
}


/**
 * @brief Get configured task/thread owner with exclusive use of the LTEm API
 * 
 * @return uint32_t Task/thread handle with ownership of LTEm resources
 */
uint32_t ATCMD_getOwner()
{
    return g_lqLTEM.atcmd->ownerTaskKey;
}


/**
 * @brief Clears configured task/thread owner with exclusive use of the LTEm API.
 * @note No warning or error is signalled if no owner currently configured.
 */
void ATCMD_clearOwner()
{
    g_lqLTEM.atcmd->ownerTaskKey = 0;
    g_lqLTEM.atcmd->ownerLTEmBackground = false;
}


/* ================================================================================================
 * Get command execution information
 =============================================================================================== */

// /**
//  * @brief Waits for atcmd result, periodically checking recv buffer for valid response until timeout.
//  */
// resultCode_t ATCMD_awaitResultWithOptions(uint32_t timeoutMS, cmdResponseParser_func cmdResponseParser)
// {
//     // set options
//     if (timeoutMS != atcmd__noTimeoutChange)
//     {
//         g_lqLTEM.atcmd->timeout = timeoutMS;
//     }
//     if (cmdResponseParser) // caller can use atcmd__useDefaultOKCompletionParser
//         g_lqLTEM.atcmd->responseParserFunc = cmdResponseParser;
//     else
//         g_lqLTEM.atcmd->responseParserFunc = ATCMD_okResponseParser;
//     return ATCMD_awaitResult();
// }


/**
 * @brief Returns the atCmd result code, 0xFFFF or cmdParseRslt_pending if command is pending completion
 */
const char* ATCMD_getCommand()
{
    return g_lqLTEM.atcmd->CMDMIRROR;
}


/**
 * @brief Returns the atCmd result code, 0xFFFF or cmdParseRslt_pending if command is pending completion
 */
resultCode_t ATCMD_getResultCode()
{
    return g_lqLTEM.atcmd->resultCode;
}


/**
 * @brief Returns the atCmd value response
 */
bool ATCMD_wasPreambleFound()
{
    return g_lqLTEM.atcmd->preambleFound;
}


/**
 * @brief Returns the string captured from the last command response; between pPreamble and pFinale (excludes both)
 */
const char* ATCMD_getRawResponse()
{
    ASSERT(g_lqLTEM.atcmd->rawResponse != NULL);
    return g_lqLTEM.atcmd->rawResponse;
}


/**
 * @brief Returns the string captured from the last command response with preamble removed.
 * @return Char pointer to the command response (note: this is stripped of preamble and finale strings)
 */
const char* ATCMD_getResponseData()
{
    ASSERT(g_lqLTEM.atcmd->rawResponse != NULL);

    char* delimPtr = memchr(g_lqLTEM.atcmd->rawResponse, ':', atcmd__respBufferSz);        // looking for ": "
    if (*(delimPtr + 1) == ' ')
        return delimPtr + 2;
    else
        return g_lqLTEM.atcmd->rawResponse;
}


// /**
//  * @brief Returns the atCmd value response
//  */
// int32_t ATCMD_getValue()
// {
//     return g_lqLTEM.atcmd->retValue;
// }


/**
 * @brief Returns a token from the result of the last module command
 */
char* ATCMD_getToken(uint8_t tokenIndx)
{
    S__getToken(": ", tokenIndx, g_lqLTEM.atcmd->respToken, atcmd__respTokenSz);        // returns empty c-str if not found
    return g_lqLTEM.atcmd->respToken;
}


/**
 * @brief Returns the atCmd last execution duration in milliseconds
 */
uint32_t ATCMD_getDuration()
{
    return g_lqLTEM.atcmd->execDuration;
}


/**
 * @brief Returns the atCmd parser result code, 0xFFFF or cmdParseRslt_pending if command is pending completion
 */
cmdParseRslt_t ATCMD_getParserResult()
{
    return g_lqLTEM.atcmd->parserResult;
}


/**
 * @brief Returns the BGx module error code or 0 if no error. Use this function to get details on a resultCode_t = 500
 */
const char * ATCMD_getErrorDetail()
{
    return g_lqLTEM.atcmd->errorDetail;
}


/**
 * @brief Returns the BGx reported CME/CMS error code as a numeric value.
 * @return Numeric CM* error code, 999 otherwise.
 */
uint16_t ATCMD_getErrorDetailCode()
{
    if (g_lqLTEM.atcmd->errorDetail[1] == 'C' && g_lqLTEM.atcmd->errorDetail[2] == 'M')
    {
        return strtol(g_lqLTEM.atcmd->errorDetail + 12, NULL, 10);
    }
    else
        return 999;
}


/**
 * @brief Sends ^Z character to ensure BGx is not in text mode.
 */
void ATCMD_exitTextMode()
{
    char ctrlZ[] = {0x1A};
    IOP_startTx(ctrlZ, sizeof(ctrlZ));
}


/**
 * @brief Sends break sequence to transition BGx out of fixed-size data mode to command mode (up to 1500 char).
 */
void ATCMD_exitDataMode()
{
}


/**
 * @brief Sends +++ sequence to transition BGx out of transparent data mode to command mode.
 */
void ATCMD_exitTransparentMode()
{
    pDelay(1000);
    IOP_startTx("+++", 3); // send +++, gaurded by 1 second of quiet
    pDelay(1000);
}

#pragma endregion



#pragma region LTEmC Internal Functions
/*-----------------------------------------------------------------------------------------------*/

// /**
//  * @brief Awaits exclusive access to QBG module command interface.
//  */
// bool ATCMD_awaitLock(uint16_t timeoutMS)
// {
//     uint32_t waitStart = pMillis();

//     do
//     {
//         if (!g_lqLTEM.atcmd->isOpenLocked) // if not existing lock
//         {
//             g_lqLTEM.atcmd->isOpenLocked = true; // ...acquired new lock
//             return true;
//         }
//         pYield();        // call back to platform yield() in case there is work there that can be done
//         ltem_eventMgr(); // process any new receives prior to starting cmd invoke

//     } while (pMillis() - waitStart < timeoutMS); // until timed out
//     return false;                                // timed out waiting for lock
// }

// /**
//  * @brief Returns the current atCmd lock state
//  */
// bool ATCMD_isLockActive()
// {
//     return lqMutexCount(mutexTableIndex_atcmd) == 0;
//     // return g_lqLTEM.atcmd->isOpenLocked;
// }

/**
 * @brief Checks receive buffer for command response and sets atcmd structure data with result.
 */
static resultCode_t S__readResult()
{
    g_lqLTEM.atcmd->parserResult = cmdParseRslt_pending;
    g_lqLTEM.atcmd->resultCode = 0;
    uint16_t peekedLen;

    ltem_eventMgr(); // check for URC events preceeding cmd response

    if (bbffr_getOccupied(g_lqLTEM.iop->rxBffr) > 0)
    {
        // chk for current command services a stream and there is a data mode handler registered
        if (g_lqLTEM.atcmd->dataMode.dataHndlr != NULL)
        {
            // looking for streamPrefix phrase
            if (BBFFR_ISFOUND(bbffr_find(g_lqLTEM.iop->rxBffr, g_lqLTEM.atcmd->dataMode.trigger, 0, 0, true)))
            {
                g_lqLTEM.atcmd->dataMode.dmState = dmState_active;
                g_lqLTEM.iop->dmActive = true;
                g_lqLTEM.iop->dmTxEvents = 0;

                resultCode_t dataRslt = (*g_lqLTEM.atcmd->dataMode.dataHndlr)();
                DPRINT(PRNT_MAGENTA, "DataHandler rslt=%d\r\n", dataRslt);
                if (dataRslt == resultCode__success)
                {
                    g_lqLTEM.atcmd->parserResult = cmdParseRslt_success;
                    g_lqLTEM.atcmd->resultCode = dataRslt;

                    if (g_lqLTEM.atcmd->dataMode.runParserAfterDataMode)
                        g_lqLTEM.atcmd->parserResult = cmdParseRslt_pending;
                }
                else
                {
                    g_lqLTEM.atcmd->parserResult = cmdParseRslt_error;
                    g_lqLTEM.atcmd->resultCode = dataRslt;
                }
                DPRINT(PRNT_WHITE, "Exit dataMode rslt=%d\r", dataRslt);
                g_lqLTEM.iop->dmActive = false;
                memset(&g_lqLTEM.atcmd->dataMode, 0, sizeof(dataMode_t));                   // done with dataMode settings
            }
        }

        uint8_t respLen = strlen(g_lqLTEM.atcmd->rawResponse);                              // response so far
        uint8_t popSz = MIN(atcmd__respBufferSz - respLen, bbffr_getOccupied(g_lqLTEM.iop->rxBffr));
        ASSERT((respLen + popSz) < atcmd__respBufferSz);                                    // ensure don't overflow

        if (g_lqLTEM.atcmd->parserResult == cmdParseRslt_pending && 
           (g_lqLTEM.atcmd->dataMode.dmState == dmState_idle || g_lqLTEM.atcmd->dataMode.runParserAfterDataMode))
        {
            bbffr_pop(g_lqLTEM.iop->rxBffr, g_lqLTEM.atcmd->rawResponse + respLen, popSz);  // pop new into response buffer for parsing

            g_lqLTEM.atcmd->parserResult = (*g_lqLTEM.atcmd->responseParserFunc)();         /* *** parse for command response *** */

            DPRINT_V(PRNT_GRAY, "prsr=%d \r", g_lqLTEM.atcmd->parserResult);
        }
    }

    if (g_lqLTEM.atcmd->parserResult & cmdParseRslt_error)                         // check error bit
    {
        if (g_lqLTEM.atcmd->parserResult & cmdParseRslt_moduleError)               // BGx ERROR or CME/CMS
            g_lqLTEM.atcmd->resultCode = resultCode__extendedCodesBase;

        else if (g_lqLTEM.atcmd->parserResult & cmdParseRslt_countShort)           // did not find expected tokens
            g_lqLTEM.atcmd->resultCode = resultCode__notFound;

        else
            g_lqLTEM.atcmd->resultCode = resultCode__internalError;                // covering the unknown

        ATCMD_close();                                                             // close action to release action lock on any error
    }

    if (g_lqLTEM.atcmd->parserResult == cmdParseRslt_pending)                      // still pending, check for timeout error
    {
        if (IS_ELAPSED(g_lqLTEM.atcmd->execStart, g_lqLTEM.atcmd->timeout))
        {
            g_lqLTEM.atcmd->resultCode = resultCode__timeout;
            // g_lqLTEM.atcmd->isOpenLocked = false;                                   // close action to release action lock
            // g_lqLTEM.atcmd->execDuration = pMillis() - g_lqLTEM.atcmd->invokedAt;
            ATCMD_close();

            if (ltem_getDeviceState() != deviceState_ready)                           // if action timed-out, verify not a device wide failure
            {
                ltem_notifyApp(appEvent_fault_hardLogic, "LTEm Module Not Ready");
                ltem_notifyApp(appEvent_fault_hardLogic, g_lqLTEM.atcmd->CMDMIRROR);
            }
            else if (!SC16IS7xx_ping())
                ltem_notifyApp(appEvent_fault_softLogic, "LTEm SPI Fault");         // UART bridge SPI not initialized correctly, IRQ not enabled

            return resultCode__timeout;
        }
        return resultCode__unknown;
    }

    if (g_lqLTEM.atcmd->parserResult & cmdParseRslt_success)                        // success bit: parser completed with success (may have excessRecv warning)
    {
        lqMutexGive(mutexTableIndex_atcmd);
        // if (g_lqLTEM.atcmd->autoLock)                                               // if the individual cmd is controlling lock state
        //     g_lqLTEM.atcmd->isOpenLocked = false;                                   // equivalent to ATCMD_close()
        g_lqLTEM.atcmd->execDuration = pMillis() - g_lqLTEM.atcmd->execStart;
        g_lqLTEM.atcmd->resultCode = resultCode__success;
        g_lqLTEM.metrics.cmdInvokeCnt++;
    }
    return g_lqLTEM.atcmd->resultCode;
}

/**
 * @brief Reset command defaults for timeout and response parser. Performed after invoke and prior to read response.
 */
static void S__clearCmdOptions()
{
    g_lqLTEM.atcmd->timeout = 0;
    g_lqLTEM.atcmd->responseParserFunc = NULL;
}


static void S__restoreCmdDefaultOptions()
{
    if (g_lqLTEM.atcmd->timeout > 0)                                    // set default timeout if not overriden
        g_lqLTEM.atcmd->timeout = atcmd__defaultTimeout;

    if (g_lqLTEM.atcmd->responseParserFunc == NULL)                     // set default parser if not overriden
        g_lqLTEM.atcmd->responseParserFunc = ATCMD_okResponseParser;
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
 * @brief Validate the response ends in a BGxx OK value.
 */
cmdParseRslt_t ATCMD_okResponseParser()
{
    return ATCMD_stdResponseParser("", false, "", 0, 0, "OK\r\n", 0);
}


void ATCMD_setOkResponseParser()
{
    ATCMD_configureResponseParser("", false, "", 0, "OK\r\n", 0);
}


/**
 * @brief Stardard TX (out) data handler used by dataMode.
 */
resultCode_t ATCMD_txOkDataHndlr()
{
    IOP_startTx(g_lqLTEM.atcmd->dataMode.txDataLoc, g_lqLTEM.atcmd->dataMode.txDataSz);

    g_lqLTEM.atcmd->dataMode.dmState = dmState_active;

    uint32_t startTime = pMillis();
    while (pMillis() - startTime < g_lqLTEM.atcmd->timeout)
    {
        uint16_t trlrIndx = bbffr_find(g_lqLTEM.iop->rxBffr, "OK", 0, 0, true);
        if (BBFFR_ISFOUND(trlrIndx))
        {
            bbffr_skipTail(g_lqLTEM.iop->rxBffr, OK_COMPLETED_LENGTH);          // OK + line-end
            return resultCode__success;
        }
        pDelay(1);
    }
    return resultCode__timeout;
}


cmdParseRslt_t ATCMD_responseParser()
{
    ASSERT(g_lqLTEM.atcmd->parserConfig.delimeters != NULL && strlen(g_lqLTEM.atcmd->parserConfig.delimeters) > 0);

    const char * preamble = g_lqLTEM.atcmd->parserConfig.preamble;
    bool preambleReqd = g_lqLTEM.atcmd->parserConfig.preambleReqd;
    const char * delimeters = g_lqLTEM.atcmd->parserConfig.delimeters;
    uint8_t tokensReqd = g_lqLTEM.atcmd->parserConfig.tokensReqd;
    const char * finale = g_lqLTEM.atcmd->parserConfig.finale;
    uint16_t lengthReqd = g_lqLTEM.atcmd->parserConfig.lengthReqd;

    // valueIndx is deprecated, removal progressing; 0 will be remove soon
    return ATCMD_stdResponseParser(preamble, preambleReqd, delimeters, tokensReqd, 0, finale, lengthReqd);
}


/**
 * @brief Stardard atCmd response parser, flexible response pattern match and parse.
 */
cmdParseRslt_t ATCMD_stdResponseParser(const char *pPreamble, bool preambleReqd, const char *pDelimeters, uint8_t tokensReqd, uint8_t valueIndx, const char *pFinale, uint16_t lengthReqd)
{
    cmdParseRslt_t parseRslt = cmdParseRslt_pending;

    ASSERT(!(preambleReqd && STREMPTY(pPreamble)));                             // if preamble required, cannot be empty
    ASSERT(pPreamble != NULL && pDelimeters != NULL && pFinale != NULL);        // char params are not NULL, must be valid empty char arrays
    ASSERT((!tokensReqd || strlen(pDelimeters) > 0));                           // if tokens count or value return, need delimiter

    uint8_t preambleLen = strlen(pPreamble);
    uint8_t reqdPreambleLen = preambleReqd ? preambleLen : 0;
    uint8_t finaleLen = strlen(pFinale);
    char* workPtr = g_lqLTEM.atcmd->rawResponse;                                // easier to work with pointer
    uint16_t responseLen = strlen(workPtr);

    // always look for error, short-circuit result if CME/CMS
    char *pErrorLoctn;
    if ((pErrorLoctn = strstr(workPtr, "+CM")) || (pErrorLoctn = strstr(workPtr, "ERROR")))
    {
        for (size_t i = 0; i < ltemSz__errorDetailSz; i++)                        // copy raw chars: unknown incoming format, stop at line end
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
     * Search response for preamble, finale, token count (tokensReqd)
     */

    while (workPtr[0] == '\r' || workPtr[0] == '\n')
    {
        workPtr + 1;                                             // skip past prefixing line terminators
    }

    bool preambleSatisfied = false;
    if (preambleLen) // if pPreamble provided
    {
        char *pPreambleLoctn = strstr(workPtr, pPreamble);      // find it in response
        if (pPreambleLoctn)
        {
            preambleSatisfied = true;
            g_lqLTEM.atcmd->preambleFound = true;

            workPtr = pPreambleLoctn + preambleLen;             // remove pPreamble from retResponse
        }
        else if (preambleReqd)
        {
            if (responseLen >= preambleLen)
                return cmdParseRslt_error | cmdParseRslt_preambleMissing;           // require preamble missing, response is sufficient for match

            return cmdParseRslt_pending;                                            // keep waiting on response
        }
        else  // !preambleReqd
        {
            if (strstr(workPtr, pFinale))
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
            pFinaleLoctn = strstr(workPtr, pFinale);
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
    bool tokenCntSatified = !tokensReqd;
    if (finaleSatisfied && !tokenCntSatified)                                   // count tokens to service value return or validate required token count
    {
        tokenCnt = 1;
        char *pDelimeterAt;
        char *pTokenAt = workPtr;
        do
        {
            pDelimeterAt = strpbrk(pTokenAt, pDelimeters);                      // look for delimeter/next token
            if (tokenCnt >= tokensReqd)                                         // at/past required token = done
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
            parseRslt |= cmdParseRslt_error | cmdParseRslt_countShort;          // set error and count short bits
    }

    if (!(parseRslt & cmdParseRslt_error) &&                                    // check error bit
        preambleSatisfied &&
        finaleSatisfied &&
        tokenCntSatified)                                                       // and all component test conditions
    {
        parseRslt |= cmdParseRslt_success;                                      // no error, preserve possible warnings (excess recv, etc.)
    }
    return parseRslt; // done
}

#pragma endregion // completionParsers

#pragma region Static Function Definitions
/*-----------------------------------------------------------------------------------------------*/

/**
 * @brief Returns a token from the result of the last module command
 * @param [in] preamble Character phrase prefixing the section of the response to search
 * @param [in] tokenIndx The 0-based token index to return
 * @param [out] token Char pointer to found token (will be returned null-terminated)
 * @param [in] tkBffrLen Size of the application provided buffer to hold the returned token
 */
static void S__getToken(const char* preamble, uint8_t tokenIndx, char* token, uint8_t tkBffrLen)
{
    uint8_t preambleLen = strlen(preamble);
    uint16_t responseLen = strlen(g_lqLTEM.atcmd->rawResponse);
    memset(token, 0, tkBffrLen);                                                            // ensure token is NULL-terminated

    if (responseLen <= preambleLen)                                                         // nothing between preamble and term
        return;

    char* searchPtr = g_lqLTEM.atcmd->rawResponse;                                          // need to leave atcmd internals intact
    if (preambleLen > 0)                                                                    // adjust start of search to past preamble
    {
        for (size_t i = 0; i < atcmd__respBufferSz - preambleLen; i++)
        {
            if (memcmp(searchPtr, preamble, preambleLen) == 0)
                break;
            searchPtr++;
        }
    }
    searchPtr += preambleLen;
    for (size_t i = 0; i <= tokenIndx; i++)
    {   
        uint16_t bffrRemaining = g_lqLTEM.atcmd->rawResponse + atcmd__respBufferSz - searchPtr;
        if (!bffrRemaining)
            return;

        const char delims[] = { ",\rO\0"};
        char* tkEnd;
        for (size_t d = 0; d < 4; d++)
        {
            tkEnd = memchr(searchPtr, delims[d], bffrRemaining);
            if (tkEnd)
                break;
        }

        // char* tkEnd = memchr(searchPtr, ',', bffrRemaining);
        // tkEnd = (tkEnd) ? tkEnd : memchr(searchPtr, '\r', bffrRemaining);                   // if delimeter not found, end of token (aka delim) is 'O'=(OK), \r=(\r\n), or \0
        // tkEnd = (tkEnd) ? tkEnd : memchr(searchPtr, 'O', bffrRemaining);
        // tkEnd = (tkEnd) ? tkEnd : memchr(searchPtr, '\0', bffrRemaining);                   // if delimeter not found, end of token (aka delim) is 'O' (OK), \r,\n, or \0

        if (i == tokenIndx && tkEnd > searchPtr)                                                // is this the token we want
        {
            memcpy(token, searchPtr, MIN(tkBffrLen, tkEnd - searchPtr));                        // copy, leaving room for '\0'
            return;
        }
        searchPtr = tkEnd + 1;                                                                  // next token
    }
}

#pragma endregion


/* --------------------------------------------------------------------------------------------- */
#pragma region Stream Registration 
/* --------------------------------------------------------------------------------------------- */

void STREAM_register(streamCtrl_t *streamCtrl)
{
    DPRINT_V(PRNT_INFO, "Registering Stream\r\n");
    streamCtrl_t* prev = ltem_findStreamFromCntxt(streamCtrl->dataCntxt, streamType__ANY);

    if (prev != NULL)
        return;

    for (size_t i = 0; i < ltemSz__streamCnt; i++)
    {
        if (g_lqLTEM.streams[i] == NULL)
        {
            g_lqLTEM.streams[i] = streamCtrl;
            switch (streamCtrl->streamType)
            {
                // case streamType_file:                                    // file module has no URC events
                // case streamType_HTTP:                                    // HTTP module has no URC events

                case streamType_MQTT:
                    // g_lqLTEM.streams[i]->urcHndlr = MQTT_urcHandler;
                    break;

                case streamType_SCKT:
                    // g_lqLTEM.streams[i]->urcHndlr = SCKT_urcHandler;
                    break;
            }
            return;
        }
    }
}


void STREAM_deregister(streamCtrl_t *streamCtrl)
{
    for (size_t i = 0; i < ltemSz__streamCnt; i++)
    {
        if (g_lqLTEM.streams[i]->dataCntxt == streamCtrl->dataCntxt)
        {
            ASSERT(memcmp(g_lqLTEM.streams[i], streamCtrl, sizeof(streamCtrl_t)) == 0);     // compare the common fields
            g_lqLTEM.streams[i] = NULL;
            return;
        }
    }
}


streamCtrl_t* STREAM_find(uint8_t dataCntxt, streamType_t streamType)
{
    for (size_t i = 0; i < ltemSz__streamCnt; i++)
    {
        if (g_lqLTEM.streams[i] != NULL && g_lqLTEM.streams[i]->dataCntxt == dataCntxt)
        {
            if (streamType == streamType__ANY)
            {
                return g_lqLTEM.streams[i];
            }
            else if (g_lqLTEM.streams[i]->streamType == streamType)
            {
                return g_lqLTEM.streams[i];
            }
            else if (streamType == streamType_SCKT)
            {
                if (g_lqLTEM.streams[i]->streamType == streamType_UDP ||
                    g_lqLTEM.streams[i]->streamType == streamType_TCP ||
                    g_lqLTEM.streams[i]->streamType == streamType_SSLTLS)
                {
                    return g_lqLTEM.streams[i];
                }
            }
        }
    }
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */
#pragma endregion
/* --------------------------------------------------------------------------------------------- */

#pragma region OBSOLETE_MAYBE
// /**
//  * @brief Parse recv'd data (in command RX buffer) for async event preambles that need to be handled immediately.
//  * @details AT cmd uses this to examine new buffer contents for +URC events that may arrive in command response
//  *           Declared in ltemc-internal.h
//  */
// void S__parseForUrcReceived(const char* urcTarget)
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