
#include "platform_stdio.h"

extern "C" {
#include <SEGGER_RTT.h>
}


void dbg_printf(uint8_t color, const char *fmt, ...)
{
    char buf[180] = {0};
    va_list args;
    va_start(args, fmt);

    vsnprintf(buf, sizeof(buf), fmt, args);

    //int SEGGER_RTT_vprintf(unsigned BufferIndex, const char * sFormat, va_list * paramList);

    switch (color)
    {
        case dbgColor_none:
            SEGGER_RTT_WriteString(0, RTT_CTRL_TEXT_WHITE);
            break;
        case dbgColor_info:
            SEGGER_RTT_WriteString(0, RTT_CTRL_TEXT_BRIGHT_GREEN);
            break;
        case dbgColor_warn:
            SEGGER_RTT_WriteString(0, RTT_CTRL_TEXT_BRIGHT_YELLOW);
            break;
        case dbgColor_error:
            SEGGER_RTT_WriteString(0, RTT_CTRL_TEXT_BRIGHT_RED);
            break;

        case dbgColor_cyan:
            SEGGER_RTT_WriteString(0, RTT_CTRL_TEXT_BRIGHT_CYAN);
            break;
        case dbgColor_magenta:
            SEGGER_RTT_WriteString(0, RTT_CTRL_TEXT_BRIGHT_MAGENTA);
            break;
        case dbgColor_white:
            SEGGER_RTT_WriteString(0, RTT_CTRL_TEXT_BRIGHT_WHITE);
            break;
        case dbgColor_gray:
            SEGGER_RTT_WriteString(0, RTT_CTRL_TEXT_BRIGHT_BLACK);
            break;
        case dbgColor_blue:
            SEGGER_RTT_WriteString(0, RTT_CTRL_TEXT_BRIGHT_BLUE);
            break;
            
        case dbgColor_dCyan:
            SEGGER_RTT_WriteString(0, RTT_CTRL_TEXT_CYAN);
            break;
        case dbgColor_dMagenta:
            SEGGER_RTT_WriteString(0, RTT_CTRL_TEXT_MAGENTA);
            break;
        case dbgColor_dGreen:
            SEGGER_RTT_WriteString(0, RTT_CTRL_TEXT_GREEN);
            break;

        default:
            SEGGER_RTT_WriteString(0, RTT_CTRL_TEXT_WHITE);
            break;
    }

    SEGGER_RTT_WriteString(0, buf);
    SEGGER_RTT_WriteString(0, RTT_CTRL_TEXT_WHITE);

    va_end(args);
}
