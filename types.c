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
    X(BF_NONE, "BF_NONE")\
    X(BF_CODE, "BF_CODE") \
    X(BF_SIGN, "BF_SIGN") \
    X(BF_WORD, "BF_WORD") \
    X(BF_DIR,  "BF_DIR") \
    X(BF_MOD,  "BF_MOD") \
    X(BF_REG,  "BF_REG") \
    X(BF_RGM,  "BF_RGM") \
    X(BF_DISP, "BF_DISP") \
    X(BF_DATA, "BF_DATA") \
    X(BF_ADDR, "BF_ADDR") \
    X(BF_INC8, "BF_INC8")

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
#undef BIT_FIELD_MAP

typedef enum ActionType {
    MOV, ADD, SUB, CMP, JMP
} InstAction;

#define INST_TYPE_MAP(X) \
    X(MOV_RGM_REG, MOV, "MOV_RGM_REG") \
    X(MOV_IMM_RGM, MOV, "MOV_IMM_RGM") \
    X(MOV_IMM_REG, MOV, "MOV_IMM_REG") \
    X(MOV_MEM_ACC, MOV, "MOV_MEM_ACC") \
    X(MOV_ACC_MEM, MOV, "MOV_ACC_MEM") \
    X(ADD_RGM_REG, ADD, "ADD_RGM_REG") \
    X(ADD_IMM_RGM, ADD, "ADD_IMM_RGM") \
    X(ADD_IMM_ACC, ADD, "ADD_IMM_ACC") \
    X(SUB_RGM_REG, SUB, "SUB_RGM_REG") \
    X(SUB_IMM_RGM, SUB, "SUB_IMM_RGM") \
    X(SUB_IMM_ACC, SUB, "SUB_IMM_ACC") \
    X(CMP_RGM_REG, CMP, "CMP_RGM_REG") \
    X(CMP_IMM_RGM, CMP, "CMP_IMM_RGM") \
    X(CMP_IMM_ACC, CMP, "CMP_IMM_ACC") \
    X(JMP_JE,      JMP, "JMP_JE") \
    X(JMP_JNE,     JMP, "JMP_JNE") \
    X(JMP_JL,      JMP, "JMP_JL") \
    X(JMP_JNL,     JMP, "JMP_JNL") \
    X(JMP_JLE,     JMP, "JMP_JLE") \
    X(JMP_JG,      JMP, "JMP_JG") \
    X(JMP_JB,      JMP, "JMP_JB") \
    X(JMP_JNB,     JMP, "JMP_JNB") \
    X(JMP_JBE,     JMP, "JMP_JBE") \
    X(JMP_JA,      JMP, "JMP_JA") \
    X(JMP_JP,      JMP, "JMP_JP") \
    X(JMP_JNP,     JMP, "JMP_JNP") \
    X(JMP_JO,      JMP, "JMP_JO") \
    X(JMP_JNO,     JMP, "JMP_JNO") \
    X(JMP_JS,      JMP, "JMP_JS") \
    X(JMP_JNS,     JMP, "JMP_JNS") \
    X(JMP_LOOP,    JMP, "JMP_LOOP") \
    X(JMP_JCXZ,    JMP, "JMP_JCXZ") \
    X(JMP_LOOPZ,   JMP, "JMP_LOOPZ") \
    X(JMP_LOOPNZ,  JMP, "JMP_LOOPNZ")

typedef enum {
#define X(K, A, N) K,
    INST_TYPE_MAP(X)
#undef X
    ID_COUNT
} InstType;
const char* instTypeStrs[ID_COUNT] = {
#define X(K, A, N) [K] = N,
    INST_TYPE_MAP(X)
#undef X
};
const InstAction INST_ACTION[ID_COUNT] = {
#define X(K,A,N) [K] = A,
    INST_TYPE_MAP(X)
#undef X
};

#undef INST_TYPE_MAP


typedef struct Bits {
    BitFieldType type;
    u8 byte, shr, and;
} BitField;
typedef struct OpCode {
    BitField where;
    u8 value;
} OpCode;
typedef struct InstDesc {
    InstType type;
    const char* instStr;
    OpCode opCode1, opCode2;
    BitField fields[BF_COUNT];
    bool isAccSrc, isAccDst;
    bool forceDstSize;
} InstDesc;

#define S(b,s,a)   [BF_SIGN] = { .type = BF_SIGN, .byte = b, .shr = s, .and = a }
#define D(b,s,a)   [BF_DIR]  = { .type = BF_DIR,  .byte = b, .shr = s, .and = a }
#define W(b,s,a)   [BF_WORD] = { .type = BF_WORD, .byte = b, .shr = s, .and = a }
#define MOD(b,s,a) [BF_MOD]  = { .type = BF_MOD,  .byte = b, .shr = s, .and = a }
#define REG(b,s,a) [BF_REG]  = { .type = BF_REG,  .byte = b, .shr = s, .and = a }
#define RGM(b,s,a) [BF_RGM]  = { .type = BF_RGM,  .byte = b, .shr = s, .and = a }
#define DISP       [BF_DISP] = { .type = BF_DISP, .byte = 0, .shr = 0, .and = 0 }
#define DATA       [BF_DATA] = { .type = BF_DATA, .byte = 0, .shr = 0, .and = 0 }
#define ADDR       [BF_ADDR] = { .type = BF_ADDR, .byte = 0, .shr = 0, .and = 0 }
#define INC8       [BF_INC8] = { .type = BF_INC8, .byte = 0, .shr = 0, .and = 0 }
#define OP(v,b,s,a) { .value = v, .where = { .type = BF_CODE, .byte = b, .shr = s, .and = a } }
#define OP_000 OP(0b000, 1, 3, 0b111)
#define OP_101 OP(0b101, 1, 3, 0b111)
#define OP_111 OP(0b111, 1, 3, 0b111)
#define NOOP { .value = 0, .where = { .type = BF_NONE, .byte = 0, .shr = 0, .and = 0 } }
#define INST(t, s, op1, op2, accS, accD, dstSize, ...) \
    [t] = (InstDesc) { \
        .type = t, \
        .instStr = s, \
        .opCode1 = op1, .opCode2 = op2, \
        .isAccSrc = accS, .isAccDst = accD, \
        .forceDstSize = dstSize, \
        .fields = { __VA_ARGS__ }, \
    }
#define T true
#define F false

InstDesc instructionDescriptors[ID_COUNT] = {
    INST(MOV_RGM_REG, "mov",    /* DISP      */OP(0b00100010, 0, 2, 0b00111111), NOOP,   F, F, F, D(0, 1, 0b1), W(0, 0, 0b1), MOD(1, 6, 0b11), REG(1, 3, 0b111), RGM(1, 0, 0b111), DISP),
    INST(MOV_IMM_RGM, "mov",    /* DISP DATA */OP(0b01100011, 0, 1, 0b01111111), OP_000, F, F, F,               W(0, 0, 0b1), MOD(1, 6, 0b11),                   RGM(1, 0, 0b111), DISP, DATA),
    INST(MOV_IMM_REG, "mov",    /*      DATA */OP(0b00001011, 0, 4, 0b00001111), NOOP,   F, F, F,               W(0, 3, 0b1),                  REG(0, 0, 0b111),                         DATA),
    INST(MOV_MEM_ACC, "mov",    /* ADDR      */OP(0b01010000, 0, 1, 0b01111111), NOOP,   F, T, F,               W(0, 0, 0b1),                                                      ADDR),
    INST(MOV_ACC_MEM, "mov",    /* ADDR      */OP(0b01010001, 0, 1, 0b01111111), NOOP,   T, F, F,               W(0, 0, 0b1),                                                      ADDR),
    INST(ADD_RGM_REG, "add",    /* DISP      */OP(0b00000000, 0, 2, 0b00111111), NOOP,   F, F, F, D(0, 1, 0b1), W(0, 0, 0b1), MOD(1, 6, 0b11), REG(1, 3, 0b111), RGM(1, 0, 0b111), DISP),
    INST(ADD_IMM_RGM, "add",    /* DISP DATA */OP(0b00100000, 0, 2, 0b00111111), OP_000, F, F, T, S(0, 1, 0b1), W(0, 0, 0b1), MOD(1, 6, 0b11),                   RGM(1, 0, 0b111), DISP, DATA),
    INST(ADD_IMM_ACC, "add",    /*      DATA */OP(0b00000010, 0, 1, 0b01111111), NOOP,   F, T, F,               W(0, 0, 0b1),                                                            DATA),
    INST(SUB_RGM_REG, "sub",    /* DISP      */OP(0b00001010, 0, 2, 0b00111111), NOOP,   F, F, F, D(0, 1, 0b1), W(0, 0, 0b1), MOD(1, 6, 0b11), REG(1, 3, 0b111), RGM(1, 0, 0b111), DISP),
    INST(SUB_IMM_RGM, "sub",    /* DISP DATA */OP(0b00100000, 0, 2, 0b00111111), OP_101, F, F, T, S(0, 1, 0b1), W(0, 0, 0b1), MOD(1, 6, 0b11),                   RGM(1, 0, 0b111), DISP, DATA),
    INST(SUB_IMM_ACC, "sub",    /*      DATA */OP(0b00010110, 0, 1, 0b01111111), NOOP,   F, T, F,               W(0, 0, 0b1),                                                            DATA),
    INST(CMP_RGM_REG, "cmp",    /* DISP      */OP(0b00001110, 0, 2, 0b00111111), NOOP,   F, F, F, D(0, 1, 0b1), W(0, 0, 0b1), MOD(1, 6, 0b11), REG(1, 3, 0b111), RGM(1, 0, 0b111), DISP),
    INST(CMP_IMM_RGM, "cmp",    /* DISP DATA */OP(0b00100000, 0, 2, 0b00111111), OP_111, F, F, T, S(0, 1, 0b1), W(0, 0, 0b1), MOD(1, 6, 0b11),                   RGM(1, 0, 0b111), DISP, DATA),
    INST(CMP_IMM_ACC, "cmp",    /*      DATA */OP(0b00011110, 0, 1, 0b01111111), NOOP,   F, T, F,               W(0, 0, 0b1),                                                            DATA),
    INST(JMP_JE,      "je",     /* INC8      */OP(0b01110100, 0, 0, 0b11111111), NOOP,   F, F, F, INC8),
    INST(JMP_JNE,     "jne",    /* INC8      */OP(0b01110101, 0, 0, 0b11111111), NOOP,   F, F, F, INC8),
    INST(JMP_JL,      "jl",     /* INC8      */OP(0b01111100, 0, 0, 0b11111111), NOOP,   F, F, F, INC8),
    INST(JMP_JNL,     "jnl",    /* INC8      */OP(0b01111101, 0, 0, 0b11111111), NOOP,   F, F, F, INC8),
    INST(JMP_JLE,     "jle",    /* INC8      */OP(0b01111110, 0, 0, 0b11111111), NOOP,   F, F, F, INC8),
    INST(JMP_JG,      "jg",     /* INC8      */OP(0b01111111, 0, 0, 0b11111111), NOOP,   F, F, F, INC8),
    INST(JMP_JB,      "jb",     /* INC8      */OP(0b01110010, 0, 0, 0b11111111), NOOP,   F, F, F, INC8),
    INST(JMP_JNB,     "jnb",    /* INC8      */OP(0b01110011, 0, 0, 0b11111111), NOOP,   F, F, F, INC8),
    INST(JMP_JBE,     "jbe",    /* INC8      */OP(0b01110110, 0, 0, 0b11111111), NOOP,   F, F, F, INC8),
    INST(JMP_JA,      "ja",     /* INC8      */OP(0b01110111, 0, 0, 0b11111111), NOOP,   F, F, F, INC8),
    INST(JMP_JP,      "jp",     /* INC8      */OP(0b01111010, 0, 0, 0b11111111), NOOP,   F, F, F, INC8),
    INST(JMP_JNP,     "jnp",    /* INC8      */OP(0b01111011, 0, 0, 0b11111111), NOOP,   F, F, F, INC8),
    INST(JMP_JO,      "jo",     /* INC8      */OP(0b01110000, 0, 0, 0b11111111), NOOP,   F, F, F, INC8),
    INST(JMP_JNO,     "jno",    /* INC8      */OP(0b01110001, 0, 0, 0b11111111), NOOP,   F, F, F, INC8),
    INST(JMP_JS,      "js",     /* INC8      */OP(0b01111000, 0, 0, 0b11111111), NOOP,   F, F, F, INC8),
    INST(JMP_JNS,     "jns",    /* INC8      */OP(0b01111001, 0, 0, 0b11111111), NOOP,   F, F, F, INC8),
    INST(JMP_LOOP,    "loop",   /* INC8      */OP(0b11100010, 0, 0, 0b11111111), NOOP,   F, F, F, INC8),
    INST(JMP_JCXZ,    "jcxz",   /* INC8      */OP(0b11100011, 0, 0, 0b11111111), NOOP,   F, F, F, INC8),
    INST(JMP_LOOPZ,   "loopz",  /* INC8      */OP(0b11100001, 0, 0, 0b11111111), NOOP,   F, F, F, INC8),
    INST(JMP_LOOPNZ,  "loopnz", /* INC8      */OP(0b11100000, 0, 0, 0b11111111), NOOP,   F, F, F, INC8),
};
static_assert(sizeof(instructionDescriptors)/sizeof(instructionDescriptors[0]) == ID_COUNT, "InstDesc array size doesn't match InstType enum count");

#undef F
#undef T
#undef S
#undef D
#undef W
#undef MOD
#undef REG
#undef RGM
#undef DISP
#undef DATA
#undef ADDR
#undef INC8
#undef OP
#undef OP_000
#undef OP_101
#undef OP_111
#undef NOOP
#undef INST

#define REGISTERS_MAP(X) \
    X(REG_AL, 0b0000, "al") \
    X(REG_CL, 0b0001, "cl") \
    X(REG_DL, 0b0010, "dl") \
    X(REG_BL, 0b0011, "bl") \
    X(REG_AH, 0b0100, "ah") \
    X(REG_CH, 0b0101, "ch") \
    X(REG_DH, 0b0110, "dh") \
    X(REG_BH, 0b0111, "bh") \
    X(REG_AX, 0b1000, "ax") \
    X(REG_CX, 0b1001, "cx") \
    X(REG_DX, 0b1010, "dx") \
    X(REG_BX, 0b1011, "bx") \
    X(REG_SP, 0b1100, "sp") \
    X(REG_BP, 0b1101, "bp") \
    X(REG_SI, 0b1110, "si") \
    X(REG_DI, 0b1111, "di")

typedef enum {
#define X(K, V, N) K = V,
    REGISTERS_MAP(X)
#undef X
    REG_COUNT
} RegisterEnum;
const char* registerNameStrs[REG_COUNT] = {
#define X(K, V, N) [K] = N,
    REGISTERS_MAP(X)
#undef X
};
#undef REGISTERS_MAP

#define EFFECTIVE_ADDR_MAP(X) \
    X(EA_BX_SI, 0b000, "bx + si") \
    X(EA_BX_DI, 0b001, "bx + di") \
    X(EA_BP_SI, 0b010, "bp + si") \
    X(EA_BP_DI, 0b011, "bp + di") \
    X(EA_SI,    0b100, "si") \
    X(EA_DI,    0b101, "di") \
    X(EA_BP,    0b110, "bp") \
    X(EA_BX,    0b111, "bx")
typedef enum {
#define X(K, V, N) K = V,
    EFFECTIVE_ADDR_MAP(X)
#undef X
    EA_COUNT
} EffectiveAddress;
const char* effectiveAddrStrs[EA_COUNT] = {
#define X(K,V,N) [K] = N,
    EFFECTIVE_ADDR_MAP(X)
#undef X
};





typedef enum OperandType {
    OPD_NOOP,
    OPD_REG,
    OPD_DISP,
    OPD_DATA,
    OPD_ADDR,
    OPD_INC8,
    OPD_COUNT
} OperandType;

typedef struct Operand {
    OperandType type;
    struct {
        RegisterEnum regIdx;
        bool writeSize;
    } regOpd;
    struct {
        EffectiveAddress eaIdx;
        char sign;
        s16 disp;
        bool writeSize;
    } dispOpd;
    struct {
        bool writeSize;
        s16 data;
    } dataOpd;
    struct {
        u16 addr;
    } addrOpd;
    struct {
        s8 inc8;
    } inc8Opd;
    char str[128];
} Operand;

typedef struct ParsedInst {
    InstType type;
    InstAction action;
    u32 bytesRead;
    u8 w: 1;
    u8 s: 1;
    u8 d: 1;
    u8 mod: 2;
    u8 reg: 3;
    u8 rgm: 3;
    u8 inc8: 8;
    u8 dispBits, dispLo, dispHi;
    u8 dataLo, dataHi;
    u8 addrLo, addrHi;
    Operand src, dst;
} ParsedInst;
