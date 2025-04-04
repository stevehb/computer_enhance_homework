//
// Created by stevehb on 02-Apr-25.
//
#include <stdint.h>

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


#define BIT_FIELD_MAP(X) \
    X(BF_NONE, "BF_NONE") \
    X(BF_LIT, "BF_LIT") \
    X(BF_SIGN, "BF_SIGN") \
    X(BF_WORD, "BF_WORD") \
    X(BF_DIR, "BF_DIR") \
    X(BF_MOD, "BF_MOD") \
    X(BF_REG, "BF_REG") \
    X(BF_RGM, "BF_RGM")

typedef enum {
#define X(K, N) K,
    BIT_FIELD_MAP(X)
#undef X
    BF_COUNT
} BitFieldType;
const char* bitFieldTypeStrs[BF_COUNT] = {
#define X(K, N) [K] = N,
        BIT_FIELD_MAP(X)
#undef X
};


#define INST_TYPE_MAP(X) \
    X(MOV_RGM_REG, "MOV_RGM_REG") \
    X(MOV_IMM_RGM, "MOV_IMM_RGM") \
    X(MOV_IMM_REG, "MOV_IMM_REG") \
    X(MOV_MEM_ACC, "MOV_MEM_ACC") \
    X(MOV_ACC_MEM, "MOV_ACC_MEM") \
    X(ADD_RGM_REG, "ADD_RGM_REG") \
    X(ADD_IMM_RGM, "ADD_IMM_RGM") \
    X(ADD_IMM_ACC, "ADD_IMM_ACC") \
    X(SUB_RGM_REG, "SUB_RGM_REG") \
    X(SUB_IMM_RGM, "SUB_IMM_RGM") \
    X(SUB_IMM_ACC, "SUB_IMM_ACC") \
    X(CMP_RGM_REG, "CMP_RGM_REG") \
    X(CMP_IMM_RGM, "CMP_IMM_RGM") \
    X(CMP_IMM_ACC, "CMP_IMM_ACC")


typedef enum {
#define X(K, N) K,
    INST_TYPE_MAP(X)
#undef X
    ID_COUNT
} InstType;
const char* instTypeStrs[ID_COUNT] = {
#define X(K, N) [K] = N,
    INST_TYPE_MAP(X)
#undef X
};


typedef struct Bits {
    BitFieldType type;
    u8 byte, shr, and;
} Bits;
typedef struct OpCode {
    Bits where;
    u8 value;
} OpCode;
typedef struct Parameter {
    bool isSrc, isRgm, isDisp, isData, isAddr;
    u16 value;
} Parameter;
typedef struct InstDesc {
    InstType type;
    const char* instStr;
    OpCode opCode1, opCode2;
    Bits fields[BF_COUNT];
    bool hasImm, isImmAddr, isAccSrc, isAccDst;
    bool forceDstSize;
} InstDesc;

#define LIT(b, s, a)                  { .type = BF_LIT,  .byte = b, .shr = s, .and = a }
#define S(b, s, a)   [BF_SIGN] = { .type = BF_SIGN, .byte = b, .shr = s, .and = a }
#define D(b, s, a)   [BF_DIR]  = { .type = BF_DIR,  .byte = b, .shr = s, .and = a }
#define W(b, s, a)   [BF_WORD] = { .type = BF_WORD, .byte = b, .shr = s, .and = a }
#define MOD(b, s, a) [BF_MOD]  = { .type = BF_MOD,  .byte = b, .shr = s, .and = a }
#define REG(b, s, a) [BF_REG]  = { .type = BF_REG,  .byte = b, .shr = s, .and = a }
#define RGM(b, s, a) [BF_RGM]  = { .type = BF_RGM,  .byte = b, .shr = s, .and = a }
#define OP(v, b, s, a) { .value = v, .where = LIT(b, s, a) }
#define OP2_000 OP(0b000, 1, 3, 0b111)
#define OP2_101 OP(0b101, 1, 3, 0b111)
#define OP2_111 OP(0b111, 1, 3, 0b111)
#define OP_NONE { .value = 0, .where = { .type = BF_NONE, .byte = 0, .shr = 0, .and = 0 } }
#define INST(t, s, op1, op2, imm, immAddr, accS, accD, dstSize, ...) \
    [t] = (InstDesc) { \
        .type = t, \
        .instStr = s, \
        .opCode1 = op1, .opCode2 = op2, \
        .fields = { __VA_ARGS__ }, \
        .hasImm = imm, .isImmAddr = immAddr, \
        .isAccSrc = accS, .isAccDst = accD, \
        .forceDstSize = dstSize \
    }
#define T true
#define F false

InstDesc instructionDescriptors[ID_COUNT] = {
    INST(MOV_RGM_REG, "mov", OP(0b00100010, 0, 2, 0b00111111), OP_NONE, F, F, F, F, F, D(0, 1, 0b1), W(0, 0, 0b1), MOD(1, 6, 0b11), REG(1, 3, 0b111), RGM(1, 0, 0b111)),
    INST(MOV_IMM_RGM, "mov", OP(0b01100011, 0, 1, 0b01111111), OP2_000, T, F, F, F, F, W(0, 0, 0b1), MOD(1, 6, 0b11), RGM(1, 0, 0b111)),
    INST(MOV_IMM_REG, "mov", OP(0b00001011, 0, 4, 0b00001111), OP_NONE, T, F, F, F, F, W(0, 3, 0b1), REG(0, 0, 0b111)),
    INST(MOV_MEM_ACC, "mov", OP(0b01010000, 0, 1, 0b01111111), OP_NONE, F, T, F, T, F, W(0, 0, 0b1)),
    INST(MOV_ACC_MEM, "mov", OP(0b01010001, 0, 1, 0b01111111), OP_NONE, F, T, T, F, F, W(0, 0, 0b1)),
    INST(ADD_RGM_REG, "add", OP(0b00000000, 0, 2, 0b00111111), OP_NONE, F, F, F, F, F, D(0, 1, 0b1), W(0, 0, 0b1), MOD(1, 6, 0b11), REG(1, 3, 0b111), RGM(1, 0, 0b111)),
    INST(ADD_IMM_RGM, "add", OP(0b00100000, 0, 2, 0b00111111), OP2_000, T, F, F, F, T, S(0, 1, 0b1), W(0, 0, 0b1), MOD(1, 6, 0b11), RGM(1, 0, 0b111)),
    INST(ADD_IMM_ACC, "add", OP(0b00000010, 0, 1, 0b01111111), OP_NONE, T, F, F, T, F, W(0, 0, 0b1)),
    INST(SUB_RGM_REG, "sub", OP(0b00001010, 0, 2, 0b00111111), OP_NONE, F, F, F, F, F, D(0, 1, 0b1), W(0, 0, 0b1), MOD(1, 6, 0b11), REG(1, 3, 0b111), RGM(1, 0, 0b111)),
    INST(SUB_IMM_RGM, "sub", OP(0b00100000, 0, 2, 0b00111111), OP2_101, T, F, F, F, T, S(0, 1, 0b1), W(0, 0, 0b1), MOD(1, 6, 0b11), RGM(1, 0, 0b111)),
    INST(SUB_IMM_ACC, "sub", OP(0b00010110, 0, 1, 0b01111111), OP_NONE, T, F, F, T, F, W(0, 0, 0b1)),
    INST(CMP_RGM_REG, "cmp", OP(0b00001110, 0, 2, 0b00111111), OP_NONE, F, F, F, F, F, D(0, 1, 0b1), W(0, 0, 0b1), MOD(1, 6, 0b11), REG(1, 3, 0b111), RGM(1, 0, 0b111)),
    INST(CMP_IMM_RGM, "cmp", OP(0b00100000, 0, 2, 0b00111111), OP2_111, T, F, F, F, T, S(0, 1, 0b1), W(0, 0, 0b1), MOD(1, 6, 0b11), RGM(1, 0, 0b111)),
    INST(CMP_IMM_ACC, "cmp", OP(0b00011110, 0, 1, 0b01111111), OP_NONE, T, F, F, T, F, W(0, 0, 0b1)),
};
static_assert(sizeof(instructionDescriptors)/sizeof(instructionDescriptors[0]) == ID_COUNT, "InstDesc array size doesn't match InstType enum count");

#undef INST2
#undef INST1
#undef OP_NONE
#undef OP
#undef RGM
#undef REG
#undef MOD
#undef W
#undef D
#undef S
#undef LIT

typedef struct {
    InstType id;
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
