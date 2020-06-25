/******************************************************************************
 *  \file LTEm1-test3-iop.ino
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
 * The test3-iop.ino tests the LTEm1 interrupt driven Input-Ouput processing
 * subsystem in the driver which multiplexes the command and protocol streams.
 *****************************************************************************/


#include <ltem1c.h>

#define _DEBUG
#include "platform/platform_stdio.h"

const int APIN_RANDOMSEED = 0;

ltem1PinConfig_t ltem1_pinConfig =
{
  spiCsPin : 13,
  irqPin : 12,
  statusPin : 6,
  powerkeyPin : 11,
  resetPin : 19,
  ringUrcPin : 5,
  wakePin : 10
};

spiConfig_t ltem1_spiConfig = 
{
  dataRate : 2000000U,
  dataMode : spiDataMode_0,
  bitOrder : spiBitOrder_msbFirst,
  csPin : ltem1_pinConfig.spiCsPin
};

ltem1Device_t *ltem1;


void setup() {
    #ifdef USE_SERIAL
        Serial.begin(115200);
        #if (USE_SERIAL)
        while (!Serial) {}
        #else
        delay(1000);
        #endif
    #endif

    PRINTF(dbgColor_none, "LTEm1c test3-iop\r\n");          // same as color=0
    gpio_openPin(LED_BUILTIN, gpioMode_output);
    
    randomSeed(analogRead(APIN_RANDOMSEED));

    ltem1_create(&ltem1_pinConfig, ltem1Functionality_iop);
}


int loopCnt = 0;
#define RESPONSE_BUF_SZ 80                                  // set to < 91 to test truncated warning for long response

void loop() {
    /* BG96 test pattern: get IMEI
    *
    *  AT+GSN
    *         
    *  <IMEI value (20 char)>
    *
    *  OK
    */

    uint8_t regValue = 0;
    //char cmd[] = "AT+GSN\r\0";                            // short response (contained in 1 rxCtrlBlock)
    char cmd[] = "at+cgpaddr\r\0";                          // long response (contained in 2 rxCtrlBlocks)
    //char cmd[] = "AT+QPOWD\r\0";                          // something is wrong! Is is rx or tx (tx works if BG power down)
    PRINTF(0, "Invoking cmd: %s \r\n", cmd);

    sendCommand(cmd);
    //timing_delay(300);      // BG reference says 300mS cmd response time

    // wait for BG96 response in FIFO buffer
    char cmdResponse[RESPONSE_BUF_SZ + 1] = {0};
    recvResponse(cmdResponse, RESPONSE_BUF_SZ);

    // // test response v. expected 
    // char* validResponse = "AT+GSN\r\r\n86450";
    // uint8_t imeiPrefixTest = strncmp(validResponse, cmdResponse, strlen(validResponse)); 

    PRINTF(0, "Got %d chars\r", strlen(cmdResponse));
    PRINTF(0, "Resp: %s\r", cmdResponse);  

    // if (strlen(cmdResponse) == 11)
    //     PRINTF_INFO("BG started\r");
    // else if (cmdResponse[0] == '\0')
    //     PRINTF_WARN("No cmd response\r");
    // else if (imeiPrefixTest != 0 || strlen(cmdResponse) != 32)
    //     indicateFailure("Unexpected IMEI value response... failed."); 

    //delay(1000);

    loopCnt ++;
    indicateLoop(loopCnt, random(1000));
}


/*
========================================================================================================================= */


void sendCommand(const char* cmd)
{
    iop_txSend(cmd, strlen(cmd), false);
    
    PRINTF(0, "CmdSent\r\n");
}



void recvResponse(char *response, uint16_t responseBufSz)
{
    iopXfrResult_t rxResult;
    uint8_t retries;
    do
    {
        rxResult = iop_rxGetCmdQueued(response, responseBufSz);
        timing_delay(5);
        retries++;
    } while (rxResult == iopXfrResult_incomplete && retries < 100);

    if (rxResult == iopXfrResult_truncated)
        PRINTF(dbgColor_warn, "Command response truncated!\r");
}



/**
 *	\brief Validate the response ends in a BG96 OK value.
 *
 *	\param[in] response The command response received from the BG96.
 *  \return bool If the response string ends in a valid OK sequence
 */
// bool validOkResponse(const char *response)
// {
//     #define BUFF_SZ 64
//     #define EXPECTED_TERMINATOR_STR "OK\r\n"
//     #define EXPECTED_TERMINATOR_LEN 4

//     const char * end = (const char *)memchr(response, '\0', BUFF_SZ);
//     if (end == NULL)
//         end = response + BUFF_SZ;

//     return strncmp(EXPECTED_TERMINATOR_STR, end - EXPECTED_TERMINATOR_LEN, EXPECTED_TERMINATOR_LEN) == 0;
// }


/* test helpers
========================================================================================================================= */

void indicateFailure(char failureMsg[])
{
	PRINTF(dbgColor_error, "\r\n** %s \r", failureMsg);
    PRINTF(dbgColor_error, "** Test Assertion Failed. \r");

    #if 1
    PRINTF(dbgColor_error, "** Halting Execution \r");
    while (1)
    {
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        timing_delay(1000);
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        timing_delay(100);
    }
    #endif
}


void indicateLoop(int loopCnt, int waitNext) 
{
    PRINTF(dbgColor_info, "\r\nLoop=%i \r\n", loopCnt);

    for (int i = 0; i < 6; i++)
    {
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_high);
        timing_delay(50);
        gpio_writePin(LED_BUILTIN, gpioPinValue_t::gpioValue_low);
        timing_delay(50);
    }

    PRINTF(0, "FreeMem=%u\r\n", getFreeMemory());
    PRINTF(0, "NextTest (millis)=%i\r\r", waitNext);
    timing_delay(waitNext);
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

