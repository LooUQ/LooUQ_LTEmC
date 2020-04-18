
#include "platform_stdio.h"

extern "C" {
#include <SEGGER_RTT.h>
}

#ifdef __SEGGER_RTT

//int SEGGER_RTT_vprintf(unsigned BufferIndex, const char * sFormat, va_list * paramList);

void _dbg_printf(print_color_t color, const char *fmt, ...)
{
    // va_list paramList;

    // va_start(paramList, fmt);
    // SEGGER_RTT_vprintf(0, fmt, &paramList);

    char buf[120] = {0};
    va_list args;
    va_start(args, fmt);

    vsnprintf(buf, sizeof(buf), fmt, args);

    switch (color)
    {
        case debug_print_color_none:
            SEGGER_RTT_WriteString(0, RTT_CTRL_TEXT_WHITE);
            break;
        case debug_print_color_info:
            SEGGER_RTT_WriteString(0, RTT_CTRL_TEXT_BRIGHT_GREEN);
            break;
        case debug_print_color_warn:
            SEGGER_RTT_WriteString(0, RTT_CTRL_TEXT_BRIGHT_YELLOW);
            break;
        case debug_print_color_error:
            SEGGER_RTT_WriteString(0, RTT_CTRL_TEXT_BRIGHT_RED);
            break;
        default:
            SEGGER_RTT_WriteString(0, RTT_CTRL_TEXT_WHITE);
            break;
    }
    SEGGER_RTT_WriteString(0, buf);
    SEGGER_RTT_WriteString(0, RTT_CTRL_TEXT_WHITE);
    va_end(args);
   
}

#endif
