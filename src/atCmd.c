// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "atCmd.h"

int atCmd_Invoke(const char * atCmd)
{
    // set resultCode = 1 (pending)
    // send "AT" as SPI 2-byte
    // send body buffer over SPI
    // set issuedAt milli count
}


int atCmd_GetResult(char* atResult)
{
    return pendingAtCommand.resultCode;
}


void atCmd_Cancel()
{
    pendingAtCommand.invokedAt = 0;
    pendingAtCommand.resultCode = -1;
    pendingAtCommand.cmd = NULL;
    pendingAtCommand.result = NULL;
}



