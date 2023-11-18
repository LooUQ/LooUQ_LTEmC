/** ***************************************************************************
  @file 
  @brief LTEm example/test for platform (porting abstraction) for GPIO/SPI.

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

#define ENABLE_DIAGPRINT                    // expand DPRINT into debug output
//#define ENABLE_DIAGPRINT_VERBOSE            // expand DPRINT and DPRINT_V into debug output
#define ENABLE_ASSERT
#include <lqdiag.h>

/* specify the pin configuration 
 * --------------------------------------------------------------------------------------------- */
#ifdef ARDUINO_ARCH_ESP32
    #define HOST_ESP32_DEVMOD_BMS
#else
    #define HOST_FEATHER_UXPLOR_L
    // #define HOST_FEATHER_UXPLOR             
    // #define HOST_FEATHER_LTEM3F
#endif

#include <ltemc.h>
#include <ltemc-nxp-sc16is.h>                                           // need internal references, low-level test here

#define PERIOD_FROM_SECONDS(period)  (period * 1000)
#define PERIOD_FROM_MINUTES(period)  (period * 1000 * 60)
#define ELAPSED(start, timeout) ((start == 0) ? 0 : millis() - start > timeout)
#define STRCMP(n, h)  (strcmp(n, h) == 0)

// test controls
uint16_t loopCnt = 0;
uint16_t cycle_interval = 2000;
uint32_t lastCycle;

// this test has no reference to global g_lqLTEM variable
// so we need a surrogate a pointer here to test locally 
platformSpi_t* platformSpi; 

union regBuffer { uint16_t val; struct { uint8_t msb; uint8_t lsb; }; };
regBuffer txBuffer;
regBuffer rxBuffer;
uint8_t testPattern;


void setup() 
{
    #ifdef DIAGPRINT_SERIAL
        Serial.begin(115200);
        delay(5000);                // just give it some time
    #endif
    DPRINT(PRNT_RED, "LTEmC - Test #1: Platform I/O and SPI\r\n");
    DPRINT(PRNT_DEFAULT, "LED pin = %i \r\n", LED_BUILTIN);           // could have used 0 as color code, rather than enum dbgColor__none
    randomSeed(analogRead(7));

	platform_writePin(ltem_pinConfig.powerkeyPin, gpioValue_low);
	platform_writePin(ltem_pinConfig.resetPin, gpioValue_low);
	platform_writePin(ltem_pinConfig.spiCsPin, gpioValue_high);
    
	platform_openPin(ltem_pinConfig.powerkeyPin, gpioMode_output);
	platform_openPin(ltem_pinConfig.statusPin, gpioMode_input);

    DPRINT(PRNT_DEFAULT, "Modem status(%i) = %i \r\n", ltem_pinConfig.statusPin, platform_readPin(ltem_pinConfig.statusPin));

    powerModemOn();
    DPRINT(PRNT_DEFAULT, "   ON Status = %i \r\n", platform_readPin(ltem_pinConfig.statusPin));
    pDelay(500);

    powerModemOff();
    DPRINT(PRNT_DEFAULT, "  OFF Status = %i \r\n", platform_readPin(ltem_pinConfig.statusPin));
    pDelay(500);

    DPRINT(PRNT_INFO, "Turn modem on for SPI tests\r\n");
    powerModemOn();

    #if defined(ARDUINO_ARCH_SAMD)
	platformSpi = spi_createFromIndex(ltem_pinConfig.spiIndx, ltem_pinConfig.spiCsPin);
    #else
	platformSpi = spi_createFromPins(ltem_pinConfig.spiClkPin, ltem_pinConfig.spiMisoPin, ltem_pinConfig.spiMosiPin, ltem_pinConfig.spiCsPin);
    #endif

	if (platformSpi == NULL)
	{
        DPRINT(PRNT_WARN, "SPI setup failed.\r\n");
	}
    spi_start(platformSpi);

    txBuffer.msb = SC16IS7xx_SPR_regAddr << 3;                                          // clear SPI and NXP-bridge
    txBuffer.lsb = 0;
    spi_transferWord(platformSpi, txBuffer.val);

    lastCycle = cycle_interval;
}

//#define HALT_ON_FAULT
uint16_t wFaults = 0;
uint16_t bFaults = 0;

void loop() 
{
    if (ELAPSED(lastCycle, cycle_interval))
    {
        loopCnt++;
        DPRINT(0,"\n\nLoop=%d (wFaults= %d, bFaults=%d)\n", loopCnt, wFaults, bFaults);
        lastCycle = millis();
        testPattern = random(256);

        txBuffer.msb = SC16IS7xx_SPR_regAddr << 3;
        txBuffer.lsb = testPattern;
        rxBuffer.msb = (SC16IS7xx_SPR_regAddr << 3) | 0x80;
        // rxBuffer.lsb doesn't matter prior to read

        DPRINT(0, "Writing scratchpad regiser %d with transfer WORD...", testPattern);
        uint16_t d = spi_transferWord(platformSpi, txBuffer.val);
        rxBuffer.val = spi_transferWord(platformSpi, rxBuffer.val);

        if (testPattern == rxBuffer.lsb)
        {
            DPRINT(PRNT_INFO, "Scratchpad transfer WORD success.\r\n");
        }
        else
        {
            DPRINT(PRNT_WARN, "Scratchpad transfer WORD write/read failed (expected=%d, got=%d).\r\n", testPattern, rxBuffer.lsb);
            wFaults++;
            #ifdef HALT_ON_FAULT
                while(1){;}
            #endif
        }

        // SPI operations are destructive to register addr; reset addr and incr pattern to differentiate
        testPattern += 15;
        txBuffer.msb = SC16IS7xx_SPR_regAddr << 3;
        rxBuffer.msb = (SC16IS7xx_SPR_regAddr << 3) | 0x80;  // write: reg addr + data
        txBuffer.lsb = testPattern;

        DPRINT(0, "Writing scratchpad regiser %d with transfer BUFFER...", testPattern);
        spi_transferBuffer(platformSpi, txBuffer.msb, &txBuffer.lsb, 1);
        spi_transferBuffer(platformSpi, rxBuffer.msb, &rxBuffer.lsb, 1);

        if (rxBuffer.lsb == testPattern)
        {
            DPRINT(PRNT_INFO, "Scratchpad transfer BUFFER success.\r\n");
        }
        else
        {
            DPRINT(PRNT_WARN, "Scratchpad transfer BUFFER write/read failed (expected=%d, got=%d).\r\n", testPattern, rxBuffer.lsb);
            bFaults++;
            #ifdef HALT_ON_FAULT
                while(1){;}
            #endif
        }
     }
}


/*
========================================================================================================================= */

void powerModemOn()
{
	if (!platform_readPin(ltem_pinConfig.statusPin))
	{
		DPRINT(0, "Powering LTEm On...");
		platform_writePin(ltem_pinConfig.powerkeyPin, gpioValue_high);
		pDelay(1000);
		platform_writePin(ltem_pinConfig.powerkeyPin, gpioValue_low);
		while (!platform_readPin(ltem_pinConfig.statusPin))
		{
			pDelay(500);
		}
		DPRINT(0, "DONE.\r\n");
	}
	else
	{
		DPRINT(0, "LTEm is already powered on.\r\n");
	}
}


void powerModemOff()
{
	if (platform_readPin(ltem_pinConfig.statusPin))
	{
		DPRINT(0, "Powering LTEm Off...");
		platform_writePin(ltem_pinConfig.powerkeyPin, gpioValue_high);
		pDelay(1000);
		platform_writePin(ltem_pinConfig.powerkeyPin, gpioValue_low);
		while (platform_readPin(ltem_pinConfig.statusPin))
		{
			pDelay(500);
		}
		DPRINT(0, "DONE.\r\n");
	}
	else
	{
		DPRINT(0, "LTEm is already powered off.\r\n");
	}
}
