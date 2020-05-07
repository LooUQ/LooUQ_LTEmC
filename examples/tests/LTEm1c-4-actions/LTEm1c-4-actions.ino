/******************************************************************************
 *  \file LTEm1-test4-atcmd.ino
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


//#define USE_SERIAL 0

extern "C" {
#include <SEGGER_RTT.h>
}


const int APIN_RANDOMSEED = 0;

ltem1_pinConfig_t ltem1_pinConfig =
{
  spiCsPin : 13,
  irqPin : 12,
  statusPin : 6,
  powerkeyPin : 11,
  resetPin : 19,
  ringUrcPin : 5,
  wakePin : 10
};

spi_config_t ltem1_spiConfig = 
{
  dataRate : 2000000U,
  dataMode : spi_dataMode_0,
  bitOrder : spi_bitOrder_msbFirst,
  csPin : ltem1_pinConfig.spiCsPin
};

ltem1_device_t *ltem1;
//spi_device_t *spi; 


void setup() {
    #ifdef USE_SERIAL
        Serial.begin(115200);
        #if (USE_SERIAL)
        while (!Serial) {}
        #else
        delay(1000);
        #endif
    #endif

    PRINTF("LTEm1c test4-actions\r\n");
    gpio_openPin(LED_BUILTIN, gpioMode_output);
    
    randomSeed(analogRead(APIN_RANDOMSEED));

    ltem1_create(&ltem1_pinConfig, ltem1_functionality_atcmd);

    // install test version override of okCompleteParser(), verbose AT result codes
    // g_ltem1->atcmd->cmdCompleteParser_func = okCompletedParser;
}


int loopCnt = 0;

void loop() {
    /* BG96 test pattern: get mfg\model
    *
    *  ATI
    * 
    *   Quectel
    *   BG96
    *   Revision: BG96MAR04A02M1G
    * 
    *   OK
    */

    uint8_t regValue = 0;
    char cmd[] = "ATI\r\0";
    PRINTF("Invoking cmd: %s \r\n", cmd);

    action_invoke(cmd);

    // // wait for BG96 response in FIFO buffer
    // char cmdResponse[65] = {0};
    // recvResponse(cmdResponse);

    action_result_t atResult = action_awaitResult(g_ltem1->dAction);
    // atcmd_result_t atResult;
    // do
    // {
    //     atResult = atcmd_getResult(g_ltem1->atcmd);

    // } while (atResult == atcmd_result_pending);
    
    if (atResult == ACTION_RESULT_SUCCESS)    // statusCode == 200
    {
        PRINTF("Got %d chars\r", strlen(g_ltem1->dAction->resultHead));
        PRINTF("Resp: %s\r", g_ltem1->dAction->resultHead);  

        // test response v. expected 
        char* validResponse = "ATI\r\r\nQuectel";
        uint8_t responseTest = strncmp(validResponse, g_ltem1->dAction->resultHead, strlen(validResponse)); 

        if (responseTest != 0 || strlen(g_ltem1->dAction->resultHead) != 54)
            indicateFailure("Unexpected command response... failed."); 
    }
    else
    {
        PRINTF_ERR("atResult=%d \r", atResult);
        // indicateFailure("Unexpected command response... failed."); 
    }

    loopCnt ++;
    indicateLoop(loopCnt, random(1000));
}


/*
========================================================================================================================= */


// void sendCommand(const char* cmd)
// {
//     bool sendCompleted = false;
//     uint8_t retries;

//     while (!iop_txClearToSend() && retries < 500)
//     {
//         timing_delay(10);
//     }

//     if (iop_txClearToSend())
//     {
//         sendCompleted = iop_txSend(cmd, strlen(cmd));
//     }
//     if (sendCompleted)
//         PRINTF("CmdSent\r\n");
//     else
//         PRINTF_ERROR("Cmd Send Failed");   
// }



void recvResponse(char *response)
{
    iop_rxGetResult_t rxResult;
    uint8_t retries;
    do
    {
        rxResult = iop_rxGetCmdQueued(response, 65);
        timing_delay(100);
        retries++;
    } while (rxResult == iop_rx_result_nodata && retries < 5);
}



/**
 *	\brief Validate the response ends in a BG96 OK value.
 *
 *	\param[in] response The command response received from the BG96.
 *  \return bool If the response string ends in a valid OK sequence
 */
bool okCompletedParser(const char *response)
{
    #define OK_COMPLETED_STRING "OK\r\n"
    #define OK_COMPLETED_LENGTH 4

    // safe strlen()
    const char * tail = (const char *)memchr(response, '\0', ACTION_DEFAULT_RESULT_BUF_SZ);
    
    if (tail == NULL)
        tail = response + ACTION_DEFAULT_RESULT_BUF_SZ;

    return strncmp(OK_COMPLETED_STRING, tail - OK_COMPLETED_LENGTH, OK_COMPLETED_LENGTH) == 0;
}



/* test helpers
========================================================================================================================= */

void indicateFailure(char failureMsg[])
{
	PRINTF_ERR("\r\n** %s \r\n", failureMsg);
    PRINTF_ERR("** Test Assertion Failed. \r\n");

    #if 1
    PRINTF_ERR("** Halting Execution \r\n");
    bool halt = true;
    while (halt)
    {
        gpio_writePin(LED_BUILTIN, gpio_pinValue_t::gpioValue_high);
        timing_delay(1000);
        gpio_writePin(LED_BUILTIN, gpio_pinValue_t::gpioValue_low);
        timing_delay(100);
    }
    #endif
}


void indicateLoop(int loopCnt, int waitNext) 
{
    PRINTF_DBG("\r\nLoop=%i \r\n", loopCnt);

    for (int i = 0; i < 6; i++)
    {
        gpio_writePin(LED_BUILTIN, gpio_pinValue_t::gpioValue_high);
        timing_delay(50);
        gpio_writePin(LED_BUILTIN, gpio_pinValue_t::gpioValue_low);
        timing_delay(50);
    }

    PRINTF("FreeMem=%u\r\n", getFreeMemory());
    PRINTF("NextTest (millis)=%i\r\r", waitNext);
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

