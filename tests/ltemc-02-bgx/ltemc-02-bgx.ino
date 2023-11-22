/** ***************************************************************************
  @file 
  @brief LTEm example/test for BGx communications (hardware).

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

#define ENABLE_DIAGPRINT                    // expand DIAGPRINT into debug output
//#define ENABLE_DIAGPRINT_VERBOSE            // expand DIAGPRINT and DIAGPRINT_V into debug output
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

// LTEmC Includes
#include <ltemc-iTypes.h>                         /* not usually referenced in user application */
#include <ltemc-types.h>                            // - necessary to access internal buffers
#include <ltemc-nxp-sc16is.h>                       // - necessary to perform direct component access via SPI

ltemDevice_t g_lqLTEM;                              // - normally created in the ltemc.c file, this test is low-level and performs direct IO

#define PERIOD_FROM_SECONDS(period)  (period * 1000)
#define PERIOD_FROM_MINUTES(period)  (period * 1000 * 60)
#define ELAPSED(start, timeout) ((start == 0) ? 0 : millis() - start > timeout)
#define STRCMP(n, h)  (strcmp(n, h) == 0)

// test controls
uint8_t testPattern;
uint16_t loopCnt = 0;
uint16_t cycle_interval = 2000;
uint32_t lastCycle;


void setup() 
{
    #ifdef DIAGPRINT_SERIAL
        Serial.begin(115200);
        delay(5000);                // just give it some time
    #endif
    DPRINT(PRNT_RED,"\n\n*** LTEmC Test: ltemc-02-bgx started ***\n\n");
    g_lqLTEM.appEvntNotifyCB = appEvntNotify;                           // set the callback address

    //  Manually create/initialize modem parts used, this is a low-level test
	g_lqLTEM.pinConfig = ltem_pinConfig;                                // initialize the I/O modem internal settings
    initIO();                                                           // initialize GPIO, SPI

    /* QBG_ (caps prefix) indicates a function for LTEmC internal use. This test is a low-level
     * direct test of LTEmX I/O and BGx module functionality requiring more direct access to 
     * modem functions.
     */
    if (QBG_isPowerOn())                                                // verify/apply BGx power (normally )
        DPRINT(PRNT_dGREEN, "LTEm found already powered on.\r\n");
    else
    {
        QBG_powerOn();
        DPRINT(PRNT_dGREEN, "Powered LTEm on.\r\n");
        pDelay(5);
    }
    SC16IS7xx_start();                                                  // start SPI-UART bridge

    // prepare for running loop
    randomSeed(analogRead(0));
    lastCycle = cycle_interval;
}


void loop() 
{
    if (ELAPSED(lastCycle, cycle_interval))
    {
        lastCycle = millis();
        loopCnt++;

        testPattern = random(256);
        uint8_t txBuffer_reg;
        uint8_t rxBuffer_reg;

        txBuffer_reg = testPattern;
        // rxBuffer doesn't matter prior to read
        
        SC16IS7xx_writeReg(SC16IS7xx_SPR_regAddr, txBuffer_reg);
        rxBuffer_reg = SC16IS7xx_readReg(SC16IS7xx_SPR_regAddr);

        if (testPattern != rxBuffer_reg)
            indicateFailure("Scratchpad write/read failed (write/read register)."); 

        /* BG96 test pattern: get IMEI
        *  AT+GSN\r\r
        *  <IMEI value (20 char)>\r\r
        *  OK\r
        */

       // BGx Test

    uint8_t regValue = 0;
    // char cmd[] = "GSN\r\0";                      // less than one bridge buffer
    char cmd[] = "ATI\r\0";
    // char cmd[] = "AT+QPOWD=0\r\0";               // if others fail, but this powers down modem, RX failing
    DPRINT(PRNT_DEFAULT, "Invoking cmd: %s \r\n", cmd);

    sendCommand(cmd);
    // wait for BG96 response in FIFO buffer
    char response[65] = {0};
    recvResponse(response);

    //\r\nQuectel\r\nBG96\r\nRevision: BG96MAR02A07M1G\r\n\r\nOK\r\n", 
    //ATI\r\r\nQuectel\r\nBG77\r\nRevision: BG77LAR02A04\r\n\r\nOK\r\n"
    // test response v. expected 

    const char* validResponse = "\r\nQuectel\r\nBG";                                              // initial characters in response
    char* tailAt = NULL;

    if (strstr(response, "APP RDY"))
    {
        DPRINT(PRNT_WARN, "Received APP RDY from LTEm.\r\n");
    }
    if (strstr(response, "RDY"))
    {
        DPRINT(PRNT_WARN, "Received BG RDY from LTEm.\r\n");
    }
    if (strlen(response) == 0)
    {
        DPRINT(PRNT_WARN, "Got no response from BGx.\r\n");
        // if (nullResponses > 2)
        //     indicateFailure("BGx is not responding to cmds... failed."); 
    }

    if (strlen(response) > 40 )
    {
        if (strstr(response, validResponse))
        {
            tailAt = strstr(response, "OK\r\n");
            if (tailAt != NULL)
                DPRINT(PRNT_DEFAULT, "Got correctly formed response: \r\n%s", response);  
            else
                DPRINT(PRNT_WARN, "Missing final OK in response.\r");
        }
        else
            indicateFailure("Unexpected device information returned on cmd test... failed."); 
    }


        DPRINT(PRNT_DEFAULT,"Loop=%d \n\n", loopCnt);
     }
}


/* Test Helpers
 * ============================================================================================= */

void initIO()
{
	// on Arduino, ensure pin is in default "logical" state prior to opening
	platform_writePin(g_lqLTEM.pinConfig.powerkeyPin, gpioValue_low);
	platform_writePin(g_lqLTEM.pinConfig.resetPin, gpioValue_low);
	platform_writePin(g_lqLTEM.pinConfig.spiCsPin, gpioValue_high);

	platform_openPin(g_lqLTEM.pinConfig.powerkeyPin, gpioMode_output);		// powerKey: normal low
	platform_openPin(g_lqLTEM.pinConfig.resetPin, gpioMode_output);			// resetPin: normal low
	platform_openPin(g_lqLTEM.pinConfig.spiCsPin, gpioMode_output);			// spiCsPin: invert, normal gpioValue_high

	platform_openPin(g_lqLTEM.pinConfig.statusPin, gpioMode_input);
	platform_openPin(g_lqLTEM.pinConfig.irqPin, gpioMode_inputPullUp);

    // SPI bus
    #if defined(ARDUINO_ARCH_ESP32)
    g_lqLTEM.platformSpi = spi_createFromPins(g_lqLTEM.pinConfig.spiClkPin, g_lqLTEM.pinConfig.spiMisoPin, g_lqLTEM.pinConfig.spiMosiPin, g_lqLTEM.pinConfig.spiCsPin);
    #else
    g_lqLTEM.platformSpi = spi_createFromIndex(g_lqLTEM.pinConfig.spiIndx, g_lqLTEM.pinConfig.spiCsPin);
    #endif
    spi_start(g_lqLTEM.platformSpi);
}


#define ASCII_CR 13U

// This functionality is normally handled in the IOP module. 
// ISR functionality is tested in the ltemc-03-iopisr test
void sendCommand(const char* cmd)
{
    size_t sendSz = strlen(cmd);

    //SC16IS7xx_write(cmd, strlen(cmd));                        // normally you are going to use buffered writes like here

    for (size_t i = 0; i < sendSz; i++)
    {
        SC16IS7xx_writeReg(SC16IS7xx_FIFO_regAddr, cmd[i]);     // without a small delay the register is not moved to FIFO before next byte\char
        pDelay(1);                                              // this is NOT the typical write cycle
    }
    pDelay(300);                                                // max response time per-Quectel specs, for this test we will wait
}


// This functionality is normally handled in the IOP module's interrupt service routine (ISR). 
// ISR functionality is tested in the ltemc-03-iopisr test
void recvResponse(char *response)
{
    bool dataAvailable = false;
    uint8_t recvSz = 0;
    uint8_t attempts = 0;

    while (!dataAvailable && attempts < 20)
    {
        uint8_t lsrValue = SC16IS7xx_readReg(SC16IS7xx_LSR_regAddr);
        if (lsrValue & SC16IS7xx__LSR_RHR_dataReady)
        {
            dataAvailable = true;
            break;
        }
        attempts++;
    }
    if (dataAvailable)
    {
        recvSz = SC16IS7xx_readReg(SC16IS7xx_RXLVL_regAddr);
        SC16IS7xx_read(response, recvSz);
    }
}


void indicateFailure(const char* failureMsg)
{
	DPRINT(PRNT_ERROR, "\r\n** %s \r\n", failureMsg);
    DPRINT(PRNT_ERROR, "** Test Assertion Failed. \r\n");

    int halt = 1;
    DPRINT(PRNT_ERROR, "** Halting Execution \r\n");
    while (halt)
    {
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        delay(1000);
        platform_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        delay(100);
    }
}


//typedef void (*eventNotifFunc_t)(uint8_t notifType, uint8_t notifAssm, uint8_t notifInst, const char *notifMsg);

void appEvntNotify(appEvent_t eventType, const char *notifyMsg)
{
    if (eventType == appEvent_fault_assertFailed)
    if (eventType == appEvent_fault_assertFailed)
    {
        DPRINT(PRNT_ERROR, "LTEmC-HardFault: %s\r", notifyMsg);
    }
    else 
    {
        DPRINT(PRNT_WHITE, "LTEmC Info: %s\r", notifyMsg);
    }
    return;
}
