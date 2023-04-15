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
static void S__scktUrcHandler();
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
void sckt_initControl(scktCtrl_t *scktCtrl, dataCntxt_t dataCntxt, const char *protocol, scktRecv_func recvCallback)
{
    ASSERT(dataCntxt < dataCntxt__cnt);

    memset(scktCtrl, 0, sizeof(scktCtrl_t));

    scktCtrl->dataCntxt = dataCntxt;
    memcpy(g_lqLTEM.streams[dataCntxt].streamType,protocol, strlen(protocol));
    scktCtrl->useTls = memcmp(protocol, STREAM_SSL, sizeof(STREAM_SSL));
    scktCtrl->irdPending = false;
    scktCtrl->flushing = false;
    scktCtrl->statsRxCnt = 0;
    scktCtrl->statsTxCnt = 0;

    g_lqLTEM.streams[dataCntxt].pCtrl = scktCtrl;
    g_lqLTEM.streams[dataCntxt].recvDataCB = recvCallback;
    LTEM_registerUrcHandler(S__scktUrcHandler);
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

    if (g_lqLTEM.streams[scktCtrl->dataCntxt].streamType[0] == 'U')                 // protocol == UDP
    {
        atcmd_tryInvoke("AT+QIOPEN=%d,%d,\"UDP\",\"%s\",%d,%d", pdpCntxt, scktCtrl->dataCntxt, scktCtrl->hostUrl, scktCtrl->hostPort, scktCtrl->lclPort);
        atResult = atcmd_awaitResultWithOptions(sckt__defaultOpenTimeoutMS, S__udptcpOpenCompleteParser);
    }

    else if (g_lqLTEM.streams[scktCtrl->dataCntxt].streamType[0] == 'T')
    {
        atcmd_tryInvoke("AT+QIOPEN=%d,%d,\"TCP\",\"%s\",%d,%d", pdpCntxt, scktCtrl->dataCntxt, scktCtrl->hostUrl, scktCtrl->hostPort, scktCtrl->lclPort);
        atResult = atcmd_awaitResultWithOptions(sckt__defaultOpenTimeoutMS, S__udptcpOpenCompleteParser);
    }

    else if (g_lqLTEM.streams[scktCtrl->dataCntxt].streamType[0] == 'S')
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
    if (g_lqLTEM.streams[cntxtNm].pCtrl == NULL)
        return;

    sckt_close(g_lqLTEM.streams[cntxtNm].pCtrl);
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
            atcmd_sendCmdData(data, dataSz, "");    // parse for BGx send complete
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
 *   @brief Perform background tasks to move socket data through pipeline.
 * 
 *   1) check for existing IRD flow and proccess
 *   2) deliver received data to application
 *   3) if no IRD flow underway, check for other sockets that have dataPending
*/
static void S__scktUrcHandler()
{
    cBuffer_t *rxBffr = g_lqLTEM.iop->rxBffr;                           // for convenience
    char parseBffr[SCKT_URC_HEADERSZ];

    bool isUdpTcp = cbffr_find(rxBffr, "+QIURC", 0, 0, false) != CBFFR_NOFIND;
    bool isSslTls = cbffr_find(rxBffr, "+QSSLURC", 0, 0, false) != CBFFR_NOFIND;

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

    // not a socket URC or insufficient chars to parse URC header
    if (!isUdpTcp && !isSslTls && cbffr_peek(rxBffr, parseBffr, SCKT_URC_HEADERSZ) < SCKT_URC_HEADERSZ)   
        return;                                                     
    if (cbffr_find(rxBffr, "+QIURC: \"pdpdeact\"", 0, 0, false) >= 0)                  // handled at higher level
        return;

    /* UDP/TCP/SSL/TLS URC
     * ----------------------------------------------------------------------------------------- */

    g_lqLTEM.urcActive = 'S';

    int16_t nextIndx = cbffr_find(rxBffr, "+Q", 0, 0, true);                    // advance bffr-tail ptr to starting point
    if (isUdpTcp) {
        cbffr_skip(rxBffr, 9);
    }
    else {
        cbffr_skip(rxBffr, 11);
    }
    cbffr_peek(rxBffr, parseBffr, sizeof(parseBffr));

    long dataCntxt;
    scktCtrl_t *scktCtrl;
    if (parseBffr[0] == 'c')                                                    // socket closed
    {
        dataCntxt = strtol(parseBffr + 8 , NULL, 10);
        ASSERT(dataCntxt < dataCntxt__cnt);
        scktCtrl = ((scktCtrl_t*)g_lqLTEM.streams[dataCntxt].pCtrl);
        scktCtrl->state = scktState_closed;
        return;
    }

    // if (parseBffr[0] == 'r')                                                 // socket new receive
    dataCntxt = strtol(parseBffr + 6, NULL, 10);
    ASSERT(dataCntxt < dataCntxt__cnt);
    scktCtrl = ((scktCtrl_t*)g_lqLTEM.streams[dataCntxt].pCtrl);

    scktCtrl->irdPending = S__requestIrdData(dataCntxt, isSslTls, cbffr_getOpenCnt(rxBffr));
    if (scktCtrl->irdPending)
        g_lqLTEM.recvCntxt = dataCntxt;                                         // IRD/SSLRECV kicks off flow incoming data, ISR buffers it
    else
        g_lqLTEM.recvCntxt = dataCntxt__none;

    // DoWork() will monitor g_lqLTEM.recvEvent and g_lqLTEM.recvPending to service incoming stream and send to host application.
}    


/**
 *	@brief Fetch receive data by host application
 */
uint16_t sckt_fetchRecv(scktCtrl_t *scktCtrl, char *recvBffr, uint16_t bffrSz)
{
    if (scktCtrl->irdPending)
    {
        uint16_t fetchCnt = MIN(scktCtrl->irdPending, bffrSz);
        cbffr_pop(g_lqLTEM.iop->rxBffr, recvBffr, fetchCnt);
        scktCtrl->irdPending -= fetchCnt;

        // scktCtrl->hostBffrHint = bffrSz;                            // ??? WHY

        if (scktCtrl->irdPending > 0)
            return scktCtrl->irdPending;

        // need to requery IRD/SSLRECV
        char proto = g_lqLTEM.streams[scktCtrl->dataCntxt].streamType[0];
        S__requestIrdData(scktCtrl->dataCntxt, proto == 'S', bffrSz);
    }
    else
        return 0;
}


/**
 *	@brief Cancel an active receive flow and discard any recieved bytes.
 *  @details This is a blocking call, returns after all outstanding bytes are retrieved from module and discarded. Connection can continue receiving new bytes if not closed.
 * 
 *	@param scktCtrl [in] Pointer to socket control being operated on.
 */
void sckt_cancelRecv(scktCtrl_t *scktCtrl)
{}


#pragma endregion


#pragma region private service functions and UDP/TCP/SSL response parsers
/*-----------------------------------------------------------------------------------------------*/

static uint16_t S__requestIrdData(dataCntxt_t dataCntxt, bool isSslTls, uint16_t requestSz)
{
    uint16_t dataRequest = cbffr_getOpenCnt(g_lqLTEM.iop->rxBffr);              // request up to current space available
    if (isSslTls)
    {
        atcmd_tryInvoke("AT+QSSLRECV=%d,%d", (uint8_t)dataCntxt, dataRequest);
        atcmd_awaitResultWithOptions(0, S__sslrecvResponseHeaderParser);
    }
    else
    {
        atcmd_tryInvoke("AT+QIRD=%d,%d", (uint8_t)dataCntxt, dataRequest);
        atcmd_awaitResultWithOptions(0, S__irdResponseHeaderParser);            // response header prior to data stream
    }
    return atcmd_getValue();
}


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
