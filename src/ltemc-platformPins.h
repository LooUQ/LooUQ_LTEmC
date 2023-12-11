/** ***************************************************************************
  @file 
  @brief LTEm pinout (GPIO/SPI) abstraction declarations.

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


/* Add one of the host board defines from the list below to configure your LTEm1 connection. 
 * To extend, define your connections in a new ltem1PinConfig_t initialization and give it a name.

#define HOST_FEATHER_UXPLOR
#define HOST_FEATHER_BASIC
#define HOST_RASPI_UXPLOR
*/


//#define STATUS_LOW_PULLDOWN

#ifdef HOST_FEATHER_UXPLOR_L
const ltemPinConfig_t ltem_pinConfig =
{
    spiIndx : 0,
    spiCsPin : 13,
    spiClkPin : 0,
    spiMisoPin : 0,
    spiMosiPin : 0,
    irqPin : 12,
    statusPin : 6,                  ///< HIGH indicates ON
    powerkeyPin : 11,               ///< toggle HIGH to change state
    resetPin : 10,                  ///< reset active HIGH
    ringUrcPin : 0,             
    connected : 0,    
    wakePin : 0,
};
#endif

#ifdef HOST_FEATHER_UXPLOR
const ltemPinConfig_t ltem_pinConfig =
{
    spiIndx : 0,
    spiCsPin : 13,
    spiClkPin : 0,
    spiMisoPin : 0,
    spiMosiPin : 0,
    irqPin : 12,
    statusPin : 19,                 ///< (aka A5) HIGH indicates ON 
    powerkeyPin : 11,               ///< toggle HIGH to change state
    resetPin : 10,                  ///< reset active HIGH
    ringUrcPin : 15,                /// (aka A2)
    connected : 17,                 /// (aka A3)
    wakePin : 18                    /// (aka A4)
};
#endif


#ifdef HOST_FEATHER_LTEM3F
const ltemPinConfig_t ltem_pinConfig =
{
    spiIndx : 0,
    spiCsPin : 18,                  /// AKA A4
    spiClkPin : 0,
    spiMisoPin : 0,
    spiMosiPin : 0,
    irqPin : 19,                    /// AKA A5
    statusPin : 12,                 /// HIGH indicates ON
    powerkeyPin : 10,               /// toggle HIGH to change state
    resetPin : 11,                  /// reset active HIGH
    ringUrcPin : 0,                 
    connected : 0,
    wakePin : 0
};
#define LQLTE_MODULE BG77
#endif

#ifdef HOST_FEATHER_BASIC
const ltemPinConfig_t ltem_pinConfig =
{
    spiIndx : 0,
    spiCsPin : 13,
    spiClkPin : 0,
    spiMisoPin : 0,
    spiMosiPin : 0,
    irqPin : 12,
    statusPin : 6,
    powerkeyPin : 11,
    resetPin : 19,
    ringUrcPin : 5,
    connected : 0,
    wakePin : 10
};
#endif

#ifdef HOST_RASPI_UXPLOR
const ltemPinConfig_t ltem_pinConfig = 
{
    spiIndx : -1,
    spiCsPin = 0, 			//< J8_24
    spiClkPin : 0,
    spiMisoPin : 0,
    spiMosiPin : 0,
	irqPin = 22U,		    //< J8_15
	statusPin = 13U,		//< J8_22
	powerkeyPin = 24U,		//< J8_18
	resetPin = 23U,		//< J8_16
	ringUrcPin = 0,
    wakePin = 0
};
#endif

#ifdef HOST_ESP32_DEVMOD_BMS
const ltemPinConfig_t ltem_pinConfig = 
{
    spiIndx : -1,
    spiCsPin : 18,
    spiClkPin : 15,
    spiMisoPin : 16,
    spiMosiPin : 17,
	irqPin : 8,
	statusPin : 47,
	powerkeyPin : 45,
	resetPin : 0,
	ringUrcPin : 0,
    wakePin : 48
};
#endif

#ifdef HOST_LOOUQ_REMOTENODE
const ltemPinConfig_t ltem_pinConfig =
{
    spiIndx : 1,
    spiCsPin : 13,
    spiClkPin : 0,
    spiMisoPin : 0,
    spiMosiPin : 0,
    irqPin : 12,
    statusPin : 6,                  ///< HIGH indicates ON
    powerkeyPin : 11,               ///< toggle HIGH to change state
    resetPin : 10,                  ///< reset active HIGH
    ringUrcPin : 0,             
    connected : 0,    
    wakePin : 0,
};
#endif

