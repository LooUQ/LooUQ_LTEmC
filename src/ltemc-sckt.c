/** ***************************************************************************
  @file 
  @brief Modem socket (UDP/TCP) communication functions/services.

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


#define SRCFILE "SKT"                       // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT
//#define ENABLE_DIAGPRINT                    // expand DPRINT into debug output
//#define ENABLE_DIAGPRINT_VERBOSE            // expand DPRINT and DPRINT_V into debug output
#define ENABLE_ASSERT
#include <lqdiag.h>

#include "ltemc-sckt.h"
// #include "ltemc-atcmd.h"


extern ltemDevice_t g_lqLTEM;


#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) < (y)) ? (y) : (x))

#define DETECT_STALL(tick, threshold)  if (pMillis() - tick > threshold) return resultCode__timeout
#define ASSERT_NOTSTALLED(tick, threshold)  ASSERT(pMillis() - tick > threshold)



// file scope local function declarations
static resultCode_t S__scktTxDataHndlr();
static resultCode_t S__scktRxHndlr();

static cmdParseRslt_t S__irdResponseHeaderParser();
static cmdParseRslt_t S__sslrecvResponseHeaderParser();
static cmdParseRslt_t S__udptcpOpenCompleteParser(const char *response, char **endptr);
static cmdParseRslt_t S__sslOpenCompleteParser(const char *response, char **endptr);
static cmdParseRslt_t S__socketSendCompleteParser(const char *response, char **endptr);
static cmdParseRslt_t S__socketStatusParser(const char *response, char **endptr);



/* FIX: make local 
 * ------------------------------------------------------------------------------------------------------------------------------*/
void SCKT_urcHandler();                                                 // parse/handle SCKT async URC events
// HTTP and filesystem run in a synchronous manner, they do not have async URC events (as implemented)






#pragma region public sockets (IP:TCP/UDP/SSL) functions
/* --------------------------------------------------------------------------------------------- */


/**
 *	@brief Create a socket data control(TCP/UDP/SSL).
 */
void sckt_initControl(scktCtrl_t *scktCtrl, dataCntxt_t dataCntxt, streamType_t protocol, appRecv_func recvCallback)
{
    ASSERT(dataCntxt < dataCntxt__cnt);

    memset(scktCtrl, 0, sizeof(scktCtrl_t));

    scktCtrl->dataCntxt = dataCntxt;
    scktCtrl->streamType = (char)protocol;
    scktCtrl->useTls = protocol == streamType_SSLTLS;
    scktCtrl->irdPending = false;
    scktCtrl->flushing = false;
    scktCtrl->statsRxCnt = 0;
    scktCtrl->statsTxCnt = 0;
    scktCtrl->appRecvDataCB = recvCallback;

    g_lqLTEM.streams[dataCntxt] = (streamCtrl_t*)scktCtrl;
}


/**
 *	@brief Set connection parameters for a socket connection (TCP/UDP)
 */
void sckt_setConnection(scktCtrl_t *scktCtrl, uint8_t pdpCntxt, const char *hostUrl, uint16_t hostPort, uint16_t lclPort)
{
    strncpy(scktCtrl->hostUrl, hostUrl, sckt__urlHostSz);
    scktCtrl->pdpCntxt = pdpCntxt;
    scktCtrl->hostPort = hostPort;
    scktCtrl->lclPort = lclPort;
}


/**
 *	@brief Open a data connection (socket) to d data to an established endpoint via protocol used to open socket (TCP/UDP/TCP INCOMING).
 */
resultCode_t sckt_open(scktCtrl_t *scktCtrl, bool cleanSession)
{
    uint8_t pdpCntxt = (scktCtrl->pdpCntxt == 0) ? g_lqLTEM.ntwkOperator->defaultContext : scktCtrl->pdpCntxt;
    resultCode_t rslt;

    if (scktCtrl->streamType == 'U')                    // protocol == UDP
    {
        ATCMD_tryInvoke("AT+QIOPEN=%d,%d,\"UDP\",\"%s\",%d,%d", pdpCntxt, scktCtrl->dataCntxt, scktCtrl->hostUrl, scktCtrl->hostPort, scktCtrl->lclPort);
        rslt = ATCMD_awaitResultWithOptions(sckt__defaultOpenTimeoutMS, S__udptcpOpenCompleteParser);
    }

    else if (scktCtrl->streamType == 'T')               // protocol == TCP
    {
        ATCMD_tryInvoke("AT+QIOPEN=%d,%d,\"TCP\",\"%s\",%d,%d", pdpCntxt, scktCtrl->dataCntxt, scktCtrl->hostUrl, scktCtrl->hostPort, scktCtrl->lclPort);
        rslt = ATCMD_awaitResultWithOptions(sckt__defaultOpenTimeoutMS, S__udptcpOpenCompleteParser);
    }

    else if (scktCtrl->streamType == 'S')               // protocol == SSL/TLS
    {
        ATCMD_tryInvoke("AT+QSSLOPEN=%d,%d,\"SSL\",\"%s\",%d,%d", pdpCntxt, scktCtrl->dataCntxt, scktCtrl->hostUrl, scktCtrl->hostPort, scktCtrl->lclPort);
        rslt = ATCMD_awaitResultWithOptions(sckt__defaultOpenTimeoutMS, S__sslOpenCompleteParser);
    }

    if (rslt == resultCode__success)
    {
        ltem_addStream(scktCtrl);
    }
    return rslt;
}



/**
 *	@brief Close an established (open) connection socket
 */
void sckt_close(scktCtrl_t *scktCtrl)
{
    if (scktCtrl->state == scktState_closed)                                    // not open
        return;

    if (scktCtrl->useTls)
        ATCMD_tryInvokeDefaults("AT+QSSLCLOSE=%d", scktCtrl->dataCntxt);        // BGx syntax different for SSL
    else
        ATCMD_tryInvokeDefaults("AT+QICLOSE=%d", scktCtrl->dataCntxt);          // BGx syntax different for TCP/UDP
    
    if (ATCMD_awaitResult() == resultCode__success)
    {
        scktCtrl->state = scktState_closed;
        ltem_deleteStream(scktCtrl);
    }
}


/**
 *	@brief Close an established (open) connection socket by context number.
 *  @details This is provided for LTEm use in the case of a connection close/loss.
 */
void SCKT_closeCntxt(uint8_t cntxtNm)
{
    streamCtrl_t* streamCtrl = ltem_getStreamFromCntxt(cntxtNm, streamType_SCKT);
    ASSERT(streamCtrl);

    sckt_close(streamCtrl);
}


/**
 *	@brief Reset open socket connection. This function drains the connection's data pipeline. 
 *
 *  NOT IMPLEMENTED
 */
bool sckt_flush(scktCtrl_t *scktCtrl)
{
    if (scktCtrl->state == scktState_open)
    {
    // if (s_requestIrdData(scktCtrl->dataCntxt, scktCtrl->recvBufCtrl._bufferSz, true))      // initiate an IRD flow
    // {
    //     scktCtrl->flushing = true;
         return true;
    // }

    }

    return false;                                                                       // unable to obtain action lock
}


/**
 *	@brief Retrieve the state of a socket connection
 */
bool sckt_getState(scktCtrl_t *scktCtrl)
{
    return scktCtrl->state;
}


/**
 *	@brief Send data to an established endpoint via protocol used to open socket (TCP/UDP/TCP INCOMING).
 */
resultCode_t sckt_send(scktCtrl_t *scktCtrl, const char *data, uint16_t dataSz)
{
    resultCode_t rslt;

    ATCMD_configDataMode(scktCtrl->dataCntxt, "> ", ATCMD_txOkDataHndlr, data, dataSz, NULL, false);
    ATCMD_configDataModeEot(0x1A);

    if (ATCMD_tryInvoke("AT+QISEND=%d,%d", scktCtrl->dataCntxt, dataSz))
    {
        ATCMD_ovrrdParser(S__socketSendCompleteParser);
        rslt = ATCMD_awaitResult();
        if (rslt == resultCode__success)
        {
            scktCtrl->statsTxCnt++;
        }
    }
    ATCMD_close();
    return rslt;                                                            // return sucess -OR- failure from sendRequest\sendRaw action
}


// static resultCode_t S__scktTxDataHndlr()
// {
//     IOP_startTx(g_lqLTEM.atcmd->dataMode.txDataLoc, g_lqLTEM.atcmd->dataMode.txDataSz);

//     uint32_t startTime = pMillis();

//     while (pMillis() - startTime < g_lqLTEM.atcmd->timeout)
//     {
//         if (BBFFR_FOUND(bbffr_find(g_lqLTEM.iop->rxBffr, "SEND OK", 0, 0, true)))
//         {
//             bbffr_skipTail(g_lqLTEM.iop->rxBffr, sizeof("SEND OK") + 1);
//             return resultCode__success;
//         }
//         else if(BBFFR_FOUND(bbffr_find(g_lqLTEM.iop->rxBffr, "SEND FAIL", 0, 0, true)))
//         {
//             bbffr_skipTail(g_lqLTEM.iop->rxBffr, sizeof("SEND FAIL") + 1);
//             return resultCode__tooManyRequests;
//         }
//         else if(BBFFR_FOUND(bbffr_find(g_lqLTEM.iop->rxBffr, "ERROR", 0, 0, true)))
//         {
//             bbffr_skipTail(g_lqLTEM.iop->rxBffr, sizeof("SEND FAIL") + 1);
//             return resultCode__internalError;
//         }

//         pDelay(1);
//     }
//     return resultCode__timeout;
// }


#pragma region private local static functions
/*-----------------------------------------------------------------------------------------------*/

#define SCKT_URC_HEADERSZ 30

/**
 *   @brief Move socket data through pipeline.
 * 
*/

    /*
     * +QIURC: "recv",<connectID>       UDP/TCP incoming receive to retrieve with AT+QIRD
     * +QIURC: "closed",<connectID>
     * +QIURC: "incoming full"          NOT IMPLEMENTED
     *
     * +QSSLURC: "recv",<clientID>      SSL/TLS incoming receive to retrieve with AT+QSSLRECV
     * +QSSLURC: "closed",<clientID>

     * NOTE:
     * +QIURC: "pdpdeact",<contextID>   // not handled here, falls through to global URC handler
    */

void SCKT_urcHandler()
{
    bBuffer_t *rxBffr = g_lqLTEM.iop->rxBffr;                           // for convenience

    // not a socket URC or insufficient chars to parse URC header
    if (bbffr_find(rxBffr, "\"pdpdeact\"", 0, 0, false) >= 0)           // +QIURC: "pdpdeact" handled at higher level, +QIURC overlaps with UDP/TCP
    {
        return;
    }

    bool isUdpTcp = BBFFR_FOUND(bbffr_find(rxBffr, "+QIURC", 0, 0, false));
    bool isSslTls = BBFFR_FOUND(bbffr_find(rxBffr, "+QSSLURC", 0, 0, false));
    if (!isUdpTcp && !isSslTls)
    {
        return;
    }

    /* UDP/TCP/SSL/TLS URC
     * ----------------------------------------------------------------------------------------- */

    char workBffr[80] = {0};
    char *workPtr = workBffr;

    int16_t nextIndx = bbffr_find(rxBffr, "+QIURC", 0, 0, true);            // advance bffr-tail ptr to starting point
    if (isUdpTcp) 
    {
        bbffr_skip(rxBffr, 9);                                              // UDP/TCP: + QIURC: "
    }
    else 
    {
        bbffr_skip(rxBffr, 11);                                             // SSL/TLS: +QSSLURC: "
    }
    uint16_t eolIndx = bbffr_find(rxBffr, "\r\n", 0, 30, false);
    if (BBFFR_FOUND(eolIndx))                                               // got full line, work on URC
    {
        bbffr_skipTail(rxBffr, 9);                                          // ignore prefix
        bbffr_pop(rxBffr, workBffr, eolIndx - 9);
    }
    else
    {
        return;                                                             // don't have full URC line yet, come back later
    }
    
    /* URC ready to process
     ----------------------------------------------------------------------- */
    uint8_t dataCntxt;

    uint16_t irdAvailSz = 0;
    // "recv" = socket new data receive
    if (workBffr[0] == 'r')
    {
        dataCntxt = strtol(workPtr + sizeof("recv\""), NULL, 10);           // valid for both UDP/TCP and SSL
        ASSERT(dataCntxt < dataCntxt__cnt);

        streamCtrl_t* streamCtrl = ltem_getStreamFromCntxt(dataCntxt, streamType__ANY);
        ASSERT(streamCtrl->streamType == streamType_UDP ||
               streamCtrl->streamType == streamType_TCP ||
               streamCtrl->streamType == streamType_SSLTLS);
        scktCtrl_t* scktCtrl = (scktCtrl_t*)streamCtrl;

        uint16_t irdRemain = 0;
        do
        {
            uint16_t irdRqstSz = bbffr_getVacant(g_lqLTEM.iop->rxBffr) / 2;     // request up to half of available buffer space
            if (isUdpTcp)
            {
                ATCMD_configDataMode(scktCtrl->dataCntxt, "+QIRD: ", S__scktRxHndlr, NULL, 0, scktCtrl->appRecvDataCB, false);
                ATCMD_tryInvoke("AT+QIRD=%d,%d", (uint8_t)dataCntxt, irdRqstSz);
            }
            else
            {
                ATCMD_configDataMode(scktCtrl->dataCntxt, "+QSSLRECV: ", S__scktRxHndlr, NULL, 0, scktCtrl->appRecvDataCB, false);
                ATCMD_tryInvoke("AT+QSSLRECV=%d,%d", (uint8_t)dataCntxt, irdRqstSz);
            }
            ATCMD_awaitResult();

            const char* token = ATCMD_getToken(1);
            irdRemain = strtol(token, NULL, 10);

            // irdRemain = ATCMD_getValue();

        } while (irdRemain > 0);
    }

    // "closed" = socket closed
    if (workBffr[0] == 'c')                                                
    {
        dataCntxt = strtol(workPtr + sizeof("closed\"") , NULL, 10);
        ASSERT(dataCntxt < dataCntxt__cnt);

        uint8_t indx = LTEM__getStreamIndx(dataCntxt);
        ((scktCtrl_t*)g_lqLTEM.streams[indx])->state = scktState_closed;
    }

    return;
}    


/**
 * @brief Socket protocol (UDP/TCP/SSL) stream RX data handler, marshalls incoming data from RX buffer to app (application).
 */
static resultCode_t S__scktRxHndlr()
{
    /* +QIRD: <read_actual_length>/r/n<data>
     * +QSSLRECV: <havereadlen>/r/n<data>
     */

    char wrkBffr[32] = {0};
    char *wrkPtr = wrkBffr;
    streamCtrl_t* streamCtrl = ltem_getStreamFromCntxt(g_lqLTEM.atcmd->dataMode.contextKey, streamType__ANY);

    ASSERT(streamCtrl->streamType == streamType_UDP ||                                                          // assert that the stream config is consistent
           streamCtrl->streamType == streamType_TCP || 
           streamCtrl->streamType == streamType_SSLTLS);
    scktCtrl_t *scktCtrl = (scktCtrl_t*)streamCtrl;
    
    pDelay(1);                                                                                                  // ugly, but creating loop to wait 500uS seems silly
    uint8_t popCnt = bbffr_find(g_lqLTEM.iop->rxBffr, "\r", 0, 0, false);
    if (BBFFR_ISNOTFOUND(popCnt))
    {
        return resultCode__internalError;
    }
    
    bbffr_pop(g_lqLTEM.iop->rxBffr, wrkBffr, popCnt + 2);                                                       // pop preamble phrase to parse data length
    wrkPtr = memchr(wrkBffr, ':', popCnt) + 2;
    uint16_t irdSz = strtol(wrkPtr, NULL, 10);







//    g_lqLTEM.atcmd->retValue = irdSz;









    DPRINT(PRNT_CYAN, "scktRxHndlr() cntxt=%d irdSz=%d\r", scktCtrl->dataCntxt, irdSz);

    while (irdSz > 0)
    {
        uint32_t readTimeout = pMillis();
        uint16_t bffrCnt;
        do                                                                                                      // wait for buffer to recv IRD data
        {
            bffrCnt = bbffr_getOccupied(g_lqLTEM.iop->rxBffr);
            ASSERT_NOTSTALLED(readTimeout, sckt__readTimeoutMs);
        } while (bffrCnt < sckt__irdRequestPageSz);
        
        char* streamPtr;
        uint16_t blockSz = bbffr_popBlock(g_lqLTEM.iop->rxBffr, &streamPtr, irdSz);                             // get data ptr from rxBffr
        DPRINT(PRNT_CYAN, "scktRxHndlr() ptr=%p, blkSz=%d, availSz=%d\r", streamPtr, blockSz, irdSz);

        irdSz -= blockSz;
        ((scktAppRecv_func)(*scktCtrl->appRecvDataCB))(scktCtrl->dataCntxt, streamPtr, blockSz, irdSz == 0);    // forward to application
        bbffr_popBlockFinalize(g_lqLTEM.iop->rxBffr, true);                                                     // commit POP

        if (irdSz == 0)                                                                                         // done with data
        {
            while (bbffr_getOccupied(g_lqLTEM.iop->rxBffr) < sckt__readTrailerSz)
            {
                pDelay(1);                                                                                      // yield
                ASSERT_NOTSTALLED(readTimeout, sckt__readTimeoutMs);
            }
            bbffr_skipTail(g_lqLTEM.iop->rxBffr, sckt__readTrailerSz);
        }
    }
    return resultCode__success;
}




#pragma endregion


#pragma region private service functions and UDP/TCP/SSL response parsers
/*-----------------------------------------------------------------------------------------------*/

/**
 *	\brief [private] UDP/TCP (IRD Request) response parser.
 *  \return LTEmC parse result
 */
static cmdParseRslt_t S__irdResponseHeaderParser() 
{
    return ATCMD_stdResponseParser("+QIRD: ", true, ",", 1, 1, "\r\n", 0);
}


/**
 *	\brief [private] UDP/TCP (IRD Request) response parser.
 *  \return LTEmC parse result
 */
static cmdParseRslt_t S__sslrecvResponseHeaderParser() 
{
    return ATCMD_stdResponseParser("+QSSLRECV: ", true, ",", 1, 1, "\r\n", 0);
}


/**
 *	@brief [private] TCP/UDP wrapper for open connection parser.
 */
static cmdParseRslt_t S__udptcpOpenCompleteParser(const char *response, char **endptr) 
{
    return ATCMD_stdResponseParser("+QIOPEN: ", true, ",", 1, 1, "", 0);
}


/**
 *	@brief [private] SSL wrapper for open connection parser.
 */
static cmdParseRslt_t S__sslOpenCompleteParser(const char *response, char **endptr) 
{
    return ATCMD_stdResponseParser("+QSSLOPEN: ", true, ",", 1, 1, "", 0);
}


/**
 *	@brief [private] SSL wrapper for open connection parser.
 */
static cmdParseRslt_t S__socketSendCompleteParser(const char *response, char **endptr)
{
    return ATCMD_stdResponseParser("", false, "", 0, 0, "SEND OK\r\n", 0);
}


/**
 *	@brief [static] Socket status parser
 *  @details Wraps generic atcm
 *  @param response [in] Character data recv'd from BGx to parse for task complete
 *  @param endptr [out] Char pointer to the char following parsed text
 *  @return HTTP style result code, 0 = not complete
 */
static cmdParseRslt_t S__socketStatusParser(const char *response, char **endptr) 
{
    // BGx +QMTCONN Read returns Status = 3 for connected, service parser returns 203
    return ATCMD_serviceResponseParser(response, "+QISTATE: ", 5, endptr) == 202 ? resultCode__success : resultCode__unavailable;
}

#pragma endregion
