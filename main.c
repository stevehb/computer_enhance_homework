#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef int8_t      s8;
typedef uint8_t     u8;
typedef int16_t     s16;
typedef uint16_t    u16;
typedef int32_t     s32;
typedef uint32_t    u32;
typedef int64_t     s64;
typedef uint64_t    u64;
typedef __int128_t  s128;
typedef __uint128_t u128;

#define BUFF_SIZE           10240
//#define DEFAULT_FILENAME    "hw/listing_0037_single_register_mov"
// #define DEFAULT_FILENAME    "hw/listing_0038_many_register_mov"
// #define DEFAULT_FILENAME    "hw/listing_0039_more_movs"
// #define DEFAULT_FILENAME    "hw/listing_0040_challenge_movs"
 #define DEFAULT_FILENAME    "hw/listing_0041_add_sub_cmp_jnz"

#define BOOL_STR(b) ((b) ? "TRUE" : "FALSE")
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

static u8 scratch[BUFF_SIZE];
static u8* next_scratch = scratch;

char* byteToBinaryStr(u8 byte) {
    char* str = (char*) next_scratch;
    next_scratch += 9;
    for(int i = 7; i >= 0; i--) {
        str[7-i] = ((byte >> i) & 1) ? '1' : '0';
    }
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





typedef enum BitFieldTypes {
    BF_NONE,
    BF_LIT,
    BF_SIGN,
    BF_WORD,
    BF_DIR,
    BF_MOD,
    BF_REG,
    BF_RGM,
    BF_COUNT
} BitFieldTypes;
const char* bitFieldTypeStrs[BF_COUNT] = {
    [BF_NONE]   = "BF_NONE",
    [BF_LIT]    = "BF_LIT ",
    [BF_SIGN]   = "BF_SIGN",
    [BF_WORD]   = "BF_WORD",
    [BF_DIR]    = "BF_DIR ",
    [BF_MOD]    = "BF_MOD ",
    [BF_REG]    = "BF_REG ",
    [BF_RGM]    = "BF_RGM ",
};
static_assert(sizeof(bitFieldTypeStrs)/sizeof(bitFieldTypeStrs[0]) == BF_COUNT, "bitFieldTypeStrs array size doesn't match BitsTypes enum count");

typedef enum InstId {
    MOV_RGM_REG,
    MOV_IMM_RGM,
    MOV_IMM_REG,
    MOV_MEM_ACC,
    MOV_ACC_MEM,
    ADD_RGM_REG,
    ADD_IMM_RGM,
    ADD_IMM_ACC,
    ID_COUNT
} InstId;
const char* instIdStrs[ID_COUNT] = {
    [MOV_RGM_REG] = "MOV_RGM_REG",
    [MOV_IMM_RGM] = "MOV_IMM_RGM",
    [MOV_IMM_REG] = "MOV_IMM_REG",
    [MOV_MEM_ACC] = "MOV_MEM_ACC",
    [MOV_ACC_MEM] = "MOV_ACC_MEM",
    [ADD_RGM_REG] = "ADD_RGM_REG",
    [ADD_IMM_RGM] = "ADD_IMM_RGM",
    [ADD_IMM_ACC] = "ADD_IMM_ACC",
};
static_assert(sizeof(instIdStrs)/sizeof(instIdStrs[0]) == ID_COUNT, "instIdStrs array size doesn't match InstId enum count");





typedef struct Bits {
    BitFieldTypes type;
    u8 byte, shr, and;
} Bits;
typedef struct OpCode {
    Bits where;
    u8 value;
} OpCode;
//typedef struct Parameter {
//    bool isDisp, isData, isAddr;
//    u16 value;
//} Parameter;
typedef struct InstDesc {
    InstId id;
    const char* instStr;
    OpCode opCode1, opCode2;
    Bits fields[BF_COUNT];
    bool hasImm, isImmAddr, isAccSrc, isAccDst;
} InstDesc;

#define BLIT(b, s, a)                 { .type = BF_LIT,  .byte = b, .shr = s, .and = a }
#define BSIGN(b, s, a)  [BF_SIGN]   = { .type = BF_SIGN, .byte = b, .shr = s, .and = a }
#define BDIR(b, s, a)   [BF_DIR]    = { .type = BF_DIR,  .byte = b, .shr = s, .and = a }
#define BWORD(b, s, a)  [BF_WORD]   = { .type = BF_WORD, .byte = b, .shr = s, .and = a }
#define BMOD(b, s, a)   [BF_MOD]    = { .type = BF_MOD,  .byte = b, .shr = s, .and = a }
#define BREG(b, s, a)   [BF_REG]    = { .type = BF_REG,  .byte = b, .shr = s, .and = a }
#define BRGM(b, s, a)   [BF_RGM]    = { .type = BF_RGM,  .byte = b, .shr = s, .and = a }
#define OP(v, b, s, a) { .value = v, .where = BLIT(b, s, a) }
#define OP_NONE() { .value = 0, .where = { .type = BF_NONE, .byte = 0, .shr = 0, .and = 0 } }
#define INST(i, s, op1, op2, imm, immAddr, accS, accD, ...) (InstDesc) { .id = i, .instStr = s, .opCode1 = op1, .opCode2 = op2, .fields = { __VA_ARGS__ }, .hasImm = imm, .isImmAddr = immAddr, .isAccSrc = accS, .isAccDst = accD }

InstDesc instructionDescriptors[ID_COUNT] = {
    [MOV_RGM_REG] = INST(MOV_RGM_REG, "mov", OP(0b00100010, 0, 2, 0b00111111), OP_NONE(),           false, false, false, false, BDIR(0, 1, 0b1), BWORD(0, 0, 0b1), BMOD(1, 6, 0b11), BREG(1, 3, 0b111), BRGM(1, 0, 0b111)),
    [MOV_IMM_RGM] = INST(MOV_IMM_RGM, "mov", OP(0b01100011, 0, 1, 0b01111111), OP(0, 1, 2, 0b000),  true,  false, false, false, BWORD(0, 0, 0b1), BMOD(1, 6, 0b11), BRGM(1, 0, 0b111)),
    [MOV_IMM_REG] = INST(MOV_IMM_REG, "mov", OP(0b00001011, 0, 4, 0b00001111), OP_NONE(),           true,  false, false, false, BWORD(0, 3, 0b1), BREG(0, 0, 0b111)),
    [MOV_MEM_ACC] = INST(MOV_MEM_ACC, "mov", OP(0b01010000, 0, 1, 0b01111111), OP_NONE(),           false, true,  false, true,  BWORD(0, 0, 0b1)),
    [MOV_ACC_MEM] = INST(MOV_ACC_MEM, "mov", OP(0b01010001, 0, 1, 0b01111111), OP_NONE(),           false, true,  true,  false, BWORD(0, 0, 0b1)),
    [ADD_RGM_REG] = INST(ADD_RGM_REG, "add", OP(0b00000000, 0, 2, 0b00111111), OP_NONE(),           false, false, false, false, BDIR(0, 1, 0b1), BWORD(0, 0, 0b1), BMOD(1, 6, 0b11), BREG(1, 3, 0b111), BRGM(1, 0, 0b111)),
    [ADD_IMM_RGM] = INST(ADD_IMM_RGM, "add", OP(0b00100000, 0, 2, 0b00111111), OP(0, 1, 2, 0b000),  true,  false, false, false, BSIGN(0, 1, 0b1), BMOD(1, 6, 0b11), BRGM(1, 0, 0b111)),
    [ADD_IMM_ACC] = INST(ADD_IMM_ACC, "add", OP(0b00000010, 0, 1, 0b01111111), OP_NONE(),           true,  false, false, false, BWORD(0, 0, 0b1)),

};
static_assert(sizeof(instructionDescriptors)/sizeof(instructionDescriptors[0]) == ID_COUNT, "InstDesc array size doesn't match InstId enum count");

#undef INST2
#undef INST1
#undef OP_NONE
#undef OP
#undef BRGM
#undef BREG
#undef BMOD
#undef BWORD
#undef BDIR
#undef BSIGN
#undef BLIT

typedef struct ParsedInst {
    InstId id;
    u32 bytesRead;
    u8 hasW :1; u8 w: 1;
    u8 hasS :1; u8 s: 1;
    u8 hasD :1; u8 d: 1;
    u8 hasMod :1; u8 mod: 2;
    u8 hasReg :1; u8 reg: 3;
    u8 hasRgm :1; u8 rgm: 3;
    char src[128];
    char dst[128];
} ParsedInst;

int writeRegStr(u8 w, u8 reg, char* outStr) {
    u8 ident = (w << 3) | reg;
    switch(ident) {
        case 0b0000: strcpy(outStr, "al"); break;
        case 0b0001: strcpy(outStr, "cl"); break;
        case 0b0010: strcpy(outStr, "dl"); break;
        case 0b0011: strcpy(outStr, "bl"); break;
        case 0b0100: strcpy(outStr, "ah"); break;
        case 0b0101: strcpy(outStr, "ch"); break;
        case 0b0110: strcpy(outStr, "dh"); break;
        case 0b0111: strcpy(outStr, "bh"); break;
        case 0b1000: strcpy(outStr, "ax"); break;
        case 0b1001: strcpy(outStr, "cx"); break;
        case 0b1010: strcpy(outStr, "dx"); break;
        case 0b1011: strcpy(outStr, "bx"); break;
        case 0b1100: strcpy(outStr, "sp"); break;
        case 0b1101: strcpy(outStr, "bp"); break;
        case 0b1110: strcpy(outStr, "si"); break;
        case 0b1111: strcpy(outStr, "di"); break;
        default: {
            fprintf(stderr, "Unknown operand identifier: w=%d, reg=%s\n", w, byteToBinaryStr(reg));
            exit(1);
        }
    }
    return 0;
}
int writeRgmStr(u8* ptr, u8 w, u8 mod, u8 rgm, char* outStr) {
    int extraBytesRead = 0;

    // Handle simple register interpretation
    if(mod == 0b11) {
        return writeRegStr(w, rgm, outStr);
    }

    // Handle "direct address" special case
    if(mod == 0b00 && rgm == 0b110) {
        u8 byte3 = ptr[2];
        extraBytesRead++;
        u8 byte4 = 0;
        if(w == 1) {
            byte4 = ptr[3];
            extraBytesRead++;
        } else {
            fprintf(stderr, "WARNING: Unexpected 1-byte displacement in direct address! Accommodate this case?");
            exit(1);
        }
        sprintf(outStr, "[%d]", (byte4 << 8) | byte3);
        return extraBytesRead;
    }

    // Set prefix
    switch(rgm) {
    case 0b000: strcpy(outStr, "[bx + si"); break;
    case 0b001: strcpy(outStr, "[bx + di"); break;
    case 0b010: strcpy(outStr, "[bp + si"); break;
    case 0b011: strcpy(outStr, "[bp + di"); break;
    case 0b100: strcpy(outStr, "[si"); break;
    case 0b101: strcpy(outStr, "[di"); break;
    case 0b110: strcpy(outStr, "[bp"); break;
    case 0b111: strcpy(outStr, "[bx"); break;
    default: {
        fprintf(stderr, "Unknown rgm identifier: rgm=%s\n", byteToBinaryStr(rgm));
        exit(1);
    }
    }

    if(mod == 0b01) {
        // Add 8-bit displacement
        u8 byte3 = ptr[2];
        extraBytesRead++;
        if (byte3 != 0) {
            s8 sbyte3 = (s8) byte3;
            char sign = sbyte3 >= 0 ? '+' : '-';
            sbyte3 = sbyte3 < 0 ? -sbyte3 : sbyte3;
            sprintf(outStr + strlen(outStr), " %c %d", sign, sbyte3);
        }
    } else if(mod == 0b10) {
        // Add 16-bit displacement
        u8 byte3 = ptr[2];
        extraBytesRead++;
        u8 byte4 = ptr[3];
        extraBytesRead++;
        s16 disp16 = (byte4 << 8) | byte3;
        if(disp16 != 0) {
            char sign = disp16 >= 0 ? '+' : '-';
            disp16 = disp16 < 0 ? -disp16 : disp16;
            sprintf(outStr + strlen(outStr), " %c %d", sign, disp16);
        }
    }

    sprintf(outStr + strlen(outStr), "%c", ']');
    return extraBytesRead;
}
int writeImmediate(u8* ptr, u8 s, u8 w, bool isImmAddr, bool isRgm, char* outStr) {
    int extraBytesRead = 0;
    char* left = isImmAddr ? "[" : "";
    char* right = isImmAddr ? "]" : "";
    char* sizeSpec = "";
    s16 imm = 0;
    s8 byte1 = *ptr;
    extraBytesRead++;
    if(w == 0) {
        imm = byte1;
        sizeSpec = isRgm ? "byte " : "";
    } else {
        s8 byte2 = *(ptr+1);
        extraBytesRead++;
        imm = (s16) ((byte2 << 8) | (u8) byte1);
        sizeSpec = isRgm ? "word " : "";
    }
    sprintf(outStr, "%s%s%d%s", left, sizeSpec, imm, right);
    return extraBytesRead;
}

u8 getBitValue(u8* ptr, Bits bits) {
    if(bits.type == BF_NONE || bits.type == BF_COUNT) {
        const char* type_str = (bits.type == BF_NONE) ? "BF_NONE" : "BF_COUNT";
        fprintf(stderr, "Error: Invalid bits type %s\n", type_str);
        exit(1);
    }
    u8 byte = *(ptr + bits.byte);
    return (byte >> bits.shr) & bits.and;
}
int findInstOpCodeMatch(u8* ptr) {
    for (int j = 0; j < ID_COUNT; j++) {
        InstDesc desc = instructionDescriptors[j];
        u8 opVal = getBitValue(ptr, desc.opCode1.where);
        if (opVal != desc.opCode1.value) {
            continue;
        }
        if (desc.opCode2.where.type == BF_NONE) {
            return j;
        }
        opVal = getBitValue(ptr, desc.opCode2.where);
        if(opVal == desc.opCode2.value) {
                return j;
        }
    }
    return -1;
}

ParsedInst parseInstruction(u8* ptr, InstDesc desc) {
    ParsedInst pi = { 0 };
    pi.id = desc.id;

    u8 opCodeBytes = 0;
    for(int i = 0; i < BF_COUNT; i++) {
        Bits bitField = desc.fields[i];
        if(bitField.type == BF_NONE || bitField.type == BF_COUNT) continue;
        u8 value = getBitValue(ptr, bitField);
        switch(bitField.type) {
            case BF_NONE:
            case BF_LIT:
            case BF_COUNT:  break;
            case BF_SIGN:   pi.hasS = true;     pi.s = value;   break;
            case BF_WORD:   pi.hasW = true;     pi.w = value;   break;
            case BF_DIR:    pi.hasD = true;     pi.d = value;   break;
            case BF_MOD:    pi.hasMod = true;   pi.mod = value; break;
            case BF_REG:    pi.hasReg = true;   pi.reg = value; break;
            case BF_RGM:    pi.hasRgm = true;   pi.rgm = value; break;
        }
        opCodeBytes = MAX(opCodeBytes, (bitField.byte + 1));
    }

    u8 additionalBytes = 0;
    if(pi.hasReg && pi.hasRgm) {
        if(pi.d == 0) {
            additionalBytes += writeRegStr(pi.w, pi.reg, pi.src);
            additionalBytes += writeRgmStr(ptr, pi.w, pi.mod, pi.rgm, pi.dst);
        } else {
            additionalBytes += writeRgmStr(ptr, pi.w, pi.mod, pi.rgm, pi.src);
            additionalBytes += writeRegStr(pi.w, pi.reg, pi.dst);
        }
    }

    else if(desc.hasImm) {
        if(pi.hasReg) {
            additionalBytes += writeRegStr(pi.w, pi.reg, pi.dst);
        } else if(pi.hasRgm) {
            additionalBytes += writeRgmStr(ptr, pi.w, pi.mod, pi.rgm, pi.dst);
        } else {
            fprintf(stderr, "Can't process immediate without REG or RGM as destination\n");
            exit(1);
        }
        additionalBytes += writeImmediate(ptr + opCodeBytes + additionalBytes, pi.s, pi.w, desc.isImmAddr, pi.hasRgm, pi.src);
    }

    else if(desc.isAccSrc || desc.isAccDst) {
        // TODO: Does Acc name depend on `w` ? Or is it always AX?
        additionalBytes += writeRegStr(pi.w, 0b000, desc.isAccSrc ? pi.src : pi.dst);
        additionalBytes += writeImmediate(ptr + opCodeBytes + additionalBytes, pi.s, pi.w, desc.isImmAddr, pi.hasRgm, desc.isAccSrc ? pi.dst : pi.src);
    }

    pi.bytesRead = opCodeBytes + additionalBytes;
    return pi;
}

char* parsedInstToStr(ParsedInst parsed) {
    InstDesc desc = instructionDescriptors[parsed.id];
    char* str = (char*) next_scratch;
    sprintf(str, "%s %s, %s", desc.instStr, parsed.dst, parsed.src);
    next_scratch += strlen(str);
    return str;
}


int main(int argc, const char** argv) {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    const char* filename = DEFAULT_FILENAME;
    if(argc > 1) {
        filename = argv[1];
    }

    // Read in binary file to be decoded
    u8 fileData[BUFF_SIZE] = { 0 };
    u32 fileDataSize = 0;
    {
        {
            FILE* file = fopen(filename, "rb");
            if(file == NULL) {
                fprintf(stderr, "Failed to open file %s: %s\n", filename, strerror(errno));
                exit(1);
            }
            fileDataSize = fread(fileData, 1, BUFF_SIZE, file);
            fclose(file);
        }
        if(fileDataSize == 0) {
            fprintf(stderr, "Failed to read file %s: %s\n", filename, strerror(errno));
            exit(1);
        }
        if(fileDataSize == BUFF_SIZE) {
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
    for(int i = 0; i < fileDataSize; /* INCREMENT IN LOOP */) {
        memset(scratch, 0, BUFF_SIZE);
        next_scratch = scratch;

        u8* data = fileData + i;
//        if(*data == 0b10100001 || *data == 0b10100011) {
//            printf("DEBUG\n");
//        }
//        fprintf(out, "[%d] Next inst: %s %s %s %s\n", i, byteToBinaryStr(*data), byteToBinaryStr(*(data+1)), byteToBinaryStr(*(data+2)), byteToBinaryStr(*(data+3)));
        int descIdx = findInstOpCodeMatch(data);
        if (descIdx == -1) {
            fprintf(stderr, "Failed to match instruction for bytes: %s\n", bytesToHexStr(data, 6));
            exit(1);
        }
//        fprintf(out, "[%d] Found %s\n", i, instIdStrs[descIdx]);

        InstDesc desc = instructionDescriptors[descIdx];
        ParsedInst parsed = parseInstruction(data, desc);
        if (parsed.bytesRead == 0) {
            fprintf(stderr, "Failed to parse instruction: %s: %s", instIdStrs[desc.id], bytesToHexStr(data, 6));
            exit(1);
        }
        i += parsed.bytesRead;

        char* line = parsedInstToStr(parsed);
        fprintf(out, "%s\n", line);
    }

    return 0;
}
