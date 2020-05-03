/* --------------------------------------------------------------------------------------------- */

#include "ltem1c.h"

/**
*	\brief Simple string to packed numeric IP address parser.
*/
ip_addr_t atcmd_parseIpAddr(const char *ipStr)
{
    char *nextOct;
    bool ipValid = false;
    ip_addr_t result;

    for (uint8_t i = 0; i < 4; i++)
    {
        result.octet[i] = (uint8_t)strtol(ipStr, &nextOct, 10);
        if (i == 0 && result.octet[i] == 0)
            break;
        if (i < 3 && *nextOct != '.' )
            break;
        if (i == 3)
            ipValid = true;
    }

    if (!ipValid) 
        result.packed = 0;
    return result;
}

// void atcmd_displayIpAddr(ip_addr_t ipAddr, char *ipDisplay)
// {
//     char ipDisplay[16];
//     sprintf(str, "%d.%d.%d.%d", ip_addr.octet[0], ip_addr.octet[1], ip_addr.octet[2], ip_addr.octet[3]);
// }

