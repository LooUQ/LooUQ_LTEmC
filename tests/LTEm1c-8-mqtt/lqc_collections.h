/******************************************************************************
 *  \file lqc_collections.h
 *  \author Greg Terrell
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
 ******************************************************************************
 * Data collection functions used by LQ Cloud
 *****************************************************************************/

#ifndef __LQC_COLLECTIONS_H__
#define __LQC_COLLECTIONS_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define KEYVALUE_DICT_SZ 15

/** 
 *  \brief Struct exposing action's parameters collection (names and values as c-strings).
 * 
 *  NOTE: This struct maps key\value pairs in an existing HTTP query string formated char array. The array is parsed
 *  using the lqc_parseQueryStringDict() function. Parsing MUTATES the original char array. The original char array
 *  must stay in scope as it contains the keys and the values. The keyValueDict struct only provides a higher level
 *  map of the data stored in the char array and a utility function lqc_getDictValue(key) to access the value.
 * 
 *  If copy (ex: free source) of underlying char array needed: memcpy(destination_ptr, keys[0], length)
*/
typedef struct keyValueDict_tag
{
    uint8_t count;                      ///< During parsing, how many properties (name/value pairs) were mapped.
    uint16_t length;                    ///< Underlying char array original length, use if copy needed to free source
    char *keys[KEYVALUE_DICT_SZ];       ///< Array of property keys.
    char *values[KEYVALUE_DICT_SZ];     ///< Array of property values (as c-strings). Application is responsible for any type conversion.
} keyValueDict_t;


typedef enum lqcJsonPropType_tag
{
    lqcJsonPropType_notFound = 0,
    lqcJsonPropType_object = 1,
    lqcJsonPropType_array = 2,
    lqcJsonPropType_text = 3,
    lqcJsonPropType_bool = 4,
    lqcJsonPropType_int = 5,
    lqcJsonPropType_float = 6,
    lqcJsonPropType_null = 9
} lqcJsonPropType_t;


typedef struct lqcJsonPropValue_tag
{
    char *value;
    uint16_t len;
    lqcJsonPropType_t type;
} lqcJsonPropValue_t;



#ifdef __cplusplus
extern "C"
{
#endif


// Query String Dictionary
keyValueDict_t lqc_createDictFromQueryString(char *dictSrc);
char *lqc_getDictValue(const char *key, keyValueDict_t dict);

// JSON (body) Documents
lqcJsonPropValue_t lqc_getJsonPropValue(const char *jsonSrc, const char *propName);

// // String Tokenizer
// char *lqc_strToken(char *source, int delimiter, char *token, uint8_t tokenMax);


#ifdef __cplusplus
}
#endif // !__cplusplus



#endif  /* !__LQCLOUD_H__ */
