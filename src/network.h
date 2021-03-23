/******************************************************************************
 *  \file network.h
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
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
 *****************************************************************************/

#ifndef __PROTOCOLS_H__
#define __PROTOCOLS_H__

#include "ltem1c.h"


#define BGX_PDPCONTEXT_COUNT 3
#define NTWK_DEFAULT_CONTEXT 255

/** 
 *  \brief Enum of protocols available on the modem. 
 * 
 *  Note: all of the protocols are clients, while the BGx line of modules support server mode the network carriers generally don't
*/
typedef enum protocol_tag
{
    protocol_tcp = 0x00,                ///< TCP client.
    protocol_udp = 0x01,                ///< UDP client.
    //protocol_tcpListener = 0x02,
    //protocol_udpService = 0x03,
    protocol_ssl = 0x02,                ///< SSL client.
    protocol_AnyIP = 0x02,              ///< special value that includes any of the above IP basic transport protocols.

    protocol_http = 0x20,               ///< HTTP client.
    protocol_https = 0x21,              ///< HTTPS client, HTTP over SSL.

    protocol_mqtt = 0x30,               ///< MQTT messaging client.
    protocol_mqtts = 0x31,              ///< MQTT over SSL.

    protocol_void = 0xFF                ///< No protocol, used in code to generally signal a null condition.
} protocol_t;


/** 
 *  \brief Enum of the two common APN contexts provided by network carriers.
*/
typedef enum pdpCntxtIpType_tag
{
    pdpCntxtIpType_IPV4 = 1,              ///< IP v4, 32-bit address (ex: 192.168.37.52)
    pdpCntxtIpType_IPV6 = 2               ///< IP v6, 128-bit address (ex: 2001:0db8:0000:0000:0000:8a2e:0370:7334)
} pdpCntxtIpType_t;


typedef enum pdpCntxtAuthMethods_tag
{
    pdpCntxtAuthMethods_none = 0,
    pdpCntxtAuthMethods_pap = 1,
    pdpCntxtAuthMethods_chap = 2,
    pdpCntxtAuthMethods_papChap = 3
} pdpCntxtAuthMethods_t;


#define NTWKOPERATOR_OPERNAME_SZ 29
#define NTWKOPERATOR_NTWKMODE_SZ 11


/** 
 *  \brief Struct respresenting an ACTIVE network carrier/operator.
*/
typedef struct networkOperator_tag
{
	char operName[29];                  ///< Carrier name, some carriers may report as 6-digit numeric carrier ID.
	char ntwkMode[11];                  ///< Network carrier protocol mode: CATM-1 or NB-IOT for BGx.
} networkOperator_t;

#define PDPCONTEXT_APNNAME_SZ 21
#define PDPCONTEXT_IPADDRESS_SZ 16

/** 
 *  \brief Struct representing the state of active PDP contexts (aka: APN or data context).
*/
typedef struct pdpCntxt_tag
{
    uint8_t contextId;              ///< context ID recognized by the carrier (valid are 1 to 16)
    pdpCntxtIpType_t ipType;        ///< IPv4 or IPv6
	char ipAddress[16];             ///< The IP address obtained from the carrier for this context. The IP address of the modem.
} pdpCntxt_t;


/** 
 *  \brief Struct representing the full connectivity with a connected network carrier.
*/
typedef struct network_tag
{
    networkOperator_t *networkOperator;             ///< Network operator name and protocol
    pdpCntxt_t pdpCntxts[BGX_PDPCONTEXT_COUNT];   ///< Collection of contexts with network carrier. This is typically only 1, but some carriers implement more (ex VZW).
} network_t;


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


void ntwk_create();

networkOperator_t ntwk_awaitOperator(uint16_t waitDuration);
uint8_t ntwk_getActivePdpCntxtCnt();
void ntwk_configPdpCntxt(uint8_t contxtId, pdpCntxtIpType_t ipType, const char *userId, const char *pw, pdpCntxtAuthMethods_t authMethod);
pdpCntxt_t *ntwk_getPdpCntxt(uint8_t contxtId);

void ntwk_activatePdpContext(uint8_t contxtId);
void ntwk_deactivatePdpContext(uint8_t contxtId);
void ntwk_resetPdpContexts();


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__PROTOCOLS_H__ */