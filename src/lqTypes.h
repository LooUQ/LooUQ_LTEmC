/******************************************************************************
 *  \file lqTypes.h
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2021 LooUQ Incorporated.
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
 * Global/base defines and typedefs
 *****************************************************************************/

#ifndef __LQ_TYPES_H__
#define __LQ_TYPES_H__

#define ASCII_cCR '\r'
#define ASCII_sCR "\r"
#define ASCII_cCOMMA ','
#define ASCII_cNULL '\0'
#define ASCII_cESC (char)0x1B
#define ASCII_cSPACE (char)0x20
#define ASCII_cDBLQUOTE (char)0x22
#define ASCII_cHYPHEN (char)0x2D
#define ASCII_sCTRLZ "\032"
#define ASCII_sCRLF "\r\n"
#define ASCII_sOK "OK\r\n"
#define ASCII_sMQTTTERM "\"\r\n"
#define ASCII_szCRLF 2

#define NOT_NULL 1

// to allow for BGxx error codes starting at 900 to be passed back to application
// ltem1c uses macros and the action_result_t typedef

#define RESULT_CODE_SUCCESS       200

#define RESULT_CODE_BADREQUEST    400
#define RESULT_CODE_FORBIDDEN     403
#define RESULT_CODE_NOTFOUND      404
#define RESULT_CODE_TIMEOUT       408
#define RESULT_CODE_CONFLICT      409
#define RESULT_CODE_GONE          410
#define RESULT_CODE_PRECONDFAILED 412
#define RESULT_CODE_CANCELLED     499
#define RESULT_CODE_ERROR         500
#define RESULT_CODE_UNAVAILABLE   503
#define RESULT_CODE_GTWYTIMEOUT   504               ///< signals for a background (doWork) process timeout

#define RESULT_CODE_ERRORS        400
#define RESULT_CODE_SUCCESSRANGE   99
#define RESULT_CODE_SUCCESSMAX    299
#define RESULT_CODE_BGXERRORS     500
#define RESULT_CODE_CUSTOMERRORS  600


// action_result_t should be populated with RESULT_CODE_x constant values or an errorCode (uint >= 400)
typedef uint16_t resultCode_t;

#define PERIOD_FROM_SECONDS(period)  (period * 1000)
#define DFLT_ATBUFSZ 40

static void brk()  { __asm__("BKPT"); }

#define EXPAND_ASSERTS
#ifdef EXPAND_ASSERTS
#define ASSERT(true_cond, fail_msg)  (void)((true_cond) ? 0 :  ltem_notifyApp(ltemNotifType_assertFailed, (fail_msg)))
#define ASSERTBRK(true_cond)  (void)((true_cond) ? 0 :  brk())
#else
#define ASSERT(true_cond, fail_msg)  (0)
#define ASSERTBRK(true_cond)  (0)
#endif


typedef enum ltemOptnModule_tag            // must preceed IOP
{
    ltemOptnModule_sockets = 0,
    ltemOptnModule_mqtt = 1,
    ltemOptnModule_gnss = 2,
    ltemOptnModule_geofence = 3
} ltemOptnModule_t;


typedef enum pdpProtocol_tag
{
    pdpProtocol_none = 0,
    pdpProtocol_sockets = 0x01,
    pdpProtocol_mqtt = 0x02,
    pdpProtocol_http = 0x04
} pdpProtocol_t;


typedef enum ltemNotifType_tag
{
    ltemNotifType_info = 0,

    ltemNotifType__NETWORK = 100,
    // transport (101-109)
    ltemNotifType_pdpDeactivate = 101,
    // protocols (111-129)
    ltemNotifType_scktInfo = 111,
    ltemNotifType_scktError = 112,
    ltemNotifType_mqttInfo = 113,
    ltemNotifType_mqttError = 114,
    // services (131-149)  -  NA to LTEm1c

    ltemNotifType__CATASTROPHIC = 200,
    ltemNotifType_memoryAllocFault = 201,
    ltemNotifType_localCommError = 202,
    ltemNotifType_hwNotReady = 203,
    ltemNotifType_hwInitFailed = 204,
    ltemNotifType_resetFailed = 205,
    ltemNotifType_bufferOverflow = 206,

    ltemNotifType_assertFailed = 254,
    ltemNotifType_hardFault = 255
} ltemNotifType_t;

typedef void (*appNotify_func)(uint8_t notifType, const char *notifMsg);

typedef struct lqAppDiagnostic_tag
{
    uint8_t notifType;
    char notifMsg[20];
    uint8_t protoType;
    uint8_t protoState;
    uint8_t ntwkState;
    // Hardfault 
    uint16_t ufsr;
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t return_address;
    uint32_t xpsr;
} lqAppDiagnostic_t;

#endif  /* !__LQ_TYPES_H__ */
