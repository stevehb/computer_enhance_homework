//
// Created by stevehb on 12-Jun-25.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common_funcs.h"
#include "json_parser.h"

#include <assert.h>
#include <math.h>

#define JSON_TOKENS(X) \
    X(TOK_LBRACE) \
    X(TOK_RBRACE) \
    X(TOK_LBRACKET) \
    X(TOK_RBRACKET) \
    X(TOK_COLON) \
    X(TOK_COMMA) \
    X(TOK_WHITESPACE) \
    X(TOK_STRING) \
    X(TOK_NUMBER) \
    X(TOK_BOOL_FALSE) \
    X(TOK_BOOL_TRUE) \
    X(TOK_NULL)
#define ENUM_ENTRY(name) name,
typedef enum {
    JSON_TOKENS(ENUM_ENTRY)
    TOK_COUNT
} JsonToken;
#undef ENUM_ENTRY
#define STRING_ENTRY(name) #name,
static const char* TOKEN_STRS[] = {
    JSON_TOKENS(STRING_ENTRY)
};
#undef STRING_ENTRY
#undef JSON_TOKENS


static u64 json_getLenWhile(FileState* state, char* validChars);
static u64 json_getLenUntil(FileState* state, char* delims);
static u64 json_addElement(JsonFile* file, JsonElement element);
static char* json_ingestString(FileState* state, JsonFile* file);
static u64 json_consumeWhitespace(FileState* state);
static JsonToken json_getToken(char c);

static u64 json_getLenWhile(FileState* state, char* validChars) {
    u64 startPos = state->position;
    u64 idx = startPos;
    while (idx < state->size) {
        char c = state->data[idx];
        bool found = false;
        for (char* v = validChars; *v != '\0'; v++) {
            if (*v == c) {
                found = true;
                break;
            }
        }
        if (!found) {
            return idx - startPos;
        }
        idx++;
    }
    return idx - startPos;
}
static u64 json_getLenUntil(FileState* state, char* delims) {
    u64 startPos = state->position;
    u64 idx = startPos;
    char prevC = '\0';
    while (idx < state->size) {
        char c = state->data[idx];
        for (char* d = delims; *d != '\0'; d++) {
            if (*d == c) {
                if (*d == '"' && prevC == '\\') {
                    // Escaped quotation mark should not match a quotation mark delimiter
                    continue;
                }
                return idx - startPos;
            }
        }
        prevC = c;
        idx++;
    }
    return idx - startPos;
}
static u64 json_addElement(JsonFile* file, JsonElement element) {
    if (file->elementCount + 1 > file->elementCapacity) {
        u64 newCapacity = file->elementCapacity == 0 ? 32 : (file->elementCapacity * 2);
        u64 newSize = newCapacity * sizeof(JsonElement);
        // printf("Realloc JSON elements: oldCapacity=%llu bytes, newCapacity=%llu bytes\n", file->elementCapacity * sizeof(JsonElement), newCapacity * sizeof(JsonElement));
        JsonElement* newElems = realloc(file->elements, newSize);
        if (newElems == NULL) {
            fprintf(stderr, "ERROR: Memory re-alloc failed for %llu bytes\n", newSize);
            exit(1);
        }
        file->elements = newElems;
        file->elementCapacity = newCapacity;
    }
    file->elements[file->elementCount++] = element;
    return file->elementCount - 1;
}
static char* json_ingestString(FileState* state, JsonFile* file) {
    u64 strLen = json_getLenUntil(state, "\"");
    u64 minCapacity = file->stringBuffUsed + strLen + 1;
    if (minCapacity > file->stringBuffCapacity) {
        u64 newCapacity = file->stringBuffCapacity == 0 ? 1024 : (file->stringBuffCapacity * 2);
        while (newCapacity < minCapacity) {
            newCapacity *= 2;
        }
        // printf("Realloc string buffer: oldCapacity=%llu, newCapacity=%llu\n", file->stringBuffCapacity, newCapacity);
        char* newBuff = realloc(file->stringBuff, newCapacity);
        if (newBuff == NULL) {
            fprintf(stderr, "ERROR: Memory re-alloc failed for %llu bytes\n", newCapacity);
            exit(1);
        }
        file->stringBuff = newBuff;
        file->stringBuffCapacity = newCapacity;
    }

    u64 buffIdx = file->stringBuffUsed;
    memcpy(file->stringBuff + buffIdx, state->data + state->position, strLen);
    file->stringBuff[buffIdx + strLen] = '\0';
    file->stringBuffUsed += strLen + 1;
    state->position += strLen + 1;  // Consume closing quotation mark
    return &file->stringBuff[buffIdx];
}
static f64 json_ingestNumber(FileState* state, JsonFile* file) {
    u64 numLen = json_getLenWhile(state, "-.0123456789");
    if (numLen > 64) {
        fprintf(stderr, "ERROR: Number string is too big. expected less than 64, found %llu\n", numLen);
        exit(1);
    }
    char numStr[64] = { 0 };
    memcpy(numStr, state->data + state->position, numLen);
    errno = 0;
    char* endPtr = NULL;
    f64 result = strtod(numStr, &endPtr);
    if (errno == ERANGE || endPtr == numStr || *endPtr != '\0') {
        fprintf(stderr, "ERROR: Failed to convert string to f64: errno=%s\n", strerror(errno));
        fprintf(stderr, "    str: %s\n", numStr);
        exit(1);
    }
    state->position += numLen;
    return result;
}
static u64 json_consumeWhitespace(FileState* state) {
    u64 wsLen = json_getLenWhile(state, " \t\r\n");
    state->position += wsLen;
    return wsLen;
}
static JsonToken json_getToken(char c) {
    JsonToken tok = TOK_COUNT;
    switch (c) {

    case '{': tok = TOK_LBRACE; break;
    case '}': tok = TOK_RBRACE; break;
    case '[': tok = TOK_LBRACKET; break;
    case ']': tok = TOK_RBRACKET; break;
    case ':': tok = TOK_COLON; break;
    case ',': tok = TOK_COMMA; break;

    case 't': tok = TOK_BOOL_TRUE; break;
    case 'f': tok = TOK_BOOL_FALSE; break;
    case 'n': tok = TOK_NULL; break;

    case '"': tok = TOK_STRING; break;

    case '.':
    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        tok = TOK_NUMBER;
        break;

    case ' ':
    case '\n':
    case '\t':
    case '\r':
        tok = TOK_WHITESPACE;
        break;
    default:
        fprintf(stderr, "ERROR: Non-token character '%c' (0x%X)\n", c, (int) c);
        exit(0);
    }
    return tok;
}

JsonFile json_parseFile(const char* filename) {
    JsonFile file = { 0 };
    FileState state = mmapFile(filename);
    strncpy(file.filename, filename, FILENAME_LEN);
    file.fileSize = state.size;

    state.position = 0;
    u64 currentParentIdx = 0;
    JsonElement pendingEl = { 0 };
    bool hasPending = false;
    u32 indentLevel = 0;
    while (state.position < state.size) {
        char c = state.data[state.position++];
        JsonToken tok = json_getToken(c);

        bool isEndOfPending = tok == TOK_COMMA || tok == TOK_RBRACE || tok == TOK_RBRACKET;
        if (isEndOfPending && hasPending) {
            pendingEl.parentElementIdx = currentParentIdx;
            file.elements[currentParentIdx].container.childCount++;
            json_addElement(&file, pendingEl);
            pendingEl = (JsonElement){ 0 };
            hasPending = false;
        }

        u64 nextInsertIdx = file.elementCount;
        switch (tok) {
        case TOK_LBRACE:
        case TOK_LBRACKET: {
            pendingEl.type = (tok == TOK_LBRACE) ? JSON_OBJECT_BEGIN : JSON_ARRAY_BEGIN;
            pendingEl.parentElementIdx = currentParentIdx;
            pendingEl.container.startIdx = nextInsertIdx;
            pendingEl.container.indentLevel = indentLevel;
            currentParentIdx = nextInsertIdx;
            indentLevel++;

            json_addElement(&file, pendingEl);
            pendingEl = (JsonElement){ 0 };
            hasPending = false;
        } break;

        case TOK_RBRACE:
        case TOK_RBRACKET: {
            JsonElement* startEl = &file.elements[currentParentIdx];
            pendingEl.type = (tok == TOK_RBRACE) ? JSON_OBJECT_END : JSON_ARRAY_END;
            pendingEl.parentElementIdx = startEl->parentElementIdx;
            pendingEl.name = startEl->name;
            pendingEl.container.startIdx = startEl->container.startIdx;
            pendingEl.container.endIdx = nextInsertIdx;
            pendingEl.container.indentLevel = startEl->container.indentLevel;
            startEl->container.endIdx = pendingEl.container.endIdx;
            currentParentIdx = startEl->parentElementIdx;
            // Root element should not count itself as a child
            if (pendingEl.parentElementIdx != pendingEl.container.startIdx) {
                file.elements[currentParentIdx].container.childCount++;
            }
            indentLevel--;

            json_addElement(&file, pendingEl);
            pendingEl = (JsonElement){ 0 };
            hasPending = false;
        } break;

        case TOK_COLON: {
            // NOOP
        } break;

        case TOK_COMMA: {
            // NOOP
        } break;

        case TOK_WHITESPACE: {
            json_consumeWhitespace(&state);
        } break;

        case TOK_STRING: {
            JsonElement* parentEl = &file.elements[currentParentIdx];
            bool needsName = parentEl->type == JSON_OBJECT_BEGIN;
            bool isName = needsName && pendingEl.name == NULL;
            if (isName) {
                pendingEl.name = json_ingestString(&state, &file);
            } else {
                pendingEl.type = JSON_STRING;
                pendingEl.string.value = json_ingestString(&state, &file);
            }
            hasPending = true;
        } break;

        case TOK_NUMBER: {
            pendingEl.type = JSON_NUMBER;
            state.position--;
            pendingEl.number.value = json_ingestNumber(&state, &file);
            hasPending = true;
        } break;

        case TOK_BOOL_FALSE:
        case TOK_BOOL_TRUE: {
            pendingEl.type = JSON_BOOL;
            pendingEl.boolean.value = (tok == TOK_BOOL_TRUE) ? 1 : 0;
            state.position += json_getLenWhile(&state, "truefalse");
            hasPending = true;
        } break;

        case TOK_NULL: {
            pendingEl.type = JSON_NULL;
            hasPending = true;
        } break;

        case TOK_COUNT:
            fprintf(stderr, "ERROR: Reached bad token at %llu\n", state.position);
            exit(1);
        }
    }

    munmapFile(&state);
    return file;
}
char* json_getElementStr(JsonElement* el, char* buff, u32 buffLen) {
    char typeStr[32] = { 0 };
    snprintf(typeStr, 32, "%20s ", JSON_TYPE_STRS[el->type]);

    char namePrefix[64] = { 0 };
    if (el->name != NULL) {
        snprintf(namePrefix, 64, "\"%s\": ", el->name);
    }

    char valueStr[128] = { 0 };
    switch (el->type) {
    case JSON_NONCE: {
        // NOOP
    } break;
    case JSON_OBJECT_BEGIN: {
        snprintf(valueStr, 128, "%*s childCount=%llu", (el->container.indentLevel*4), "{", el->container.childCount);
    } break;
    case JSON_OBJECT_END: {
        snprintf(valueStr, 128, "%*s", (el->container.indentLevel*4), "}");
    } break;
    case JSON_ARRAY_BEGIN: {
        snprintf(valueStr, 128, "%*s childCount=%llu", (el->container.indentLevel*4), "[", el->container.childCount);
    } break;
    case JSON_ARRAY_END: {
        snprintf(valueStr, 128, "%*s", (el->container.indentLevel*4), "]");
    } break;
    case JSON_STRING: {
        snprintf(valueStr, 128, "%s", el->string.value);
    } break;
    case JSON_NUMBER: {
        snprintf(valueStr, 128, "%.16f", el->number.value);
    } break;
    case JSON_BOOL: {
        snprintf(valueStr, 128, "%s", el->boolean.value ? "TRUE" : "FALSE");
    } break;
    case JSON_NULL: {
        snprintf(valueStr, 128, "%s", "NULL");
    } break;
    case JSON_TYPE_COUNT:
        // NOOP
        break;
    }

    snprintf(buff, buffLen, "%s%s%s", typeStr, namePrefix, valueStr);
    return buff;
}
void json_freeFile(JsonFile* file) {
    assert(file != NULL);
    assert(file->elements != NULL);
    assert(file->stringBuff != NULL);
    free(file->elements);
    file->elements = NULL;
    file->elementCount = 0;
    file->elementCapacity = 0;
    free(file->stringBuff);
    file->stringBuff = NULL;
    file->stringBuffUsed = 0;
    file->stringBuffCapacity = 0;
}

