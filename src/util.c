/* --------------------------------------------------------------------------------------------- */

#include "ltem1c.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))


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



/**
*  \brief C99 implementation of float to c-string formatter.
*
*  \param[in] fVal The float value to convert.
*  \param[in/out] buf The character buffer to place formatted number in.
*  \param[in] bufSz The size of the passed char buffer.
*  \param[in] precision The number of digits to the right of the decimal.
*/
void floatToString(float fVal, char *buf, uint8_t bufSz, uint8_t precision)
{
    uint32_t max = pow(10, (bufSz - precision - 3));    //remember '-', '.' and '\0'
    if (abs(fVal) >= max)
    {
        *buf = '\0';
        return;
    }

    int iVal = (int)fVal;
    itoa(iVal, buf, 10);

    fVal = abs(fVal);
    iVal = abs(iVal);
    uint8_t pos = strlen(buf);
    buf[pos++] = '.';

    for (size_t i = 0; i < precision; i++)
    {
        fVal -= (float)iVal;      // hack off the whole part of the number
 		fVal *= 10;           // move next digit over
 		iVal = (int)fVal;
        char c = (char)iVal + 0x30;
 		buf[pos++] = c;
    }
    buf[pos++] = '\0';
}


/**
*  \brief C99 thread-safe implementation of c-string token extractor.
*
*  \param[in] source The source to extract tokens from.
*  \param[in] delimeter The character delimeter as an integer.
*  \param[in] token Buffer containing the token.
*  \param[in] tokenMax The maximum size for the extracted token.
*
*  \return Pointer to the continue point of tokenization.
*/
char *strToken(char *source, int delimiter, char *token, uint8_t tokenMax) 
{
    char *delimAt;
    if (source == NULL)
        return false;

    delimAt = strchr(source, delimiter);
    uint8_t tokenSz = delimAt - source;
    if (tokenSz == 0)
        return NULL;

    memset(token, 0, tokenMax);
    strncpy(token, source, MIN(tokenSz, tokenMax-1));
    return delimAt + 1;
}




