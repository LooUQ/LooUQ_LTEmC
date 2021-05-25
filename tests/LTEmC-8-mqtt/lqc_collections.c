#include "lqc_collections.h"


#pragma region Local Static Function Declarations
static uint16_t findJsonBlockLength(const char *blockStart, const char *jsonEnd, char blockOpen, char blockClose);
#pragma endregion


/**
 *  \brief Parses a HTTP style query string (key and value pairs), creating a dictionary overlay for the keys and values. 
 * 
 *  \param dictSrc [in] - Char pointer to the c-string containing key value pairs to identify. 
 *  NOTE: the source is mutated in the process, keys/values are NULL term'd.
 * 
 *  \return Struct with map (pointers) to the keys and values (within the source array)
*/
keyValueDict_t lqc_createDictFromQueryString(char *dictSrc)
{
    keyValueDict_t result = {0};
    //keyValueDict_t result = {0, 0, {0}, {0}};

    result.length = strlen(dictSrc);
    if (result.length == 0)
        return result;
    
    char *next = dictSrc;
    char *delimAt;
    char *endAt = dictSrc + result.length;

    for (size_t i = 0; i < KEYVALUE_DICT_SZ; i++)                        // 1st pass; get names + values
    {
        delimAt = memchr(dictSrc, '&', endAt - dictSrc);
        delimAt = (delimAt == NULL) ? endAt : delimAt;

        result.keys[i] = dictSrc;
        *delimAt = '\0';
        dictSrc = delimAt + 1;
        result.count = i;
        if (delimAt == endAt)
            break;
    }
    result.count++;
    
    for (size_t i = 0; i < result.count; i++)                               // 2nd pass; split names/values
    {
        delimAt = memchr(result.keys[i], '=', endAt - result.keys[i]);
        if (delimAt == NULL)
        {
            result.count = i;
            break;
        }
        *delimAt = '\0';
        result.values[i] = delimAt + 1;
    }
    return result;
}


/**
 *  \brief Scans the qryProps struct for the a prop and returns the value from the underlying char array.
 * 
 *  \param key [in] - Char pointer to the c-string to locate in the dictionary 
 *  \param dict [in] - Dictionary to be searched 
 * 
 *  \return Struct with pointer arrays to the properties (name/value)
*/
char *lqc_getDictValue(const char *key, keyValueDict_t dict)
{
    if (dict.keys[0] == NULL || dict.keys[0][0] == '\0')        // underlying char array is invalid
        return NULL;

    for (size_t i = 0; i < dict.count; i++)
    {
        if (strcmp(dict.keys[i], key) == 0)
            return dict.values[i];
    }

    return NULL;
}



/**
 *  \brief Scans a JSON formatted C-String (char array) for a property, once found a descriptive struct is populated with info to allow for property value consumption.
 * 
 *  \param [in] jsonSrc - Char array containing the JSON document.
 *  \param [in] propName - The name of the property you are searching for.
 * 
 *  \return Struct with a pointer to property value, a property type (enum) and the len of property value.
*/
lqcJsonPropValue_t lqc_getJsonPropValue(const char *jsonSrc, const char *propName)
{
    lqcJsonPropValue_t results = {0, 0, lqcJsonPropType_notFound};
    char *jsonEnd = (char*)jsonSrc + strlen(jsonSrc);
    char *next;

    char propSearch[40] = {0};
    uint8_t nameSz = strlen(propName);

    memcpy(propSearch + 1, propName, nameSz);
    propSearch[0] = '"';
    propSearch[nameSz + 1] = '"';

    char *nameAt = strstr(jsonSrc, propSearch);
    if (nameAt)
    {
        next = nameAt + nameSz;
        next = (char*)memchr(next, ':', jsonEnd - next);
        if (next)
        {
            next++;
            while (*next == '\040' || *next == '\011' )   // skip space or tab
            {
                next++;
                if (next >= jsonEnd)
                    return results;
            }
            switch (*next)
            {
            case '{':
                results.type = lqcJsonPropType_object;
                results.value = next;
                results.len = findJsonBlockLength(next, jsonEnd, '{', '}');
                return results;
            case '[':
                results.type = lqcJsonPropType_array;
                results.value = next;
                results.len = findJsonBlockLength(next, jsonEnd, '[', ']');
                return results;
            case '"':
                results.type = lqcJsonPropType_text;
                results.value = (char*)++next;
                results.len = (char*)memchr(results.value, '\042', jsonEnd - next) - results.value;    // dblQuote = \042
                return results;
            case 't':
                results.type = lqcJsonPropType_bool;
                results.value = (char*)next;
                results.len = 4;
                return results;
            case 'f':
                results.type = lqcJsonPropType_bool;
                results.value = (char*)next;
                results.len = 5;
                return results;
            case 'n':
                results.type = lqcJsonPropType_null;
                results.value = (char*)next;
                results.len = 4;
                return results;
            default:
                results.type = lqcJsonPropType_int;
                results.value = (char*)next;
                while (*next != ',' && *next != '}' && next < jsonEnd)   // scan forward until beyond current property
                {
                    next++;
                    if (*next == '.') { results.type = lqcJsonPropType_float; }
                }
                results.len = next - results.value;
                return results;
            }
        }
    }
    return results;
}


#pragma region Static Local Functions

/**
 *  \brief STATIC Scope: Local function to determine the length of a JSON object or array. Used by lqc_getJsonPropValue().
 * 
 *  \param [in] blockStart - Pointer to the starting point for the scan.
 *  \param [in] jsonEnd - End of the original JSON formatted char array.
 *  \param [in] blockOpen - Character marking the start of the block being sized, used to identify nested blocks.
 *  \param [in] blockClose - Character marking the end of the block being sized (including nested).
 * 
 *  \return The size of the block (object\array) including the opening and closing marking chars.
*/
static uint16_t findJsonBlockLength(const char *blockStart, const char *jsonEnd, char blockOpen, char blockClose)
{
    uint8_t openPairs = 1;
    char * next = (char*)blockStart;

    while (openPairs > 0 && next < jsonEnd)   // scan forward until . or beyond current property
    {
        next++;
        if (*next == blockOpen) openPairs++;
        if (*next == blockClose) openPairs--;
   }
   return (next - blockStart + 1);
}

#pragma endregion

