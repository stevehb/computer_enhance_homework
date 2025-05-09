//
// Created by stevehb on 02-Apr-25.
//
#include <stdarg.h>
#include <stdbool.h>

static u8 scratch[BUFF_SIZE];
static u8* next_scratch = scratch;

#define BOOL_STR(b) ((b) ? "TRUE" : "FALSE")
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

char* byteToBinaryStr(u8 byte) {
    char* str = (char*) next_scratch;
    next_scratch += 9;
    for(int i = 7; i >= 0; i--) {
        str[7-i] = ((byte >> i) & 1) ? '1' : '0';
    }
    return str;
}
char* bytesToBinaryStr(u8* bytes, int size) {
    int len = MAX(size * 9, 1);
    char* str = (char*) next_scratch;
    next_scratch += len;
    char* next = str;
    for(int i = 0; i < size; i++) {
        u8 byte = bytes[i];
        for(int j = 7; j >= 0; j--) {
            next[7-j] = ((byte >> j) & 1) ? '1' : '0';
        }
        next[8] = ' ';
        next += 9;
    }
    str[len - 1] = '\0';
    return str;
}
char* bytesToHexStr(u8* bytes, int size) {
    static char HEX_CHARS[] = "0123456789ABCDEF";
    char* start = (char*) next_scratch;
    char* ptr = start;
    for(int i = 0; i < size; i++) {
        u8 byte = bytes[i];
        *ptr++ = HEX_CHARS[(byte >> 4) & 0xF];
        *ptr++ = HEX_CHARS[byte & 0xF];
        *ptr++ = ' ';
    }
    if(size > 0) {
        *(ptr - 1) = '\0';
    } else {
        *ptr++ = '\0';
    }
    next_scratch += (ptr - start);
    return start;
}

int sprintfcat(char *dst, const char *format, ...) {
    size_t len = strlen(dst);
    va_list args;
    va_start(args, format);
    int result = vsprintf(dst + len, format, args);
    va_end(args);
    return result;
}
int str_prepend(char* dst, const char* src) {
    int dstLen = strlen(dst);
    int srcLen = strlen(src);
    memmove(dst + srcLen, dst, dstLen + 1);
    memcpy(dst, src, srcLen);
    return strlen(dst);
}

u8 hasEvenParity(u8 bytes) {
    bytes ^= bytes >> 4;
    bytes ^= bytes >> 2;
    bytes ^= bytes >> 1;
    return (~bytes & 1);
}
u16 calcFlags(bool isAdd, u16 oldValue, u16 srcValue, u16 result) {
    bool carry;
    bool parity = hasEvenParity((u8)result);
    bool auxCarry;
    bool zero = result == 0;
    bool sign = (result >> 15) & 0b1;
    bool over;
    bool trap = false;
    bool interrupt = false;
    bool direction = false;

    if(isAdd) {
        carry = result < oldValue || result < srcValue;
    } else {
        carry = srcValue > oldValue;
    }
    if(isAdd) {
        auxCarry = ((result & 0xF) < (oldValue & 0xF)) || ((result & 0xF) < (srcValue & 0xF));
    } else {
        auxCarry = (srcValue & 0xF) > (oldValue & 0xF);
    }
    if(isAdd) {
        u16 sameSign = ~(oldValue ^ srcValue);
        u16 diffSign = (oldValue ^ result);
        over = ((sameSign & diffSign) & 0x8000) != 0;
    } else {
        u16 diffSign = (oldValue ^ srcValue);
        u16 sameSign = ~(srcValue ^ result);
        over = ((diffSign & sameSign) & 0x8000) != 0;
    }
    return (carry << flagsBitPosition[CF]) |
           (parity << flagsBitPosition[PF]) |
           (auxCarry << flagsBitPosition[AF]) |
           (zero << flagsBitPosition[ZF]) |
           (sign << flagsBitPosition[SF]) |
           (trap << flagsBitPosition[TF]) |
           (interrupt << flagsBitPosition[IF]) |
           (direction << flagsBitPosition[DF]) |
           (over << flagsBitPosition[OF]);
}


