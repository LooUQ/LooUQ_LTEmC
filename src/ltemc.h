/** ***************************************************************************
  @file 
  @brief Driver application for control and use of the LooUQ LTEm cellular modem.

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


#ifndef __LTEMC_H__
#define __LTEMC_H__

#include <lq-types.h>                   // LooUQ embedded device library typedefs, common across products/libraries
#include <lqdiag.h>                     // PRINTDBG and ASSERT diagnostic data collection
#include "ltemc-platform.h"             // platform abstractions (arduino, etc.)


/* Add the following LTEmC feature sets as required for your project's main .cpp or .c file 
 * ----------------------------------------------------------------------------------------------- */
// #include "ltemc-sckt.h"              // tcp/udp socket communications
// #include "ltemc-tls"                 // SSL/TLS support
// #include "ltemc-http"                // HTTP(S) support: GET/POST requests
// #include "ltemc-mqtt"                // MQTT(S) support
// #include "ltemc-gnss.h"              // GNSS/GPS location services
// #include "ltemc-geo.h"               // Location geo-fencing services
// #include "ltemc-filesys.h"           // use of BGx module file system functionality
// #include "ltemc-gpio.h"              // use of BGx module GPIO expansion functionality (LTEm3F)
/* ---------------------------------------------------------------------------------------------- */

/**
 * @brief LTEmC Version and product code 
 */
#define LTEmC_VERSION "4.1.0"
#define PRODUCT "LM"                    // For use in LooUQ-Diagnostics


/**
 * @brief LTEmC internal object size definitions
 * @note It is recommended that LooUQ be consulted prior to any changes 
 */
enum ltemSz__constants
{
    ltemSz__bufferSz_rx = 2000,         // Receive buffer from module, holds command responses, async event notifications, and syncronous data
    // ltemSz__moduleTypeSz = 8,
    ltemSz__streamCnt = 4,              // 3 concurrent streams; BGx support 6 SSL/TLS capable data contexts
    ltemSz__asyncStreamCnt = 2,         // number of async streams (these require background handlers registration)
    ltemSz__errorDetailSz = 18,         // max length of error detail reported 
    ltemSz__dateTimeBffrSz = 24,        // max length of the date/time buffer
    ltemSz__reportsBffrSz = 160,        // max length of the static reporting composition buffer

    ltem__fileStreamPos = (ltemSz__streamCnt - 1)
};


/** 
 *  @brief Enum of the available dataCntxt indexes for BGx (only SSL/TLS capable contexts are supported).
 */
typedef enum dataCntxt_tag
{
    dataCntxt_0 = 0,
    dataCntxt_1 = 1,
    dataCntxt_2 = 2,
    dataCntxt_3 = 3,
    dataCntxt_4 = 4,
    dataCntxt_5 = 5,
    dataCntxt__cnt = 6,
    dataCntxt__none = 255
} dataCntxt_t;


/**
 * @brief LTEmC system constants
 * @details Field sizes, resource allocations/counts, etc.  
 */
enum ltem__constants
{
    ltem__imeiSz = 15,
    ltem__iccidSz = 24,
    ltem__dvcMfgSz = 18,
    ltem__dvcModelSz = 18,
    ltem__swVerSz = 12,
    ltem__dvcFwVerSz = 20,
    ltem__hostUrlSz = 192
};


/** 
 *  \brief Enum describing the current device/module state
 */
typedef enum deviceState_tag
{
    deviceState_powerOff = 0,        // BGx is powered off, in this state all components on the LTEm1 are powered down.
    deviceState_powerOn = 1,         // BGx is powered ON, while powered on the BGx may not be able to interact fully with the host application.
    deviceState_ready = 2,           // BGx is powered ON and ready for application/services.
    deviceState_error = 99           // error or invalid state detected.
} deviceState_t;



/** 
 *  \brief RF Priority map for BG95/BG77 modules.
*/
typedef enum ltemRfPriorityMode_tag
{
    ltemRfPriorityMode_gnss = 0,
    ltemRfPriorityMode_wwan = 1,
    ltemRfPriorityMode_none = 9
} ltemRfPriorityMode_t;


/** 
 *  \brief RF Priority map for LTEm modem with BG95/BG77 modules.
*/
typedef enum ltemRfPriorityState_tag
{
ltemRfPriorityState_unloaded = 0,           // WWAN/GNSS in unloaded state
ltemRfPriorityState_wwanPending = 1,        // WWAN in pending state
ltemRfPriorityState_gnssPending = 2,        // GNSS in pending state
ltemRfPriorityState_wwanLoaded = 3,         // WWAN in loaded state
ltemRfPriorityState_gnssLoaded = 4          // GNSS in loaded state
} ltemRfPriorityState_t;


/** 
 *  \brief Struct holding information about the physical BGx module.
*/
typedef struct modemInfo_tag
{
	char imei[PSZ(ltem__imeiSz)];            // IMEI (15 digits) International Mobile Equipment Identity or IEEE UI (aka MAC, EUI-48 or EUI-64).
	char iccid[PSZ(ltem__iccidSz)];          // ICCID (up to 24 digits) Integrated Circuit Card ID. Set in the SIM card at manufacture.
    char mfg[PSZ(ltem__dvcMfgSz)];           // Device manufacturer name
	char model[PSZ(ltem__dvcModelSz)];       // Device model number
	char fwver[PSZ(ltem__dvcFwVerSz)];       // Firmware version of the device
    char swver[PSZ(ltem__swVerSz)];          // software driver version
} modemInfo_t;


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus


// typedef void (*eventNotifCallback_func)(uint8_t notifCode, const char *message);

/**
 *	@brief Initialize the LTEm1 modem.
 *	@param ltem_config [in] - The LTE modem gpio pin configuration.
 *  @param applicationCallback [in] - If supplied (not NULL), this function will be invoked for significant LTEm events.
 */
void ltem_create(const ltemPinConfig_t ltem_config, yield_func yieldCB, appEvntNotify_func eventNotifyCB);


/**
 *	@brief Uninitialize the LTEm device structures.
 */
void ltem_destroy();


/**
 *	@brief Set radio priority. 
 *  @param [in] radioPriority The priority consumer for the radio receive path.
 *  @return Result code representing status of operation, OK = 200.
 */
resultCode_t ltem_setRfPriorityMode(ltemRfPriorityMode_t priorityMode);


/**
 *	@brief Set RF priority on BG95/BG77 modules. 
 *  @return Current RF priority mode
 */
ltemRfPriorityMode_t ltem_getRfPriorityMode();


/**
 *	@brief Set RF priority on BG95/BG77 modules. 
 *  @return Current RF priority state
 */
ltemRfPriorityState_t ltem_getRfPriorityState();


/**
 *	@brief Power on and start the modem
 *  @param resetIfPoweredOn [in] Perform a software reset on the modem, if found in a powered on state
 */
bool ltem_start(resetAction_t resetAction);


/**
 *	@brief Powers off the modem without destroying memory objects. Power modem back on with ltem_start()
 */
void ltem_stop();


// /**
//  *	@brief Performs a software restart of LTEm1.
//  *  @param coldStart [in] Set to true if the LTEm is being hard started, from a reset/power ON
//  */
// void ltem_initDevice(bool coldStart);


/**
 *	@brief Performs a reset of LTEm.
 */
bool ltem_reset(bool hardReset);


/**
 *	@brief Turn modem power on/off.
 *	@param [in] powerState New state for modem: true is on, false is off.
 */
void ltem_setPowerState(bool powerState);


/**
 *	@brief Test for responsive BGx.
 *  @return True if modem is ready and responsive.
 */
bool ltem_ping();

/* RF Priority (BG95\BG77)
 ----------------------------------------------------------------------------------------------- */

/**
 *	@brief Set RF priority on BG95/BG77 modules. 
 *	@param [in] setPriority New radio priority.
 *  @return Result code representing status of operation, OK = 200.
 */
resultCode_t ltem_setRfPriorityMode(ltemRfPriorityMode_t priorityMode);


// /**
//  *	@brief Get RF priority mode on BG95/BG77 modules. 
//  *  @return Result code representing status of operation, OK = 200.
//  */
// ltemRfPriorityMode_t ltem_getRfPriorityMode();


// /**
//  *	@brief Get RF priority state on BG95/BG77 modules. 
//  *  @return Result code representing status of operation, OK = 200.
//  */
// ltemRfPriorityState_t ltem_getRfPriorityState();


// /**
//  *	@brief Get the current UTC date and time.
//  *  @param [out] dateTime Pointer to a character array (length >= 20 chars) to be updated with current UTC date/time 
//  *  @details Formatted as: 23/09/01,13:48:55
//  */
// void ltem_getDateTimeUtc(char *dateTime);

/**
 *	@brief Get the current UTC date and time.
 */
const char* ltem_getUtcDateTime(char format);


/**
 *	@brief Get local time zone offset.
 */
int8_t ltem_getLocalTimezoneOffset(bool precise);


/**
 *  @brief Get the LTEm1 static device identification/provisioning information.
 *  @return Modem information struct, see mdminfo.h for details.
*/
modemInfo_t *ltem_getModemInfo();


/**
 *  @brief Test for SIM ready
 *  @return True if SIM is inserted and available
*/
bool ltem_isSimReady();


/**
 *  @brief Get the signal strength reported by the LTEm device at a percent
 *  @return The radio signal strength in the range of 0 to 100 (0 is no signal)
*/
uint8_t ltem_signalPercent();


/**
 *  @brief Get the signal strength reported by the LTEm device as RSSI reported
 *  @return The radio signal strength in the range of -51dBm to -113dBm (-999 is no signal)
*/
int16_t ltem_signalRSSI();


/**
 *  @brief Get the signal strength reported by the LTEm device as RSSI reported
 *  @return The raw radio signal level reported by BGx
*/
uint8_t ltem_signalRaw();


/** 
 *  @brief Get the signal strength, as a bar count for visualizations, (like on a smartphone) 
 *  @return The radio signal strength factored into a count of bars for UI display
 * */
uint8_t ltem_signalBars(uint8_t displayBarCount);


/**
 *	@brief Get the LTEmC software version.
 *  \return Version as a const char pointer.
 */
const char* ltem_getSwVersion();


/**
 *	@brief Get the LTEmC software version.
 *  \return Version as a const char pointer.
 */
const char* ltem_getModuleType();


/**
 *	@brief Reads the hardware status and internal application ready field to return device ready state
 *  \return DeviceState: 0=power off, 1=power on, 2=appl ready
 */
deviceState_t ltem_getDeviceState();


/**
 *	@brief Background work task runner. To be called in application Loop() periodically.
 */
void ltem_eventMgr();


/**
 *	@brief Registers the address (void*) of your application yield callback handler.
 *  @param yieldCallback [in] Callback function in application code to be invoked when LTEmC is in await section.
 */
void ltem_setYieldCallback(yield_func yieldCB);


/**
 *	@brief Registers the address (void*) of your application event notification callback handler.
 *  @param eventNotifCallback [in] Callback function in application code to be invoked when LTEmC is in await section.
 */
void ltem_setEventNotifCallback(appEvntNotify_func eventNotifyCB);


/**
 *	@brief Function of last resort, catastrophic failure Background work task runner. To be called in application Loop() periodically.
 *  @param notifyType [in] - Enum of broad notification categories.
 *  @param notifyMsg [in] - Message from origination about the issue being reported.
 */
void ltem_notifyApp(uint8_t notifyType, const char *notifyMsg);


#pragma region LooUQ/LTEM internal functions
/*
 ----------------------------------------------------------------------------------------------- */

void LTEM_registerDiagCallback(appDiagCallback_func diagCB);
void LTEM_diagCallback(const char* diagPointDescription);



// uint8_t LTEM__getStreamIndx(dataCntxt_t dataCntxt);


// /**
//  *	@brief Function of last resort, catastrophic failure Background work task runner. To be called in application Loop() periodically.
//  *  @param faultCode [in] - HTTP style error code.
//  */
// void ltem_notifyAssert(uint16_t faultCode)   __attribute__ ((noreturn));

#pragma endregion

#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__LTEMC_H__ */