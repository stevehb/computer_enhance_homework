#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFF_SIZE       102400
#define MAX_INST_COUNT  128

#include "types.c"
#include "utils.c"

//#define DEFAULT_FILENAME    "hw/listing_0037_single_register_mov"
//#define DEFAULT_FILENAME    "hw/listing_0038_many_register_mov"
//#define DEFAULT_FILENAME    "hw/listing_0039_more_movs"
//#define DEFAULT_FILENAME    "hw/listing_0040_challenge_movs"
//#define DEFAULT_FILENAME    "hw/listing_0041_add_sub_cmp_jnz"
//#define DEFAULT_FILENAME    "hw/listing_0043_immediate_movs"
//#define DEFAULT_FILENAME    "hw/listing_0044_register_movs"
//#define DEFAULT_FILENAME    "hw/listing_0045_challenge_register_movs"
#define DEFAULT_FILENAME    "hw/listing_0046_add_sub_cmp"
//#define DEFAULT_FILENAME    "hw/listing_0047_challenge_flags"

u8 getBitValue(u8* ptr, BitField bits) {
    if(bits.type == BF_NONE || bits.type == BF_COUNT) {
        const char* type_str = (bits.type == BF_NONE) ? "BF_NONE" : "BF_COUNT";
        fprintf(stderr, "ERROR: Invalid bits action %s\n", type_str);
        exit(1);
    }
    u8 byte = *(ptr + bits.byte);
    return (byte >> bits.shr) & bits.and;
}

void makeRegOpd(Operand* op, u8 reg, u8 w) {
    op->type = OPD_REG;
    op->regOpd.regIdx = (w << 3) | reg;
    if(op->regOpd.regIdx >= REG_COUNT) {
        fprintf(stderr, "Unknown values for regIdx=%d and w=%d", reg, w);
        exit(1);
    }
    strcat(op->str, registerNameStrs[op->regOpd.regIdx]);
}
void makeSrgOpd(Operand* op, u8 srg) {
    op->type = OPD_SRG;
    op->srgOpd.srgIdx = srg;
    if(op->srgOpd.srgIdx >= SRG_COUNT) {
        fprintf(stderr, "Unknown values for srgIdx=%d", srg);
        exit(1);
    }
    strcat(op->str, segmentRegisterStrs[op->srgOpd.srgIdx]);
}
void makeRgmOpd(Operand* op, u8 rgm, u8 w, u8 mod, bool writeSize, u8 dispLo, u8 dispHi) {
    if(writeSize) {
        strcat(op->str, w ? "word " : "byte ");
    }
    if(mod == 0b11) {
        makeRegOpd(op, rgm, w);
        op->regOpd.writeSize = writeSize;
        return;
    }

    op->type = OPD_DISP;
    op->dispOpd.writeSize = writeSize;
    if(mod == 0b00 && rgm == 0b110) {
        op->dispOpd.eaIdx = EA_COUNT;
        // TODO: Do we need to check the `w` flag here?
        op->dispOpd.disp = (dispHi << 8) | dispLo;
        op->dispOpd.sign = op->dispOpd.disp >= 0 ? '+' : '-';
        sprintfcat(op->str, "[%d]", op->dispOpd.disp);
        return;
    }

    op->dispOpd.eaIdx = rgm;
    op->dispOpd.disp = mod == 0b01 ? (s8) dispLo : (dispHi << 8) | dispLo;
    op->dispOpd.sign = op->dispOpd.disp >= 0 ? '+' : '-';
    u16 uDisp = op->dispOpd.disp < 0 ? -op->dispOpd.disp : op->dispOpd.disp;
    sprintfcat(op->str, "[%s", effectiveAddrStrs[op->dispOpd.eaIdx]);
    if(op->dispOpd.disp != 0) {
        sprintfcat(op->str, " %c %d", op->dispOpd.sign, uDisp);
    }
    strcat(op->str, "]");
}
void makeAddrOpd(Operand* op, u8 addrLo, u8 addrHi) {
    op->type = OPD_ADDR;
    op->addrOpd.addr = (addrHi << 8) | addrLo;
    sprintfcat(op->str, "[%d]", op->addrOpd.addr);
}
void makeImmOpd(Operand *op, u8 w, u8 s, bool writeSize, u8 dataLo, u8 dataHi) {
    op->type = OPD_DATA;
    op->dataOpd.writeSize = writeSize;
    op->dataOpd.data = w ? (dataHi << 8) | dataLo : (s8) dataLo;
    char* sizeStr = writeSize ? (w ? "word " : "byte ") : "";
    sprintfcat(op->str, "%s%d", sizeStr, op->dataOpd.data);
}

ParsedInst parseInstruction(u8* ptr, InstDesc desc) {
    ParsedInst pi = { 0 };
    pi.type = desc.type;
    pi.action = INST_ACTION[desc.type];

    /*
     * READ OP-CODE BITFIELDS
     */
    u8 byteCount = 0;
    for(int i = 0; i < BF_COUNT; i++) {
        BitField bitField = desc.fields[i];
        if(bitField.type == BF_NONE) continue;
        switch(bitField.type) {
            case BF_SIGN:   pi.s = getBitValue(ptr, bitField);   break;
            case BF_WORD:   pi.w = getBitValue(ptr, bitField);   break;
            case BF_DIR:    pi.d = getBitValue(ptr, bitField);   break;
            case BF_MOD:    pi.mod = getBitValue(ptr, bitField); break;
            case BF_REG:    pi.reg = getBitValue(ptr, bitField); break;
            case BF_SRG:    pi.srg = getBitValue(ptr, bitField); break;
            case BF_RGM:    pi.rgm = getBitValue(ptr, bitField); break;
            default:
                break;
        }
        byteCount = MAX(byteCount, (bitField.byte + 1));
    }

    /*
     * READ EXTENDED BITFIELDS
     */
    bool hasDisp = desc.fields[BF_DISP].type == BF_DISP;
    bool hasData = desc.fields[BF_DATA].type == BF_DATA;
    bool hasAddr = desc.fields[BF_ADDR].type == BF_ADDR;
    bool hasInc8 = desc.fields[BF_INC8].type == BF_INC8;
    if(hasDisp) {
        // This is always a RGM-based read
        u8 bytesRead = 0;
        if(pi.mod == 0b01) {
            pi.dispLo = *(ptr + byteCount);
            pi.dispHi = 0;
            bytesRead = 1;
        } else if(pi.mod == 0b10 || (pi.mod == 0b00 && pi.rgm == 0b110)) {
            pi.dispLo = *(ptr + byteCount);
            pi.dispHi = *(ptr + byteCount + 1);
            bytesRead = 2;
        }
        byteCount += bytesRead;
    }
    if(hasData) {
        u8 bytesRead = 0;
        pi.dataLo = *(ptr + byteCount);
        bytesRead++;
        if(pi.s == 0 && pi.w == 1) {
            pi.dataHi = *(ptr + byteCount + 1);
            bytesRead++;
        }
        if(pi.s && pi.w) {
            // Add sign extension if needed
            s16 tmp = (s8) pi.dataLo;
            pi.dataHi = (tmp >> 8) & 0xFF;
        }
        byteCount += bytesRead;
    }
    if(hasAddr) {
        pi.addrLo = *(ptr + byteCount);
        pi.addrHi = *(ptr + byteCount + 1);
        byteCount += 2;
    }
    if(hasInc8) {
        pi.inc8 = *(ptr + byteCount);
        byteCount++;
    }
    pi.bytesRead = byteCount;

    /*
     * MAKE THE OPERANDS
     */
    bool hasReg = desc.fields[BF_REG].type == BF_REG;
    bool hasSrg = desc.fields[BF_SRG].type == BF_SRG;
    bool hasRgm = desc.fields[BF_RGM].type == BF_RGM;
    if(desc.isAccSrc || desc.isAccDst) {
        pi.reg = 0b000;
        hasReg = true;
    }

    // JMP_*
    if(pi.action == JMP && hasInc8) {
        pi.src.type = OPD_NOOP;  // Won't be printed
        pi.dst.type = OPD_INC8;
        pi.dst.inc8Opd.inc8 = (s8) pi.inc8;
        sprintfcat(pi.dst.str, "($+%d)%+d", byteCount, pi.dst.inc8Opd.inc8);
        return pi;
    }

    // MOV_MEM_ACC, MOV_ACC_MEM
    if(hasAddr) {
        if(desc.isAccSrc) {
            makeRegOpd(&pi.src, pi.reg, pi.w);
            makeAddrOpd(&pi.dst, pi.addrLo, pi.addrHi);
        } else {
            makeAddrOpd(&pi.src, pi.addrLo, pi.addrHi);
            makeRegOpd(&pi.dst, pi.reg, pi.w);
        }
        return pi;
    }

    // MOV_SRG_RGM (d=0), MOV_RGM_SRG (d=1)
    if(hasSrg && hasRgm) {
        if(pi.d == 0) {
            makeSrgOpd(&pi.src, pi.srg);
            makeRgmOpd(&pi.dst, pi.rgm, 1, pi.mod, false, pi.dispLo, pi.dispHi);
        } else {
            makeRgmOpd(&pi.src, pi.rgm, 1, pi.mod, false, pi.dispLo, pi.dispHi);
            makeSrgOpd(&pi.dst, pi.srg);
        }
        return pi;
    }

    // MOV_RGM_REG, ADD_RGM_REG, SUB_RGM_REG, CMP_RGM_REG
    if(hasReg && hasRgm) {
        if(pi.d == 0) {
            makeRegOpd(&pi.src, pi.reg, pi.w);
            makeRgmOpd(&pi.dst, pi.rgm, pi.w, pi.mod, false, pi.dispLo, pi.dispHi);
        } else {
            makeRgmOpd(&pi.src, pi.rgm, pi.w, pi.mod, false, pi.dispLo, pi.dispHi);
            makeRegOpd(&pi.dst, pi.reg, pi.w);
        }
        return pi;
    }

    // MOV_IMM_RGM, ADD_IMM_RGM, SUB_IMM_RGM, CMP_IMM_RGM
    if(hasData && hasDisp) {
        bool dstSize = desc.forceDstSize && (pi.mod != 0b11);
        bool srcSize = !dstSize && (pi.mod != 0b11);
        makeImmOpd(&pi.src, pi.w, pi.s, srcSize, pi.dataLo, pi.dataHi);
        makeRgmOpd(&pi.dst, pi.rgm, pi.w, pi.mod, dstSize, pi.dispLo, pi.dispHi);
        return pi;
    }

    // MOV_IMM_REG, ADD_IMM_ACC, SUB_IMM_ACC, CMP_IMM_ACC
    if(hasData && hasReg) {
        makeImmOpd(&pi.src, pi.w, pi.s, false, pi.dataLo, pi.dataHi);
        makeRegOpd(&pi.dst, pi.reg, pi.w);
        return pi;
    }

    fprintf(stderr, "Failed to parse instruction: %s", bytesToBinaryStr(ptr, 6));
    fprintf(stderr, "Got: %s hasReg=%s, hasRgm=%s, hasDisp=%s, hasData=%s, hasAddr=%s, hasInc8=%s", instTypeStrs[desc.type],
            BOOL_STR(hasReg), BOOL_STR(hasRgm), BOOL_STR(hasDisp), BOOL_STR(hasData), BOOL_STR(hasAddr), BOOL_STR(hasInc8));
    exit(1);
}

char* parsedInstToStr(ParsedInst parsed) {
    InstDesc desc = instructionDescriptors[parsed.type];
    char* str = (char*) next_scratch;
    char* sep = parsed.src.type == OPD_NOOP ? "" : ", ";
    sprintf(str, "%s %s%s%s", desc.instStr, parsed.dst.str, sep, parsed.src.str);
    next_scratch += strlen(str) + 1;
    return str;
}

int findInstOpCodeMatch(u8* ptr) {
    for (int j = 0; j < ID_COUNT; j++) {
        InstDesc desc = instructionDescriptors[j];
        if(desc.opCode1.where.type == BF_NONE) continue;
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
    fprintf(stderr, "ERROR: No instruction match for %s\n", bytesToBinaryStr(ptr, 6));
    exit(1);
}

u16 srgRead(Computer* comp, SegmentRegisterEnum srgIdx) {
    assert(srgIdx < SRG_COUNT);
    switch(srgIdx) {
        case SRG_ES: return comp->es.word;
        case SRG_CS: return comp->cs.word;
        case SRG_SS: return comp->ss.word;
        case SRG_DS: return comp->ds.word;
        case SRG_COUNT:
            fprintf(stderr, "ERROR: Invalid read segment register index %d (SRG_COUNT=%d)\n", srgIdx, SRG_COUNT);
            exit(1);
    }
}
u16 srgWrite(Computer* comp, SegmentRegisterEnum srgIdx, u16 value) {
    assert(srgIdx < SRG_COUNT);
    u16 oldValue = srgRead(comp, srgIdx);
    switch(srgIdx) {
        case SRG_ES: comp->es.word = value; break;
        case SRG_CS: comp->cs.word = value; break;
        case SRG_SS: comp->ss.word = value; break;
        case SRG_DS: comp->ds.word = value; break;
        case SRG_COUNT:
            fprintf(stderr, "ERROR: Invalid write segment register index %d (SRG_COUNT=%d)\n", srgIdx, SRG_COUNT);
            exit(1);
    }
    return oldValue;
}

u16 regRead(Computer* comp, RegisterEnum regIdx) {
    assert(regIdx < REG_COUNT);
    switch(regIdx) {
        case REG_AL: return comp->ax.lo;
        case REG_BL: return comp->bx.lo;
        case REG_CL: return comp->cx.lo;
        case REG_DL: return comp->dx.lo;
        case REG_AH: return comp->ax.hi;
        case REG_BH: return comp->bx.hi;
        case REG_CH: return comp->cx.hi;
        case REG_DH: return comp->dx.hi;
        case REG_AX: return comp->ax.word;
        case REG_BX: return comp->bx.word;
        case REG_CX: return comp->cx.word;
        case REG_DX: return comp->dx.word;
        case REG_SP: return comp->sp.word;
        case REG_BP: return comp->bp.word;
        case REG_SI: return comp->si.word;
        case REG_DI: return comp->di.word;
        case REG_COUNT:
            fprintf(stderr, "ERROR: Invalid read register index %d (REG_COUNT=%d)\n", regIdx, REG_COUNT);
            exit(1);
    }
}
u16 regWrite(Computer* comp, RegisterEnum regIdx, u16 value) {
    assert(regIdx < REG_COUNT);
    u16 oldValue = regRead(comp, regIdx);
    switch(regIdx) {
        case REG_AL: comp->ax.lo = value; break;
        case REG_BL: comp->bx.lo = value; break;
        case REG_CL: comp->cx.lo = value; break;
        case REG_DL: comp->dx.lo = value; break;
        case REG_AH: comp->ax.hi = value; break;
        case REG_BH: comp->bx.hi = value; break;
        case REG_CH: comp->cx.hi = value; break;
        case REG_DH: comp->dx.hi = value; break;
        case REG_AX: comp->ax.word = value; break;
        case REG_BX: comp->bx.word = value; break;
        case REG_CX: comp->cx.word = value; break;
        case REG_DX: comp->dx.word = value; break;
        case REG_SP: comp->sp.word = value; break;
        case REG_BP: comp->bp.word = value; break;
        case REG_SI: comp->si.word = value; break;
        case REG_DI: comp->di.word = value; break;
        case REG_COUNT:
            fprintf(stderr, "ERROR: Invalid write register index %d (REG_COUNT=%d)\n", regIdx, REG_COUNT);
            exit(1);
    }
    return oldValue;
}

bool flagsRead(Computer* comp, FlagsRegisterEnum flag) {
    assert(flag < FLAG_REG_COUNT);
    u16 mask = 1 << flagsBitPosition[flag];
    return (comp->flags & mask) != 0;
}
u16 flagsSet(Computer* comp, u16 value) {
    u16 oldValue = comp->flags;
    comp->flags = value;
    return oldValue;
}
char* flagsToStr(Computer* comp) {
    char* str = (char*) next_scratch;
    strcpy(str, "[                ]");
    int len = strlen(str);
    for(int i = 0; i < FLAG_REG_COUNT; i++) {
        bool isSet = (comp->flags >> flagsBitPosition[i]) & 0b1;
        int idx = len - 1 - flagsBitPosition[i];
        str[idx] = isSet ? flagsRegisterStrs[i] : ' ';
    }
    next_scratch += strlen(str) + 1;
    return str;
}

char* executeInst(Computer* comp, ParsedInst pi) {
    if(pi.src.type == OPD_NOOP) {
        return "OPD_NOOP src not supported (yet)";
    }

    u16 srcValue = 0;
    switch(pi.src.type) {
        case OPD_REG:  srcValue = regRead(comp, pi.src.regOpd.regIdx); break;
        case OPD_SRG:  srcValue = srgRead(comp, pi.src.srgOpd.srgIdx); break;
        case OPD_DISP: srcValue = pi.src.dispOpd.disp; break;
        case OPD_DATA: srcValue = pi.src.dataOpd.data; break;
        case OPD_ADDR: srcValue = pi.src.addrOpd.addr; break;
        case OPD_INC8: srcValue = pi.src.inc8Opd.inc8; break;
        case OPD_NOOP:
        case OPD_COUNT:
            break;
    }

    u16 oldValue = 0;
    u16 newValue = 0;
    const char* name = "";
    const char* flagsStr = "";
    switch(pi.action) {
        case MOV:
            if(pi.dst.type == OPD_REG) {
                // Break out reads to get parent values
                oldValue = regRead(comp, REGISTER_PARENT_MAP[pi.dst.regOpd.regIdx]);
                regWrite(comp, pi.dst.regOpd.regIdx, srcValue);
                newValue = regRead(comp, REGISTER_PARENT_MAP[pi.dst.regOpd.regIdx]);
                name = registerNameStrs[REGISTER_PARENT_MAP[pi.dst.regOpd.regIdx]];
            }
            if(pi.dst.type == OPD_SRG) {
                oldValue = srgWrite(comp, pi.dst.srgOpd.srgIdx, srcValue);
                name = segmentRegisterStrs[pi.dst.srgOpd.srgIdx];
            }
            break;
        case ADD:
            assert(pi.dst.type == OPD_REG);
            if(pi.dst.type == OPD_REG) {
                oldValue = regRead(comp, REGISTER_PARENT_MAP[pi.dst.regOpd.regIdx]);
                regWrite(comp, pi.dst.regOpd.regIdx, oldValue + srcValue);
                newValue = regRead(comp, REGISTER_PARENT_MAP[pi.dst.regOpd.regIdx]);
                name = registerNameStrs[REGISTER_PARENT_MAP[pi.dst.regOpd.regIdx]];
                flagsSet(comp, calcFlags(true, oldValue, srcValue, newValue));
                flagsStr = flagsToStr(comp);
            }
            break;
        case SUB:
            assert(pi.dst.type == OPD_REG);
            if(pi.dst.type == OPD_REG) {
                oldValue = regRead(comp, REGISTER_PARENT_MAP[pi.dst.regOpd.regIdx]);
                regWrite(comp, pi.dst.regOpd.regIdx, oldValue - srcValue);
                newValue = regRead(comp, REGISTER_PARENT_MAP[pi.dst.regOpd.regIdx]);
                name = registerNameStrs[REGISTER_PARENT_MAP[pi.dst.regOpd.regIdx]];
                flagsSet(comp, calcFlags(false, oldValue, srcValue, newValue));
                flagsStr = flagsToStr(comp);
            }
            break;
        case CMP:
            assert(pi.dst.type == OPD_REG);
            if(pi.dst.type == OPD_REG) {
                oldValue = regRead(comp, REGISTER_PARENT_MAP[pi.dst.regOpd.regIdx]);
                newValue = oldValue - srcValue;
                name = "";
                flagsSet(comp, calcFlags(false, oldValue, srcValue, newValue));
                flagsStr = flagsToStr(comp);
            }
            break;
        case JMP:
            break;
    }

    char* regStr = (char*) next_scratch;
    if(strlen(name) == 0) {
        strcat(regStr, " ");
    } else {
        sprintfcat(regStr, "%-4s: 0x%04x -> 0x%04x", name, oldValue, newValue);
    }
    next_scratch += strlen(regStr) + 1;
    char* str = (char*) next_scratch;
    sprintfcat(str, "%-30s %s", regStr, flagsStr);
    next_scratch += strlen(str) + 1;
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
        if(fileDataSize >= BUFF_SIZE) {
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
    ParsedInst program[MAX_INST_COUNT];
    int instCount = 0;
    for(int fileOffset = 0; fileOffset < fileDataSize; /* INCREMENT IN LOOP */) {
        memset(scratch, 0, BUFF_SIZE);
        next_scratch = scratch;

        u8* data = fileData + fileOffset;
        int descIdx = findInstOpCodeMatch(data);
        if (descIdx == -1) {
            fprintf(stderr, "Failed to match instruction for bytes: %s\n", bytesToBinaryStr(data, 6));
            exit(1);
        }

        InstDesc desc = instructionDescriptors[descIdx];
        ParsedInst parsed = parseInstruction(data, desc);
        if (parsed.bytesRead == 0) {
            fprintf(stderr, "Failed to parse instruction: %s: %s", instTypeStrs[desc.type], bytesToHexStr(data, 6));
            exit(1);
        }
        fileOffset += parsed.bytesRead;
        program[instCount++] = parsed;
    }

    // Run program
    Computer comp = { 0 };
    for(int i = 0; i < instCount; i++) {
        memset(scratch, 0, BUFF_SIZE);
        next_scratch = scratch;
        ParsedInst parsed = program[i];
        char* asmLine = parsedInstToStr(parsed);
        char* effect = executeInst(&comp, parsed);
        fprintf(out, "%-16s ; %s\n", asmLine, effect);
    }

    // Final state
    fprintf(out, "\nFinal registers:\n");
    RegisterEnum registers[] = { REG_AX, REG_BX, REG_CX, REG_DX, REG_SP, REG_BP, REG_SI, REG_DI };
    for(int i = 0; i < sizeof(registers) / sizeof (registers[0]); i++) {
        RegisterEnum ri = registers[i];
        u16 value = regRead(&comp, ri);
        fprintf(out, "    %s: 0x%04x (%d)\n", registerNameStrs[ri], value, value);
    }
    fprintf(out, "    ----\n");
    SegmentRegisterEnum  serRegisters[] = { SRG_ES, SRG_CS, SRG_SS, SRG_DS };
    for(int i = 0; i < sizeof(serRegisters) / sizeof (serRegisters[0]); i++) {
        SegmentRegisterEnum ri = serRegisters[i];
        u16 value = srgRead(&comp, ri);
        fprintf(out, "    %s: 0x%04x (%d)\n", segmentRegisterStrs[ri], value, value);
    }
    fprintf(out, "    ----\n");
    fprintf(out, " flags: %s\n", flagsToStr(&comp));
    return 0;
}
