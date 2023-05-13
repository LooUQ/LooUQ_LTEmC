/** ****************************************************************************
  \file 
  \brief Public API providing socket streams (UDP/TCP/SSL/TLS) support
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

#define SRCFILE "SKT"                           // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT
#include "ltemc-internal.h"
#include "ltemc-sckt.h"

extern ltemDevice_t g_lqLTEM;


#define ASCII_sSENDOK "SEND OK\r\n"
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) < (y)) ? (y) : (x))


// file scope local function declarations
static void S__scktUrcHndlr();
static resultCode_t S__scktRxHndlr(streamCtrl_t *streamCtrl);

static uint16_t S__requestIrdData(dataCntxt_t dataCntxt, bool isSslTls, uint16_t requestSz);
static cmdParseRslt_t S__irdResponseHeaderParser();
static cmdParseRslt_t S__sslrecvResponseHeaderParser();

static cmdParseRslt_t S__udptcpOpenCompleteParser(const char *response, char **endptr);
static cmdParseRslt_t S__sslOpenCompleteParser(const char *response, char **endptr);
static cmdParseRslt_t S__socketSendCompleteParser(const char *response, char **endptr);
static cmdParseRslt_t S__socketStatusParser(const char *response, char **endptr);


#pragma region public sockets (IP:TCP/UDP/SSL) functions
/* --------------------------------------------------------------------------------------------- */


/**
 *	@brief Create a socket data control(TCP/UDP/SSL).
 */
void sckt_initControl(scktCtrl_t *scktCtrl, dataCntxt_t dataCntxt, streamType_t protocol, scktAppRecv_func recvCallback)
{
    ASSERT(dataCntxt < dataCntxt__cnt);

    memset(scktCtrl, 0, sizeof(scktCtrl_t));

    scktCtrl->dataCntxt = dataCntxt;
    g_lqLTEM.streams[dataCntxt]->streamType = (char)protocol;
    scktCtrl->useTls = protocol == streamType_SSL;
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
void sckt_setConnection(scktCtrl_t *scktCtrl, const char *hostUrl, uint16_t hostPort, uint16_t lclPort)
{
    strncpy(scktCtrl->hostUrl, hostUrl, sckt__urlHostSz);
    scktCtrl->hostPort = lclPort;
}


/**
 *	@brief Open a data connection (socket) to d data to an established endpoint via protocol used to open socket (TCP/UDP/TCP INCOMING).
 */
resultCode_t sckt_open(scktCtrl_t *scktCtrl, bool cleanSession)
{
    uint8_t pdpCntxt = g_lqLTEM.providerInfo->defaultContext;
    resultCode_t atResult;

    if (g_lqLTEM.streams[scktCtrl->dataCntxt]->streamType == 'U')                 // protocol == UDP
    {
        atcmd_tryInvoke("AT+QIOPEN=%d,%d,\"UDP\",\"%s\",%d,%d", pdpCntxt, scktCtrl->dataCntxt, scktCtrl->hostUrl, scktCtrl->hostPort, scktCtrl->lclPort);
        atResult = atcmd_awaitResultWithOptions(sckt__defaultOpenTimeoutMS, S__udptcpOpenCompleteParser);
    }

    else if (g_lqLTEM.streams[scktCtrl->dataCntxt]->streamType == 'T')            // protocol == TCP
    {
        atcmd_tryInvoke("AT+QIOPEN=%d,%d,\"TCP\",\"%s\",%d,%d", pdpCntxt, scktCtrl->dataCntxt, scktCtrl->hostUrl, scktCtrl->hostPort, scktCtrl->lclPort);
        atResult = atcmd_awaitResultWithOptions(sckt__defaultOpenTimeoutMS, S__udptcpOpenCompleteParser);
    }

    else if (g_lqLTEM.streams[scktCtrl->dataCntxt]->streamType == 'S')            // protocol == SSL/TLS
    {
        atcmd_tryInvoke("AT+QSSLOPEN=%d,%d,\"SSL\",\"%s\",%d,%d", pdpCntxt, scktCtrl->dataCntxt, scktCtrl->hostUrl, scktCtrl->hostPort, scktCtrl->lclPort);
        atResult = atcmd_awaitResultWithOptions(sckt__defaultOpenTimeoutMS, S__sslOpenCompleteParser);
    }
    return atResult;
}



/**
 *	@brief Close an established (open) connection socket
 */
void sckt_close(scktCtrl_t *scktCtrl)
{
    if (scktCtrl->state == scktState_closed)                                    // not open
        return;

    if (scktCtrl->useTls)
        atcmd_tryInvokeDefaults("AT+QSSLCLOSE=%d", scktCtrl->dataCntxt);        // BGx syntax different for SSL
    else
        atcmd_tryInvokeDefaults("AT+QICLOSE=%d", scktCtrl->dataCntxt);          // BGx syntax different for TCP/UDP
    
    if (atcmd_awaitResult() == resultCode__success)
    {
        scktCtrl->state = scktState_closed;
    }
}

/**
 *	@brief Close an established (open) connection socket by context number.
 *  @details This is provided for LTEm use in the case of a connection close/loss.
 */
void SCKT_closeCntxt(uint8_t cntxtNm)
{
    // if (g_lqLTEM.streams[cntxtNm].pCtrl == NULL)
    //     return;

    // sckt_close(g_lqLTEM.streams[cntxtNm].pCtrl);
}


/**
 *	@brief Reset open socket connection. This function drains the connection's data pipeline. 
 */
bool sckt_flush(scktCtrl_t *scktCtrl)
{
    // if ((scktCtrl_t *)g_lqLTEM.iop->streamPeers[scktCtrl->dataCntxt] == NULL)              // not open
    //     return;

    // if (s_requestIrdData(scktCtrl->dataCntxt, scktCtrl->recvBufCtrl._bufferSz, true))      // initiate an IRD flow
    // {
    //     scktCtrl->flushing = true;
    //     return true;
    // }
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
    resultCode_t atResult;

    if (ATCMD_awaitLock(atcmd__defaultTimeout))
    {
        atcmd_invokeReuseLock("AT+QISEND=%d,%d", scktCtrl->dataCntxt, dataSz);      // reusing manual lock
        atResult = atcmd_awaitResult();                                             // waiting for data prompt, leaving action open on return if sucessful

        // await data prompt atResult successful, now send data sub-command to actually transfer data, now automatically close action after data sent
        if (atResult == resultCode__success)
        {
            atcmd_sendCmdData(data, dataSz);
            atResult = atcmd_awaitResultWithOptions(atcmd__defaultTimeout, S__socketSendCompleteParser);
            scktCtrl->statsTxCnt++;
        }
    }
    atcmd_close();
    return atResult;                                                            // return sucess -OR- failure from sendRequest\sendRaw action
}



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

static void S__scktUrcHndlr()
{
    cBuffer_t *rxBffr = g_lqLTEM.iop->rxBffr;                           // for convenience

    // not a socket URC or insufficient chars to parse URC header
    if (cbffr_find(rxBffr, "\"pdpdeact\"", 0, 0, false) >= 0)           // +QIURC: "pdpdeact" handled at higher level, +QIURC overlaps with UDP/TCP
    {
        return;
    }

    bool isUdpTcp = CBFFR_FOUND(cbffr_find(rxBffr, "+QIURC", 0, 0, false));
    bool isSslTls = CBFFR_FOUND(cbffr_find(rxBffr, "+QSSLURC", 0, 0, false));
    if (!isUdpTcp && !isSslTls)
    {
        return;
    }

    /* UDP/TCP/SSL/TLS URC
     * ----------------------------------------------------------------------------------------- */

    char workBffr[80] = {0};
    char *workPtr = workBffr;

    int16_t nextIndx = cbffr_find(rxBffr, "+QIURC", 0, 0, true);            // advance bffr-tail ptr to starting point
    if (isUdpTcp) 
    {
        cbffr_skip(rxBffr, 9);                                              // UDP/TCP: + QIURC: "
    }
    else 
    {
        cbffr_skip(rxBffr, 11);                                             // SSL/TLS: +QSSLURC: "
    }
    uint16_t eolIndx = cbffr_find(rxBffr, "\r\n", 0, 30, false);
    if (CBFFR_FOUND(eolIndx))                                               // got full line, work on URC
    {
        cbffr_skipTail(rxBffr, 9);                                          // ignore prefix
        cbffr_pop(rxBffr, workBffr, eolIndx - 9);
    }
    else
    {
        return;                                                             // don't have full URC line yet, come back later
    }
    
    /* URC ready to process
     ----------------------------------------------------------------------- */
    uint8_t dataCntxt;

    // "recv" = socket new data receive
    if (workBffr[0] == 'r')
    {
        dataCntxt = strtol(workPtr + sizeof("recv\""), NULL, 10);
        ASSERT(dataCntxt < dataCntxt__cnt);

        uint8_t indx = LTEM__getStreamIndx(dataCntxt);
        scktCtrl_t* scktCtrl = (scktCtrl_t*)g_lqLTEM.streams[indx];
        scktCtrl->irdPending = S__requestIrdData(dataCntxt, isSslTls, cbffr_getOpenCnt(rxBffr));

        uint16_t irdSz = cbffr_getVacant(g_lqLTEM.iop->rxBffr) / 2;              // request up to half of available buffer space

        if (isUdpTcp)
        {
            atcmd_setStreamControl("+QIRD:", scktCtrl);
            atcmd_tryInvoke("AT+QIRD=%d,%d", (uint8_t)dataCntxt, irdSz);
            atcmd_awaitResultWithOptions(0, S__irdResponseHeaderParser);            // response header prior to data stream
        }
        else
        {
            atcmd_setStreamControl("+QSSLRECV:", scktCtrl);
            atcmd_tryInvoke("AT+QSSLRECV=%d,%d", (uint8_t)dataCntxt, irdSz);
            atcmd_awaitResultWithOptions(0, S__sslrecvResponseHeaderParser);
        }
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
static resultCode_t S__scktRxHndlr(streamCtrl_t *streamCtrl)
{
    /* +QIRD: <read_actual_length>/r/n<data>
     * +QSSLRECV: <havereadlen>/r/n<data>
     */

    char wrkBffr[32] = {0};
    char *wrkPtr = wrkBffr;
    scktCtrl_t *scktCtrl = (scktCtrl_t*)streamCtrl;
    ASSERT(scktCtrl);
    
    uint8_t popCnt = cbffr_find(g_lqLTEM.iop->rxBffr, "\r", 0, 0, false);
    if (CBFFR_NOTFOUND(popCnt))
    {
        return resultCode__internalError;
    }
    
    cbffr_pop(g_lqLTEM.iop->rxBffr, wrkBffr, popCnt + 2);                                               // pop preamble phrase for parsing data length
    wrkPtr = memchr(wrkBffr, ':', popCnt) + 2;
    uint16_t availSz = strtol(wrkPtr, NULL, 10);

    PRINTF(dbgColor__cyan, "scktRxHndlr() cntxt=%d sz=%d\r", scktCtrl->dataCntxt, availSz);
    while (availSz > 0)
    {
        uint32_t readTimeout = pMillis();
        uint16_t occupiedCnt;
        do
        {
            occupiedCnt = cbffr_getOccupied(g_lqLTEM.iop->rxBffr);
            if (pMillis() - readTimeout > sckt__readTimeoutMs)
            {
                return resultCode__timeout;
            }
        } while (occupiedCnt == 0);
        
        char* streamPtr;
        uint16_t blockSz = cbffr_popBlock(g_lqLTEM.iop->rxBffr, &streamPtr, availSz);                               // get data ptr from rxBffr
        PRINTF(dbgColor__cyan, "scktRxHndlr() ptr=%p, blkSz=%d, availSz=%d\r", streamPtr, blockSz, availSz);

        availSz -= blockSz;
        ((scktAppRecv_func)(*scktCtrl->appRecvDataCB))(scktCtrl->dataCntxt, streamPtr, blockSz, availSz == 0);    // forward to application
        cbffr_popBlockFinalize(g_lqLTEM.iop->rxBffr, true);                                                         // commit POP

        if (cbffr_getOccupied(g_lqLTEM.iop->rxBffr) >= sckt__readTrailerSz)                                         // cleanup: remove trailer
        {
            cbffr_skipTail(g_lqLTEM.iop->rxBffr, sckt__readTrailerSz);
        }
    }
    return resultCode__success;
}



// /**
//  *	@brief Fetch receive data by host application
//  */
// uint16_t sckt_fetchRecv(scktCtrl_t *scktCtrl, char *recvBffr, uint16_t bffrSz)
// {
//     if (scktCtrl->irdPending)
//     {
//         uint16_t fetchCnt = MIN(scktCtrl->irdPending, bffrSz);
//         cbffr_pop(g_lqLTEM.iop->rxBffr, recvBffr, fetchCnt);
//         scktCtrl->irdPending -= fetchCnt;

//         // scktCtrl->hostBffrHint = bffrSz;                            // ??? WHY

//         if (scktCtrl->irdPending > 0)
//             return scktCtrl->irdPending;

//         // need to requery IRD/SSLRECV
//         char proto = g_lqLTEM.streams[scktCtrl->dataCntxt]->streamType[0];
//         S__requestIrdData(scktCtrl->dataCntxt, proto == 'S', bffrSz);
//     }
//     else
//         return 0;
// }


#pragma endregion


#pragma region private service functions and UDP/TCP/SSL response parsers
/*-----------------------------------------------------------------------------------------------*/

/**
 *	\brief [private] UDP/TCP (IRD Request) response parser.
 *  \return LTEmC parse result
 */
static cmdParseRslt_t S__irdResponseHeaderParser() 
{
    return atcmd_stdResponseParser("+QIRD: ", true, ",", 1, 1, "\r\n", 0);
}


/**
 *	\brief [private] UDP/TCP (IRD Request) response parser.
 *  \return LTEmC parse result
 */
static cmdParseRslt_t S__sslrecvResponseHeaderParser() 
{
    return atcmd_stdResponseParser("+QSSLRECV: ", true, ",", 1, 1, "\r\n", 0);
}


/**
 *	@brief [private] TCP/UDP wrapper for open connection parser.
 */
static cmdParseRslt_t S__udptcpOpenCompleteParser(const char *response, char **endptr) 
{
    return atcmd_stdResponseParser("+QIOPEN: ", true, NULL, 1, 1, NULL, 0);
}


/**
 *	@brief [private] SSL wrapper for open connection parser.
 */
static cmdParseRslt_t S__sslOpenCompleteParser(const char *response, char **endptr) 
{
    return atcmd_stdResponseParser("+QSSLOPEN: ", true, NULL, 1, 1, NULL, 0);
}


/**
 *	@brief [private] SSL wrapper for open connection parser.
 */
static cmdParseRslt_t S__socketSendCompleteParser(const char *response, char **endptr)
{
    return atcmd_stdResponseParser("", false, NULL, 0, 0, ASCII_sSENDOK, 0);
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
    return atcmd_serviceResponseParser(response, "+QISTATE: ", 5, endptr) == 202 ? resultCode__success : resultCode__unavailable;
}

#pragma endregion
