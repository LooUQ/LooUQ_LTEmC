// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef _ATCMD_H_
#define _ATCMD_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AtCommand_t
{
  char* cmd;
  char* result;
  int resultCode;
  unsigned long invokedAt;
  int timeoutMillis;
} pendingAtCommand;

int atCmd_Invoke(const char * atCmd);
int atCmd_GetResult(char* atResult);
void atCmd_Cancel();

#ifdef __cplusplus
}
#endif

#endif  //!_ATCMD_H_
