#include <errno.h>
#include <math.h>
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

#define BUFF_SIZE           10240
#define DEFAULT_FILENAME    "hw/listing_0040_challenge_movs"


typedef struct InstPrefix {
    u8 prefix;
    u8 mask;
} InstPrefix;

const InstPrefix OP_MOV_RGM_REG = { .prefix = 0b10001000, .mask = 0b11111100 };
const InstPrefix OP_MOV_IMM_RGM = { .prefix = 0b11000110, .mask = 0b11111110 };
const InstPrefix OP_MOV_IMM_REG = { .prefix = 0b10110000, .mask = 0b11110000 };
const InstPrefix OP_MOV_MEM_ACC = { .prefix = 0b10100000, .mask = 0b11111110 };
const InstPrefix OP_MOV_ACC_MEM = { .prefix = 0b10100010, .mask = 0b11111110 };
const InstPrefix OP_MOV_RGM_SRG = { .prefix = 0b10001110, .mask = 0b11111111 };
const InstPrefix OP_MOV_SRG_RGM = { .prefix = 0b10001100, .mask = 0b11111111 };

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

int decode_rgm(char* out_str, u8 rgm, u8 mod, u8 byte3, u8 byte4) {
    // Set prefix
    switch(rgm) {
    case 0b000: strcpy(out_str, "[bx + si"); break;
    case 0b001: strcpy(out_str, "[bx + di"); break;
    case 0b010: strcpy(out_str, "[bp + si"); break;
    case 0b011: strcpy(out_str, "[bp + di"); break;
    case 0b100: strcpy(out_str, "[si"); break;
    case 0b101: strcpy(out_str, "[di"); break;
    case 0b110: strcpy(out_str, "[bp"); break;
    case 0b111: strcpy(out_str, "[bx"); break;
    default: {
        fprintf(stderr, "Unknown rgm identifier: rgm=%s\n", u8_to_str(rgm));
        exit(1);
    }
    }

    int step_by = 0;
    if(mod == 0b00 && rgm == 0b110) {
        step_by = 2;
        u16 disp16 = (byte4 << 8) | byte3;
        sprintf(out_str, "[%d", disp16);
    } else if(mod == 0b01) {
        step_by = 1;
        if(byte3 != 0) {
            i8 sbyte3 = (i8) byte3;
            char sign = sbyte3 >= 0 ? '+' : '-';
            sbyte3 = sbyte3 < 0 ? -sbyte3 : sbyte3;
            sprintf(out_str + strlen(out_str), " %c %d", sign, sbyte3);
        }
    } else if(mod == 0b10) {
        step_by = 2;
        i16 disp16 = (byte4 << 8) | byte3;
        if(disp16 != 0) {
            char sign = disp16 >= 0 ? '+' : '-';
            disp16 = disp16 < 0 ? -disp16 : disp16;
            sprintf(out_str + strlen(out_str), " %c %d", sign, disp16);
        }
    }
    sprintf(out_str + strlen(out_str), "%c", ']');
    return step_by;
}


void decode_reg(char* out_str, u8 w, u8 reg) {
    u8 ident = (w << 3) | reg;
    switch(ident) {
    case 0b0000: strcpy(out_str, "al"); break;
    case 0b0001: strcpy(out_str, "cl"); break;
    case 0b0010: strcpy(out_str, "dl"); break;
    case 0b0011: strcpy(out_str, "bl"); break;
    case 0b0100: strcpy(out_str, "ah"); break;
    case 0b0101: strcpy(out_str, "ch"); break;
    case 0b0110: strcpy(out_str, "dh"); break;
    case 0b0111: strcpy(out_str, "bh"); break;
    case 0b1000: strcpy(out_str, "ax"); break;
    case 0b1001: strcpy(out_str, "cx"); break;
    case 0b1010: strcpy(out_str, "dx"); break;
    case 0b1011: strcpy(out_str, "bx"); break;
    case 0b1100: strcpy(out_str, "sp"); break;
    case 0b1101: strcpy(out_str, "bp"); break;
    case 0b1110: strcpy(out_str, "si"); break;
    case 0b1111: strcpy(out_str, "di"); break;
    default: {
        fprintf(stderr, "Unknown operand identifier: w=%d, reg=%s\n", w, u8_to_str(reg));
        exit(1);
    }
    }
}

int main(int argc, const char** argv) {
    const char* filename = DEFAULT_FILENAME;
    if(argc > 1) {
        filename = argv[1];
    }

    u8 data[BUFF_SIZE] = { 0 };
    u32 total_bytes = 0;
    {
        {
            FILE* file = fopen(filename, "rb");
            if(file == NULL) {
                fprintf(stderr, "Failed to open file %s: %s\n", filename, strerror(errno));
                exit(1);
            }
            total_bytes = fread(data, 1, BUFF_SIZE, file);
            fclose(file);
        }
        if(total_bytes == 0) {
            fprintf(stderr, "Failed to read file %s: %s\n", filename, strerror(errno));
            exit(1);
        }
        if(total_bytes == BUFF_SIZE) {
            fprintf(stderr, "Failed to read entire file of %s: increase BUFF_SIZE past %d\n", filename, BUFF_SIZE);
            exit(1);
        }
    }

    // Print preamble
    FILE* out = stdout;
    {
        fprintf(out, "; Instruction decode of %s\n", filename);
        fprintf(out, "bits 16\n\n");
    }

    // Decode
    for(int i = 0; i < total_bytes; i++) {
        memset(scratch, 0, BUFF_SIZE);
        next_scratch = scratch;

        u8 byte1 = data[i];
        // fprintf(out, "BYTE[%d]: %s\n", i, u8_to_str(byte1));

        char inst_str[8] = { 0 };
        char dst_operand_str[32] = { 0 };
        char src_operand_str[32] = { 0 };
        if((byte1 & OP_MOV_RGM_REG.mask) == OP_MOV_RGM_REG.prefix) {
            strcpy(inst_str, "mov");
            u8 byte2    = data[++i];
            u8 d        = (byte1 & 0b00000010) >> 1;           // 0 = REG is SRC, 1 = REG is DST
            u8 w        = (byte1 & 0b00000001);
            u8 mod      = (byte2 & 0b11000000) >> 6;
            u8 reg      = (byte2 & 0b00111000) >> 3;
            u8 rgm      = (byte2 & 0b00000111);

            if(mod == 0b11) {
                u8 dst = d == 0 ? rgm : reg;
                u8 src = d == 0 ? reg : rgm;
                decode_reg(dst_operand_str, w, dst);
                decode_reg(src_operand_str, w, src);
            } else {
                char* rgm_str = d == 0 ? dst_operand_str : src_operand_str;
                char* reg_str = d == 0 ? src_operand_str : dst_operand_str;
                decode_reg(reg_str, w, reg);
                int step_by = decode_rgm(rgm_str, rgm, mod, data[i+1], data[i+2]);
                i += step_by;
            }

        }
        else if((byte1 & OP_MOV_IMM_RGM.mask) == OP_MOV_IMM_RGM.prefix) {
            strcpy(inst_str, "mov");
            u8 byte2    = data[++i];
            u8 w        = (byte1 & 0b00000001);
            u8 mod      = (byte2 & 0b11000000) >> 6;
            u8 rgm      = (byte2 & 0b00000111);

            int step_by = decode_rgm(dst_operand_str, rgm, mod, data[i+1], data[i+2]);
            i += step_by;

            if(w == 0) {
                u8 byte5 = data[++i];
                sprintf(src_operand_str, "byte %d", byte5);
            } else {
                u8 byte5 = data[++i];
                u8 byte6 = data[++i];
                u16 value16 = (byte6 << 8) | byte5;
                sprintf(src_operand_str, "word %d", value16);
            }
        }
        else if((byte1 & OP_MOV_IMM_REG.mask) == OP_MOV_IMM_REG.prefix) {
            strcpy(inst_str, "mov");
            u8 w        = (byte1 & 0b00001000) >> 3;
            u8 reg      = (byte1 & 0b00000111);
            decode_reg(dst_operand_str, w, reg);
            u16 disp16 = 0;
            if(w == 0) {
                disp16 = data[++i];
            } else {
                u8 byte2 = data[++i];
                u8 byte3 = data[++i];
                disp16 = (byte3 << 8) | byte2;
            }
            sprintf(src_operand_str, "%d", disp16);
        }
        else if((byte1 & OP_MOV_MEM_ACC.mask) == OP_MOV_MEM_ACC.prefix ||
                (byte1 & OP_MOV_ACC_MEM.mask) == OP_MOV_ACC_MEM.prefix) {
            strcpy(inst_str, "mov");
            char* mem;
            char* acc;
            if((byte1 & OP_MOV_MEM_ACC.mask) == OP_MOV_MEM_ACC.prefix) {
                mem = src_operand_str;
                acc = dst_operand_str;
            } else {
                mem = dst_operand_str;
                acc = src_operand_str;
            }

            u8 w        = (byte1 & 0b00000001);
            if(w == 0) {
                strcpy(acc, "al");
                u8 byte2 = data[++i];
                sprintf(mem, "[%d]", byte2);
            } else {
                strcpy(acc, "ax");
                u8 byte2 = data[++i];
                u8 byte3 = data[++i];
                u16 value16 = (byte3 << 8) | byte2;
                sprintf(mem, "[%d]", value16);
            }
        }
        else if((byte1 & OP_MOV_RGM_SRG.mask) == OP_MOV_RGM_SRG.prefix) {
            strcpy(inst_str, "mov");
            u8 byte2    = data[++i];
            fprintf(stderr, "Not yet: OP_MOV_RGM_SRG");
            exit(1);

        }
        else if((byte1 & OP_MOV_SRG_RGM.mask) == OP_MOV_SRG_RGM.prefix) {
            strcpy(inst_str, "mov");
            u8 byte2    = data[++i];
            fprintf(stderr, "Not yet: OP_MOV_SRG_RGM");
            exit(1);

        }
        else {
            fprintf(stderr, "Unknown instruction: %d (%s)\n", byte1, u8_to_str(byte1));
            exit(1);
        }

        fprintf(out, "%s %s, %s\n", inst_str, dst_operand_str, src_operand_str);
    }
    return 0;
}
