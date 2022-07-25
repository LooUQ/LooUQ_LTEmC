/******************************************************************************
 *  \file ltemc-atcmd.h
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
 *****************************************************************************/

#ifndef __LTEMC_TYPES_H__
#define __LTEMC_TYPES_H__

#include <lq-types.h>

enum bufferSizes
{
                                /* can be reduced based on you protocol selections and your data segment sizes */
    bufferSz__txData = 1800,    // size should be equal or greater than length of largest data transmission
    bufferSz__cmdTx = 192,
    bufferSz__coreRx = 192,

};


typedef struct ltemPinConfig_tag
{
    int spiCsPin;
    int irqPin;
    int statusPin;
    int powerkeyPin;
    int resetPin;
    int ringUrcPin;
    int wakePin;
} ltemPinConfig_t;



/** 
 *  \brief Enum describing the current device/module state
 */
typedef enum deviceState_tag
{
    deviceState_powerOff = 0,        ///< BGx is powered off, in this state all components on the LTEm1 are powered down.
    deviceState_powerOn = 1,         ///< BGx is powered ON, while powered on the BGx may not be able to interact fully with the host application.
    deviceState_appReady = 2         ///< BGx is powered ON and ready for application/services.
} deviceState_t;


typedef enum cmdParseRslt_tag
{
    cmdParseRslt_pending = 0x00,

    cmdParseRslt_preambleMissing = 0x01,
    cmdParseRslt_countShort = 0x02,
    cmdParseRslt_moduleError = 0x04,
    cmdParseRslt_excessRecv = 0x20,

    cmdParseRslt_success = 0x40,
    cmdParseRslt_error = 0x80,
} cmdParseRslt_t;

 /** 
 *  @brief Background work function signature.
 *  @details Most subsystems are linked to this struct with pointers to allow for better abstraction and optional subsystems
 */
typedef void (*moduleDoWorkFunc_t)();




/* LTEmC global fields/properties as Singleton 
 * ------------------------------------------------------------------------------------------------------------------------------*/

 /** 
 *  \brief Struct representing the LTEmC model. The struct behind the g_ltem1 global variable with all driver controls.
 * 
 *  Most subsystems are linked to this struct with pointers to allow for better abstraction and optional subsystems
 */
typedef struct ltemDevice_tag
{
    // ltem1Functionality_t funcLevel;      /// Enum value indicating services enabled during ltemC startup
	ltemPinConfig_t pinConfig;              /// GPIO pin configuration for required GPIO and SPI interfacing
    bool cancellationRequest;               /// For RTOS implementations, token to request cancellation of long running task/action
    deviceState_t deviceState;              /// Device state of the BGx module
    appEventCallback_func appEventCB;       /// Event notification callback to parent application
    uint8_t instNm;                         /// LTEm instance number 0=undefined, 1..254
    char moduleType[8];                     /// c-str indicating module type. BG96, BG95-M3 (so far)

    void *spi;                              /// SPI device (methods signatures compatible with Arduino)
    void *pdpContext;                       /// The primary packet data protocol (PDP) context with the network carrier for application transfers
    void *iop;                              /// IOP subsystem controls
    void *atcmd;                            /// Action subsystem controls
	void *modemInfo;                        /// Data structure holding persistent information about application modem state
    void *providerInfo;                     /// Data structure representing the cellular network provider and the networks (PDP contexts it provides)
    moduleDoWorkFunc_t streamWorkers[6];    /// Stream background doWork functions, registered by Open;
} ltemDevice_t;

typedef struct ltemc_sensor
{
    int id;

    uint32_t sampleLastAt;                  /// last sample taken instance (milli count)
    uint32_t samplePeriod;                  /// the planned interval between samples
    uint16_t sampleCount;                   /// the number of invokes for the sampleCallback, cleared with each alert/report
    void *sampleCallback;                   /// user defined function to perform sample acquisition and report formatting

    uint32_t rptLastAt;                     /// last reporting instance (milli count)
    uint32_t rptPeriod;                     /// the planned interval between device to LQCloud sensor samples reporting
    char *rptBuffer;                        /// character buffer to store sample event output
    uint16_t rptBufferSz;                   /// buffer size in characters, includes the intended /0 char string terminating symbol

    uint32_t alertAt;                       /// source millis cnt at event sourcing
};


/*  LQCloud/device time syncronization; at each LQCloud interaction cloud will send new epochRef (Unix time of cloud)
 *  Device will record this value along with the current millis count. This information is sent to cloud with 
 *  (epochRef and elapsed, where elapsed = now (millis) - epochAt) alert/report connection to allow LQCloud to correlate
 *  device information at a specific date/time.
 * 
 *  Timing derived from millisecond counter, such as Arduini millis(); max at 49.7 days 
 */
typedef struct ltemcEpoch
{
    char epochRef[12];                      /// unix epoch reported at last alert/report interaction with LQCloud
    uint32_t epochAt;                       /// millis counter at the last alert/report interaction with LQCloud
};


extern ltemDevice_t g_lqLTEM;            ///< The LTEm "object".


typedef cmdParseRslt_t (*cmdResponseParser_func)(ltemDevice_t *modem);



#endif  // !__LTEMC_TYPES_H__
