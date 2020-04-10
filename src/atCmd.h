// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef _ATCMD_H_
#define _ATCMD_H_

#include "ltem1c.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct at_command_tag
{
  char *cmd;
  char *result;
  int resultCode;
  unsigned long invokedAt;
  int timeoutMillis;
  char *(*cmdresult_parser_func)();
} at_command_t;


void atcmd_invoke(const char *atCmd);
void atcmd_invokeAdv(const char *atCmd, uint8_t timeoutMillis,  char *responseBuf, uint8_t responseBufSz, char *(*cmdresult_parser_func)());
uint8_t atcmd_getResult(char *atResult);
void atcmd_cancel();


#ifdef __cplusplus
}
#endif

#endif  //!_ATCMD_H_
