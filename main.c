#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef int8_t      i8;
typedef uint8_t     u8;
typedef int16_t     i16;
typedef uint16_t    u16;
typedef int32_t     i32;
typedef uint32_t    u32;
typedef int64_t     i64;
typedef uint64_t    u64;
typedef __int128_t  i128;
typedef __uint128_t u128;

#define BUFF_SIZE   10240
#define FILENAME    "hw/listing_0038_many_register_mov"

typedef enum OpCodes {
    OP_MOV = 0b100010,

} OpCodes;

static u8 scratch[BUFF_SIZE];
static u8* next_scratch = scratch;

char* u8_to_str(u8 byte) {
    char* str = (char*) next_scratch;
    next_scratch += 9;
    for(int i = 7; i >= 0; i--) {
        str[7-i] = ((byte >> i) & 1) ? '1' : '0';
    }
    return str;
}

void set_operand_str(char* dest, u8 word, u8 reg) {
    u8 ident = (word << 3) | reg;
    switch(ident) {
        case 0b0000: strcpy(dest, "al"); break;
        case 0b0001: strcpy(dest, "cl"); break;
        case 0b0010: strcpy(dest, "dl"); break;
        case 0b0011: strcpy(dest, "bl"); break;
        case 0b0100: strcpy(dest, "ah"); break;
        case 0b0101: strcpy(dest, "ch"); break;
        case 0b0110: strcpy(dest, "dh"); break;
        case 0b0111: strcpy(dest, "bh"); break;
        case 0b1000: strcpy(dest, "ax"); break;
        case 0b1001: strcpy(dest, "cx"); break;
        case 0b1010: strcpy(dest, "dx"); break;
        case 0b1011: strcpy(dest, "bx"); break;
        case 0b1100: strcpy(dest, "sp"); break;
        case 0b1101: strcpy(dest, "bp"); break;
        case 0b1110: strcpy(dest, "si"); break;
        case 0b1111: strcpy(dest, "di"); break;
        default: {
            fprintf(stderr, "Unknown operand identifier: word=%d, reg=%s\n", word, u8_to_str(reg));
            exit(1);
        }
    }
}

int main(void) {
    u8 data[BUFF_SIZE] = { 0 };
    u32 total_bytes = 0;
    {
        FILE* file = fopen(FILENAME, "rb");
        if(file == NULL) {
            fprintf(stderr, "Failed to open file %s: %s\n", FILENAME, strerror(errno));
            exit(1);
        }
        total_bytes = fread(data, 1, BUFF_SIZE, file);
        fclose(file);
    }
    if(total_bytes == 0) {
        fprintf(stderr, "Failed to read file %s: %s\n", FILENAME, strerror(errno));
        exit(1);
    }
    if(total_bytes == BUFF_SIZE) {
        fprintf(stderr, "Failed to read entire file of %s: increase BUFF_SIZE past %d\n", FILENAME, BUFF_SIZE);
        exit(1);
    }

    FILE* out = stdout;

    // Preamble
    {
        fprintf(out, "; Instruction decode of %s\n", FILENAME);
        fprintf(out, "bits 16\n\n");
    }

    // Decode
    for(int i = 0; i < total_bytes; i++) {
        memset(scratch, 0, BUFF_SIZE);
        next_scratch = scratch;

        u8 byte1 = data[i];
        // fprintf(out, "BYTE[%d]: %s\n", i, u8_to_str(byte1));

        char op_str[8] = { 0 };
        char operand1_str[8] = { 0 };
        char operand2_str[8] = { 0 };
        u8 opcode = byte1 >> 2;
        switch(opcode) {
            case OP_MOV: {
                strcpy(op_str, "mov");
                u8 byte2    = data[++i];
                // fprintf(out, "BYTE[%d]: %s\n", i, u8_to_str(byte2));
                u8 dir  = byte1 & 0b00000010;           // 0 = REG is SRC, 1 = REG is DST
                u8 word = byte1 & 0b00000001;
                u8 mode = (byte2 & 0b11000000) >> 6;
                u8 reg  = (byte2 & 0b00111000) >> 3;
                u8 rm   = byte2 & 0b00000111;
                if(mode != 0b11) {
                    fprintf(stderr, "Unsupported MOD %s for %s\n", u8_to_str(mode), op_str);
                    exit(1);
                }
                if(dir == 0) {
                    set_operand_str(operand1_str, word, rm);
                    set_operand_str(operand2_str, word, reg);
                } else {
                    set_operand_str(operand1_str, word, reg);
                    set_operand_str(operand2_str, word, rm);
                }
                fprintf(out, "%s %s, %s\n", op_str, operand1_str, operand2_str);
            } break;

            default: {
                fprintf(stderr, "Unknown opcode: %s from %d\n", u8_to_str(opcode), byte1);
                exit(1);
            }
        }
    }
    return 0;
}
