// Copyright (c) 2020 LooUQ Incorporated.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

//#define _DEBUG
#include "ltem1c.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))


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


/**
 *  \brief Parses the topic properties string into a mqttProperties structure (MUTATES, NO COPY).
 * 
 *  \param [in] propsSrc - Char pointer to the c-string containing message properties passed in topic 
 * 
 *  \return Struct with pointer arrays to the properties (name/value)
*/
propsDict_t util_parseStringToPropsDict(char *propsSrc)
{
    propsDict_t result = {0, {0}, {0}};

    if (strlen(propsSrc) == 0)
        return result;
    
    char *next = propsSrc;
    char *delimAt;
    char *endAt = propsSrc + strlen(propsSrc);

    for (size_t i = 0; i < MQTT_PROPERTIES_CNT; i++)                        // 1st pass; get names + values
    {
        delimAt = memchr(propsSrc, '&', endAt - propsSrc);
        delimAt = (delimAt == NULL) ? endAt : delimAt;

        result.names[i] = propsSrc;
        *delimAt = ASCII_cNULL;
        propsSrc = delimAt + 1;
        result.count = i;
        if (delimAt == endAt)
            break;
    }
    result.count++;
    
    for (size_t i = 0; i < result.count; i++)                               // 2nd pass; split names/values
    {
        delimAt = memchr(result.names[i], '=', endAt - result.names[i]);
        if (delimAt == NULL)
        {
            result.count = i;
            break;
        }
        *delimAt = ASCII_cNULL;
        result.values[i] = delimAt + 1;
    }
    return result;
}


/**
 *  \brief Scans the qryProps struct for the a prop and returns the value.
 * 
 *  \param [in] propsName - Char pointer to the c-string containing message properties passed in topic 
 * 
 *  \return Struct with pointer arrays to the properties (name/value)
*/
char *util_getPropValue(const char *propName, propsDict_t props)
{
    for (size_t i = 0; i < props.count; i++)
    {
        if (strcmp(props.names[i], propName) == 0)
            return props.values[i];
    }
    return NULL;
}


// /**
//  *	\brief Safe string length, limits search for NULL char to maxSz.
//  *
//  *  \param [in] charStr - Pointer to character string to search for its length.
//  *  \param [in] maxSz - The maximum number of characters to search.
//  *
//  *  \return AT command control structure.
//  */
// const char *strlenSafe(const char *charStr, uint16_t maxSz)
// {
//     return memchr(charStr, '\0', maxSz);
// }




/**
*	\brief Simple string to packed numeric IP address parser.
*/
// ip_addr_t util_parseIpAddr(const char *ipStr)
// {
//     char *nextOct;
//     bool ipValid = false;
//     ip_addr_t result;

//     for (uint8_t i = 0; i < 4; i++)
//     {
//         result.octet[i] = (uint8_t)strtol(ipStr, &nextOct, 10);
//         if (i == 0 && result.octet[i] == 0)
//             break;
//         if (i < 3 && *nextOct != '.' )
//             break;
//         if (i == 3)
//             ipValid = true;
//     }

//     if (!ipValid) 
//         result.packed = 0;
//     return result;
// }



// /**
// *  \brief C99 implementation of float to c-string formatter.
// *
// *  \param[in] fVal The float value to convert.
// *  \param[in/out] buf The character buffer to place formatted number in.
// *  \param[in] bufSz The size of the passed char buffer.
// *  \param[in] precision The number of digits to the right of the decimal.
// */
// void floatToString(float fVal, char *buf, uint8_t bufSz, uint8_t precision)
// {
//     // calc max: the largest possible conversion result
//     // if fval > max: conversion with overflow
//     uint32_t max = pow(10, (bufSz - precision - 3));    //need room for '-', '.' and '\0'
//     if (abs(fVal) >= max)
//     {
//         *buf = '\0';
//         return;
//     }

//     int iVal = (int)fVal;
//     itoa(iVal, buf, 10);            // got sign and integer

//     fVal = fVal > 0 ? fVal : fVal * -1;
//     iVal = iVal > 0 ? iVal : iVal * -1;
//     uint8_t pos = strlen(buf);
//     buf[pos++] = '.';

//     for (size_t i = 0; i < precision; i++)
//     {
//         fVal -= (float)iVal;        // hack off the whole part of the number
//  		fVal *= 10;                 // move next digit over
//  		iVal = (int)fVal;
//         char c = (char)iVal + 0x30;
//  		buf[pos++] = c;
//     }
//     buf[pos++] = '\0';
// }



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

