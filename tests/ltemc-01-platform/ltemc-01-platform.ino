
#define ENABLE_DIAGPRINT                    // expand DPRINT into debug output
//#define ENABLE_DIAGPRINT_VERBOSE            // expand DPRINT and DPRINT_V into debug output
#define ENABLE_ASSERT
#include <lqdiag.h>

/* specify the pin configuration 
 * --------------------------------------------------------------------------------------------- */
#ifdef ARDUINO_ARCH_ESP32
    //#define HOST_ESP32_DEVMOD_BMS2
#else
    #define HOST_FEATHER_UXPLOR_L
    // #define HOST_FEATHER_UXPLOR             
    // #define HOST_FEATHER_LTEM3F
#endif

#include <ltemc.h>
#include <ltemc-nxp-sc16is.h>                                           // need internal references, low-level test here

/* If custom board not defined in ltemcPlatform-pins.h
 * Override by:
 * UNCOMMENT ltem_pinConfig struct initializer below to override for a custom board
 * Note: this needs to follow #include <ltemc.h> 
 */
const static ltemPinConfig_t ltem_pinConfig = 
{
    spiIndx : -1,
    spiCsPin : 8,    // original: 18
    spiClkPin : 16,  // original: 15
    spiMisoPin : 17, // original: 16
    spiMosiPin : 18, // original: 17
    irqPin : 3,      // original: 8
    statusPin : 47,
    powerkeyPin : 45,
    resetPin : 0,
    ringUrcPin : 0,
    wakePin : 48
};

#define PERIOD_FROM_SECONDS(period)  (period * 1000)
#define PERIOD_FROM_MINUTES(period)  (period * 1000 * 60)
#define ELAPSED(start, timeout) ((start == 0) ? 0 : millis() - start > timeout)
#define STRCMP(n, h)  (strcmp(n, h) == 0)

// test controls
uint16_t loopCnt = 0;
uint16_t cycle_interval = 500;
uint32_t lastCycle;

// this test has no reference to global g_lqLTEM variable
// so we need a surrogate a pointer here to test locally 
platformSpi_t* platformSpi; 

union regBuffer { uint16_t val; struct { uint8_t msb; uint8_t lsb; }; };
regBuffer txWord;
regBuffer rxWord;
uint8_t txBffr[2];
uint8_t rxBffr[2];
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

    txWord.msb = SC16IS7xx_SPR_regAddr << 3;                                          // clear SPI and NXP-bridge
    txWord.lsb = 0;
    spi_transferWord(platformSpi, txWord.val);

    lastCycle = cycle_interval;
}

//#define HALT_ON_FAULT
uint16_t byteFaults = 0;
uint16_t wordFaults = 0;
uint16_t bytesFaults = 0;

void loop() 
{
    if (ELAPSED(lastCycle, cycle_interval))
    {
        loopCnt++;
        DPRINT(PRNT_CYAN,"\n\nLoop=%d FAULTS: byte=%d, word=%d, bytes=%d\r\n", loopCnt, byteFaults, wordFaults, byteFaults);
        lastCycle = millis();


        DPRINT(0, "Writing scratchpad regiser %d with transfer BYTE...", testPattern);
        testPattern = random(256);

        rxWord.msb = (SC16IS7xx_SPR_regAddr << 3) | 0x80;
        // rxBuffer.lsb doesn't matter prior to read
        uint8_t rxData;

        spi_transferBegin(platformSpi);                                     // write scratchpad
        txWord.msb = SC16IS7xx_SPR_regAddr << 3;
        txWord.lsb = testPattern;
        rxData = spi_transferByte(platformSpi, txWord.msb);
        rxData = spi_transferByte(platformSpi, txWord.lsb);
        spi_transferEnd(platformSpi);

        spi_transferBegin(platformSpi);                                     // read scratchpad
        txWord.msb = (SC16IS7xx_SPR_regAddr << 3) | 0x80;                   // register addr + read data bit set
        txWord.lsb = 0;                                                     // doesn't matter on read
        rxData = spi_transferByte(platformSpi, txWord.msb);
        rxData = spi_transferByte(platformSpi, txWord.lsb);
        spi_transferEnd(platformSpi);

        if (testPattern == rxData)
        {
            DPRINT(PRNT_INFO, "Scratchpad transfer BYTE success.\r\n");
        }
        else
        {
            DPRINT(PRNT_WARN, "Scratchpad transfer BYTE write/read failed (expected=%d, got=%d).\r\n", testPattern, rxData);
            byteFaults++;
            #ifdef HALT_ON_FAULT
                while(1){;}
            #endif
        }


        DPRINT(0, "Writing scratchpad regiser %d with transfer WORD...", testPattern);
        testPattern = random(256);

        txWord.msb = SC16IS7xx_SPR_regAddr << 3;
        txWord.lsb = testPattern;
        rxWord.msb = (SC16IS7xx_SPR_regAddr << 3) | 0x80;
        // rxBuffer.lsb doesn't matter prior to read

        uint16_t d = spi_transferWord(platformSpi, txWord.val);
        rxWord.val = spi_transferWord(platformSpi, rxWord.val);

        if (testPattern == rxWord.lsb)
        {
            DPRINT(PRNT_INFO, "Scratchpad transfer WORD success.\r\n");
        }
        else
        {
            DPRINT(PRNT_WARN, "Scratchpad transfer WORD write/read failed (expected=%d, got=%d).\r\n", testPattern, rxWord.lsb);
            wordFaults++;
            #ifdef HALT_ON_FAULT
                while(1){;}
            #endif
        }


        DPRINT(0, "Writing scratchpad regiser %d with transfer BYTES (buffer)...", testPattern);
        testPattern = random(256);

        // write scratchpad
        txBffr[0] = SC16IS7xx_SPR_regAddr << 3;
        txBffr[1] = testPattern;
        spi_transferBytes(platformSpi, txBffr, NULL, 2);

        // read scratchpad
        txBffr[0] = (SC16IS7xx_SPR_regAddr << 3) | 0x80;                    // write: reg addr + read bit set
        txBffr[1] = 0;                                                      // doesn't matter on read
        spi_transferBytes(platformSpi, txBffr, rxBffr, 2);

        if (rxBffr[1] == testPattern)
        {
            DPRINT(PRNT_INFO, "Scratchpad transfer BYTES (buffer) success.\r\n");
        }
        else
        {
            DPRINT(PRNT_WARN, "Scratchpad transfer BYTES (buffer) write/read failed (expected=%d, got=%d).\r\n", testPattern, rxBffr[1]);
            bytesFaults++;
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
