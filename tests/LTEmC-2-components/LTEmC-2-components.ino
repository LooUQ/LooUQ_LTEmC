/******************************************************************************
 *  \file LTEmC-2-components.ino
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
 ******************************************************************************
 * Test 2: tests the LTEm NXP serial bridge chip and BGx module for basic
 * serial operations. 
 * 
 * The sketch is designed for debug output to observe results.
 *****************************************************************************/

#define _DEBUG 2                        // set to non-zero value for PRINTF debugging output, 
// debugging output options             // LTEm1c will satisfy PRINTF references with empty definition if not already resolved
#if defined(_DEBUG)
    asm(".global _printf_float");       // forces build to link in float support for printf
    #if _DEBUG == 2
    #include <jlinkRtt.h>               // output debug PRINTF macros to J-Link RTT channel
    #define PRINTF(c_,f_,__VA_ARGS__...) do { rtt_printf(c_, (f_), ## __VA_ARGS__); } while(0)
    #else
    #define SERIAL_DBG _DEBUG           // enable serial port output using devl host platform serial, _DEBUG 0=start immediately, 1=wait for port
    #endif
#else
#define PRINTF(c_, f_, ...) ;
#endif

                                                    // define options for how to assemble this build
//#define HOST_FEATHER_UXPLOR                         // specify the pin configuration
#define HOST_FEATHER_LTEM3F

#include <ltemc.h>
#include <ltemc-internal.h>                         // not usually referenced in application, necessary to access non-public functions
#include <ltemc-nxp-sc16is.h>

void setup() {
    #ifdef SERIAL_DBG
        Serial.begin(115200);
        #if (SERIAL_OPT > 0)
        while (!Serial) {}      // force wait for serial ready
        #else
        delay(5000);            // just give it some time
        #endif
    #endif

    PRINTF(dbgColor__red, "LTEmC Test2: modem components\r\n");
    lqDiag_registerEventCallback(appNotifyCB);                      // configure ASSERTS to callback into application

    ltem_create(ltem_pinConfig, NULL, appNotifyCB);                 // create LTEmC modem
    randomSeed(analogRead(0));

    // initialize GPIO, SPI, UART-Bridge, BGx power 
    initIO();
    spi_start(g_ltem.spi);

    
    if (qbg_isPowerOn())
        PRINTF(dbgColor__dGreen, "LTEm found already powered on.\r\n");
    else
    {
        qbg_powerOn();
        PRINTF(dbgColor__dGreen, "Powered LTEm on.\r\n");
        pDelay(5);
    }
    SC16IS7xx_start();     // start NXP SPI-UART bridge
}


int loopCnt = 0;
uint8_t testPattern;
int nullResponses = 0;

void loop() {
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
    *
    *  AT+GSN
    *         
    *  <IMEI value (20 char)>
    *
    *  OK
    */

    uint8_t regValue = 0;
    char cmd[] = "ATI\r\0";
    // char cmd[] = "AT+QPOWD=0\r\0";
    PRINTF(0, "Invoking cmd: %s \r\n", cmd);

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
        PRINTF(dbgColor__warn, "Received APP RDY from LTEm.\r\n");
    }
    if (strstr(response, "RDY"))
    {
        PRINTF(dbgColor__warn, "Received BG RDY from LTEm.\r\n");
    }
    if (strlen(response) == 0)
    {
        PRINTF(dbgColor__warn, "Got no response from BGx.\r\n");
        nullResponses++;
        if (nullResponses > 2)
            indicateFailure("BGx is not responding to cmds... failed."); 
    }

    if (strlen(response) > 40 )
    {
        if (strstr(response, validResponse))
        {
            tailAt = strstr(response, "OK\r\n");
            if (tailAt != NULL)
                PRINTF(0, "Got correctly formed response: \r\n%s", response);  
            else
                PRINTF(dbgColor__warn, "Missing final OK in response.\r");
        }
        else
            indicateFailure("Unexpected device information returned on cmd test... failed."); 
    }
    loopCnt ++;
    indicateLoop(loopCnt, random(1000));
}


/*
========================================================================================================================= */

#define ASCII_CR 13U

void sendCommand(const char* cmd)
{
    size_t sendSz = strlen(cmd);

    //SC16IS7xx_write(cmd, strlen(cmd));                      // normally you are going to use buffered writes like here

    for (size_t i = 0; i < sendSz; i++)
    {
        SC16IS7xx_writeReg(SC16IS7xx_FIFO_regAddr, cmd[i]);      // without a small delay the register is not moved to FIFO before next byte\char
        pDelay(1);                                              // this is NOT the typical write cycle
    }
    pDelay(300);                                                // max response time per-Quectel specs, for this test we will wait
}


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



/**
 *	\brief Validate the response ends in a BG96 OK value.
 *
 *	\param[in] response The command response received from the BG96.
 *  \return bool If the response string ends in a valid OK sequence
 */
bool validOkResponse(const char *response)
{
    #define BUFF_SZ 64
    #define EXPECTED_TERMINATOR_STR "OK\r\n"
    #define EXPECTED_TERMINATOR_LEN 4

    const char * end = (const char *)memchr(response, '\0', BUFF_SZ);
    if (end == NULL)
        end = response + BUFF_SZ;

    return strncmp(EXPECTED_TERMINATOR_STR, end - EXPECTED_TERMINATOR_LEN, EXPECTED_TERMINATOR_LEN) == 0;
}


/* test helpers
========================================================================================================================= */

//typedef void (*eventNotifFunc_t)(uint8_t notifType, uint8_t notifAssm, uint8_t notifInst, const char *notifMsg);

void appNotifyCB(uint8_t notifType, const char *notifMsg)
{
    if (notifType > 200)
    {
        PRINTF(dbgColor__error, "LQCloud-HardFault: %s\r", notifMsg);
        while (1) {}
    }
    PRINTF(dbgColor__info, "LQCloud Info: %s\r", notifMsg);
    return;
}


void indicateLoop(int loopCnt, int waitNext) 
{
    PRINTF(dbgColor__info, "Loop: %i \r\n", loopCnt);

    for (int i = 0; i < 6; i++)
    {
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        delay(50);
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        delay(50);
    }

    PRINTF(dbgColor__magenta, "Free memory: %u \r\n", getFreeMemory());
    PRINTF(0, "Next test in (millis): %i\r\n\r\n", waitNext);
    delay(waitNext);
}


void indicateFailure(char failureMsg[])
{
	PRINTF(dbgColor__error, "\r\n** %s \r\n", failureMsg);
    PRINTF(dbgColor__error, "** Test Assertion Failed. \r\n");

    int halt = 1;
    PRINTF(dbgColor__error, "** Halting Execution \r\n");
    while (halt)
    {
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        delay(1000);
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        delay(100);
    }
}

void initIO()
{
	// on Arduino, ensure pin is in default "logical" state prior to opening
	gpio_writePin(g_ltem.pinConfig.powerkeyPin, gpioValue_low);
	gpio_writePin(g_ltem.pinConfig.resetPin, gpioValue_low);
	gpio_writePin(g_ltem.pinConfig.spiCsPin, gpioValue_high);

	gpio_openPin(g_ltem.pinConfig.powerkeyPin, gpioMode_output);		// powerKey: normal low
	gpio_openPin(g_ltem.pinConfig.resetPin, gpioMode_output);			// resetPin: normal low
	gpio_openPin(g_ltem.pinConfig.spiCsPin, gpioMode_output);			// spiCsPin: invert, normal gpioValue_high

	gpio_openPin(g_ltem.pinConfig.statusPin, gpioMode_input);
	gpio_openPin(g_ltem.pinConfig.irqPin, gpioMode_inputPullUp);
}

/* Check free memory (stack-heap) 
 * - Remove if not needed for production
--------------------------------------------------------------------------------- */

#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__

int getFreeMemory() 
{
    char top;
    #ifdef __arm__
    return &top - reinterpret_cast<char*>(sbrk(0));
    #elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
    return &top - __brkval;
    #else  // __arm__
    return __brkval ? &top - __brkval : &top - __malloc_heap_start;
    #endif  // __arm__
}

