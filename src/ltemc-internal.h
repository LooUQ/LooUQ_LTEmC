/******************************************************************************
 *  \file ltemc-internal.h
 *  \author Greg Terrell, Jensen Miller
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
 ******************************************************************************
 * Internal define and type definitions for LTEmC modules.
 *****************************************************************************/

#ifndef __LTEMC_INTERNAL_H__
#define __LTEMC_INTERNAL_H__


#include <lq-types.h>
#include <lq-diagnostics.h>

#include "ltemc-types.h"
#include "ltemc-srcfiles.h"

#include "lq-platform.h"
#include "ltemc-nxp-sc16is.h"
#include "ltemc-quectel-bg.h"
#include "ltemc-iop.h"

#include "ltemc-atcmd.h"
#include "ltemc-mdminfo.h"
#include "ltemc-network.h"
#include "ltemc-streams.h"

/* optional services not defined/included globally, application using the provided services should add to their application files 
#include "ltemc-sckt.h"
#include "ltemc-mqtt.h"
#include "ltemc-http.h"
#include "ltemc-gnss.h"
#include "ltemc-geo.h"
*/

#endif  /* !__LTEMC_INTERNAL_H__ */