/******************************************************************************
 *  \file cr_lte_modem.h
 *  \author Jensen Miller
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
 *****************************************************************************/
#ifndef __LOOUQ_CIRCUITRIVER_LTE_MODEM_H__
#define __LOOUQ_CIRCUITRIVER_LTE_MODEM_H__

#ifdef __cplusplus
extern "C"
{
#include <cstdint>
#include <cstdlib>
#include <cstdbool>
#else
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#endif // __cplusplus


#define LTEMDM_SPI_CLOCK_FREQ_HZ	2000000U


typedef struct
{
    int chip_sel_line;
    int irq_line;
    int status_pin;
    int power_key;
    int reset_pin;
    int connected_pin;
} lte_modem_config_t;


typedef struct cr_lte_modem_tag* lte_modem;


lte_modem ltemdm_init(const lte_modem_config_t* ltemdm_config);
void ltemdm_uninit(lte_modem modem);



extern lte_modem_config_t FEATHER_BREAKOUT;
extern lte_modem_config_t RPI_BREAKOUT;

#ifdef __cplusplus
}
#endif // !__cplusplus



#endif  /* !__LOOUQ_CIRCUITRIVER_LTE_MODEM_H__ */