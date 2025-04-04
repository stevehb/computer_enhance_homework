//
// Created by stevehb on 02-Apr-25.
//

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
