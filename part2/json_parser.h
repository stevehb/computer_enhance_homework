//
// Created by stevehb on 12-Jun-25.
//

#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <stdbool.h>

#include "common_funcs.h"
#include "types.h"

#define JSON_TYPES(X) \
    X(JSON_NONCE) \
    X(JSON_OBJECT_BEGIN) \
    X(JSON_OBJECT_END) \
    X(JSON_ARRAY_BEGIN) \
    X(JSON_ARRAY_END) \
    X(JSON_STRING) \
    X(JSON_NUMBER) \
    X(JSON_BOOL) \
    X(JSON_NULL)

#define ENUM_ENTRY(name) name,
typedef enum {
    JSON_TYPES(ENUM_ENTRY)
    JSON_TYPE_COUNT
 } JsonType;
#undef ENUM_ENTRY
#define STRING_ENTRY(name) #name,
static const char* JSON_TYPE_STRS[] = {
    JSON_TYPES(STRING_ENTRY)
 };
#undef STRING_ENTRY
#undef JSON_TYPES

typedef struct JsonElement JsonElement;

typedef struct JsonElement {
    JsonType type;
    u64 parentElementIdx;
    u64 nameOffset;

    union {
        struct {
            u64 startIdx, endIdx;
            u32 indentLevel;
            u64 childCount;
        } container;

        struct {
            u64 valueOffset;
        } string;

        struct {
            f64 value;
        } number;

        struct {
            bool value;
        } boolean;

        struct {
            void* null;
        } null;
    };
} JsonElement;


typedef struct JsonFile {
    char filename[FILENAME_LEN];
    u64 fileSize;

    JsonElement* elements;
    u64 elementCount;
    u64 elementCapacity;

    char* stringBuff;
    u64 stringBuffUsed;
    u64 stringBuffCapacity;
} JsonFile;



JsonFile json_parseFile(const char* filename);
char* json_getElementStr(JsonFile* file, JsonElement* el, char* out_buff, u32 buffLen);
void json_freeFile(JsonFile* file);

#endif //JSON_PARSER_H
