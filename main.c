#include <assert.h>
#include <errno.h>
#include <libgen.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFF_SIZE       102400
#define MAX_INST_COUNT  128

#include "types.c"
#include "utils.c"

// #define DEFAULT_FILENAME    "hw/listing_0056_estimating_cycles"
#define DEFAULT_FILENAME    "hw/listing_0057_challenge_cycles"

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
ClockCounts calcClocks(ParsedInst pi, bool hasAddr) {
    bool foundClocks = false;
    InstClocks instClocks = { 0 };
    int count = sizeof(INST_CLOCKS) / sizeof(INST_CLOCKS[0]);
    for (int i = 0; i < count; i++) {
        bool isMatch =
            INST_CLOCKS[i].type == pi.type &&
            INST_CLOCKS[i].srcType == pi.src.type &&
            INST_CLOCKS[i].dstType == pi.dst.type;
        if (isMatch) {
            instClocks = INST_CLOCKS[i];
            foundClocks = true;
            break;
        }
    }
    if (!foundClocks) {
        fprintf(stderr, "ERROR: Failed to find clock information for INST %s[%d]: SRC=%s[%d], DST=%s[%d]\n", instTypeStrs[pi.type], pi.type, OPERAND_TYPE_STRS[pi.src.type], pi.src.type, OPERAND_TYPE_STRS[pi.dst.type], pi.dst.type);
        exit(1);
    }
    // fprintf(stdout, "Got clocks: %d (ea=%s)\n", clocks.clocks, clocks.needsEA ? "true" : "false");
    // Effective Address calculation from Table 2-20 in the 8086 Manual
    u8 eaClocks = 0;
    if (instClocks.needsEA) {
        bool nonZeroDisp = pi.dispLo != 0 || pi.dispHi != 0;
        u8 eaPath = ((0b11 & pi.mod) << 3) | (0b111 & pi.rgm);
        switch (eaPath) {
        case 0b00000: eaClocks = 7;  break;     // BX + SI
        case 0b00001: eaClocks = 8;  break;     // BX + DI
        case 0b00010: eaClocks = 8;  break;     // BP + SI
        case 0b00011: eaClocks = 7;  break;     // BP + DI
        case 0b00100: eaClocks = 5;  break;     // SI
        case 0b00101: eaClocks = 5;  break;     // DI
        case 0b00110: eaClocks = 6;  break;     // DIRECT ADDRESS
        case 0b00111: eaClocks = 5;  break;     // BX
        case 0b01000: eaClocks = 11; break;     // BX + SI + D8
        case 0b01001: eaClocks = 12; break;     // BX + DI + D8
        case 0b01010: eaClocks = 12; break;     // BP + SI + D8
        case 0b01011: eaClocks = 11; break;     // BP + DI + D8
        case 0b01100: eaClocks = 9;  break;     // SI + D8
        case 0b01101: eaClocks = 9;  break;     // DI + D8
        case 0b01110: eaClocks = nonZeroDisp ? 9 : 5; break;    // BP + D8
        case 0b01111: eaClocks = 9;  break;     // BX + D8
        case 0b10000: eaClocks = 11; break;     // BX + SI + D16
        case 0b10001: eaClocks = 12; break;     // BX + DI + D16
        case 0b10010: eaClocks = 12; break;     // BP + SI + D16
        case 0b10011: eaClocks = 11; break;     // BP + DI + D16
        case 0b10100: eaClocks = 9;  break;     // SI + D16
        case 0b10101: eaClocks = 9;  break;     // DI + D16
        case 0b10110: eaClocks = 9;  break;     // BP + D16
        case 0b10111: eaClocks = 9;  break;     // BX + D16
        default: {
            if (!pi.mod == 0b00) {
                fprintf(stdout, "WARNING: Not an EA instruction: mod=%d, rmg=%d, path=%s\n", pi.mod, pi.rgm, byteToBinaryStr(eaPath));
            }
            break;
        }
        }
    }
    u8 alignClocks = 0;
    if (hasAddr) {
        u16 addr = (pi.addrHi << 8) | pi.addrLo;
        alignClocks = (addr % 2) == 1 ? 4 : 0;
    }
    return (ClockCounts) {
        .base = instClocks.clocks,
        .ea = eaClocks,
        .oddAlign = alignClocks
    };
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
    }

    // MOV_MEM_ACC, MOV_ACC_MEM
    else if(hasAddr) {
        if(desc.isAccSrc) {
            makeRegOpd(&pi.src, pi.reg, pi.w);
            makeAddrOpd(&pi.dst, pi.addrLo, pi.addrHi);
        } else {
            makeAddrOpd(&pi.src, pi.addrLo, pi.addrHi);
            makeRegOpd(&pi.dst, pi.reg, pi.w);
        }
    }

    // MOV_SRG_RGM (d=0), MOV_RGM_SRG (d=1)
    else if(hasSrg && hasRgm) {
        if(pi.d == 0) {
            makeSrgOpd(&pi.src, pi.srg);
            makeRgmOpd(&pi.dst, pi.rgm, 1, pi.mod, false, pi.dispLo, pi.dispHi);
        } else {
            makeRgmOpd(&pi.src, pi.rgm, 1, pi.mod, false, pi.dispLo, pi.dispHi);
            makeSrgOpd(&pi.dst, pi.srg);
        }
    }

    // MOV_RGM_REG, ADD_RGM_REG, SUB_RGM_REG, CMP_RGM_REG
    else if(hasReg && hasRgm) {
        if(pi.d == 0) {
            makeRegOpd(&pi.src, pi.reg, pi.w);
            makeRgmOpd(&pi.dst, pi.rgm, pi.w, pi.mod, false, pi.dispLo, pi.dispHi);
        } else {
            makeRgmOpd(&pi.src, pi.rgm, pi.w, pi.mod, false, pi.dispLo, pi.dispHi);
            makeRegOpd(&pi.dst, pi.reg, pi.w);
        }
    }

    // MOV_IMM_RGM, ADD_IMM_RGM, SUB_IMM_RGM, CMP_IMM_RGM
    else if(hasData && hasDisp) {
        bool dstSize = desc.forceDstSize && (pi.mod != 0b11);
        bool srcSize = !dstSize && (pi.mod != 0b11);
        makeImmOpd(&pi.src, pi.w, pi.s, srcSize, pi.dataLo, pi.dataHi);
        makeRgmOpd(&pi.dst, pi.rgm, pi.w, pi.mod, dstSize, pi.dispLo, pi.dispHi);
    }

    // MOV_IMM_REG, ADD_IMM_ACC, SUB_IMM_ACC, CMP_IMM_ACC
    else if(hasData && hasReg) {
        makeImmOpd(&pi.src, pi.w, pi.s, false, pi.dataLo, pi.dataHi);
        makeRegOpd(&pi.dst, pi.reg, pi.w);
    }

    // ERROR
    else {
        fprintf(stderr, "Failed to parse instruction: %s", bytesToBinaryStr(ptr, 6));
        fprintf(stderr, "^^^ got: %s hasReg=%s, hasRgm=%s, hasDisp=%s, hasData=%s, hasAddr=%s, hasInc8=%s", instTypeStrs[desc.type],
                BOOL_STR(hasReg), BOOL_STR(hasRgm), BOOL_STR(hasDisp), BOOL_STR(hasData), BOOL_STR(hasAddr), BOOL_STR(hasInc8));
        exit(1);
    }

    // CALCULATE CLOCKS
    ClockCounts counts = calcClocks(pi, hasAddr);
    pi.baseClocks = counts.base;
    pi.eaClocks = counts.ea;
    pi.oddAlignClocks = counts.oddAlign;

    return pi;
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
    return -1;
}

u16 srgRead(Computer* comp, SegmentRegisterEnum srgIdx) {
    assert(srgIdx < SRG_COUNT);
    switch(srgIdx) {
        case SRG_ES: return comp->es.word;
        case SRG_CS: return comp->cs.word;
        case SRG_SS: return comp->ss.word;
        case SRG_DS: return comp->ds.word;
        case SRG_COUNT: break;
    }
    fprintf(stderr, "ERROR: Invalid read segment register index %d (SRG_COUNT=%d)\n", srgIdx, SRG_COUNT);
    exit(1);
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
        case REG_COUNT: break;
    }
    fprintf(stderr, "ERROR: Invalid read register index %d (REG_COUNT=%d)\n", regIdx, REG_COUNT);
    exit(1);
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
u16 calcEffectiveAddress(Computer* comp, EffectiveAddressEnum eaIdx, s16 disp) {
    assert(eaIdx <= EA_COUNT);
    u16 ea = 0;
    switch (eaIdx) {
    case EA_BX_SI: ea = regRead(comp, REG_BX) + regRead(comp, REG_SI); break;
    case EA_BX_DI: ea = regRead(comp, REG_BX) + regRead(comp, REG_DI); break;
    case EA_BP_SI: ea = regRead(comp, REG_BP) + regRead(comp, REG_SI); break;
    case EA_BP_DI: ea = regRead(comp, REG_BP) + regRead(comp, REG_DI); break;
    case EA_SI:    ea = regRead(comp, REG_SI); break;
    case EA_DI:    ea = regRead(comp, REG_DI); break;
    case EA_BP:    ea = regRead(comp, REG_BP); break;
    case EA_BX:    ea = regRead(comp, REG_BX); break;
    case EA_COUNT: break;
    }
    return ea + disp;
}
u16 memRead(Computer* comp, u16 addr, bool isWord) {
    u8 lo = comp->mem[addr];
    u8 hi = isWord ? comp->mem[addr+1] : 0;
    return (hi << 8) | lo;
}
u16 memWrite(Computer* comp, u16 addr, u16 value, bool isWord) {
    u8 oldLo = comp->mem[addr];
    u8 oldHi = isWord ? comp->mem[addr+1] : 0;
    u8 lo = value & 0xff;
    u8 hi = (value >> 8) & 0xff;
    comp->mem[addr] = lo;
    if (isWord) {
        comp->mem[addr+1] = hi;
    }
    return (oldHi << 8) | oldLo;
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

// ReSharper disable CppDFAUnreachableCode
char* executeInst(Computer *comp, ParsedInst* program, const int instCount) {
    assert(comp->ip.instIdx < instCount);
    u16 currentInstIdx = comp->ip.instIdx;
    ParsedInst pi = program[currentInstIdx];
    comp->ip.instIdx++;
    pi.oddAlignClocks = 0;  // TODO: We need an execution log, not resetting instruction data on each execution
    comp->ip.byteOffset += pi.bytesRead;
    char* execNotes = NULL;

    bool checkSrcAlign = pi.w && (pi.src.type == OPD_DISP || pi.src.type == OPD_ADDR);
    bool checkDstAlign = pi.w && (pi.dst.type == OPD_DISP || pi.dst.type == OPD_ADDR);

    u16 srcValue = 0;
    switch(pi.src.type) {
        case OPD_REG:  srcValue = regRead(comp, pi.src.regOpd.regIdx); break;
        case OPD_SRG:  srcValue = srgRead(comp, pi.src.srgOpd.srgIdx); break;
        case OPD_DISP: {
            u16 addr = calcEffectiveAddress(comp, pi.src.dispOpd.eaIdx, pi.src.dispOpd.disp);
            srcValue = memRead(comp, addr, pi.w);
            if (checkSrcAlign && addr % 2 == 1) {
                pi.oddAlignClocks += 4;
            }
        } break;
        case OPD_DATA: srcValue = pi.src.dataOpd.data; break;
        case OPD_ADDR: {
            srcValue = pi.src.addrOpd.addr;
            if (checkSrcAlign && pi.src.addrOpd.addr % 2 == 1) {
                pi.oddAlignClocks += 4;
            }
        } break;
        case OPD_INC8: srcValue = pi.src.inc8Opd.inc8; break;
        case OPD_NOOP:
        case OPD_COUNT:
            break;
    }


    switch(pi.action) {
        case MOV: {
            const char* dstName = "";
            u16 oldValue = 0;
            u16 newValue = 0;
            if(pi.dst.type == OPD_REG) {
                // Break out reads to get parent values
                oldValue = regRead(comp, REGISTER_PARENT_MAP[pi.dst.regOpd.regIdx]);
                regWrite(comp, pi.dst.regOpd.regIdx, srcValue);
                newValue = regRead(comp, REGISTER_PARENT_MAP[pi.dst.regOpd.regIdx]);
                dstName = registerNameStrs[REGISTER_PARENT_MAP[pi.dst.regOpd.regIdx]];
            } else if(pi.dst.type == OPD_SRG) {
                oldValue = srgWrite(comp, pi.dst.srgOpd.srgIdx, srcValue);
                dstName = segmentRegisterStrs[pi.dst.srgOpd.srgIdx];
            } else if (pi.dst.type == OPD_DISP) {
                u16 addr = calcEffectiveAddress(comp, pi.dst.dispOpd.eaIdx, pi.dst.dispOpd.disp);
                if (checkDstAlign && addr % 2 == 1) {
                    pi.oddAlignClocks += 4;
                }
                oldValue = memWrite(comp, addr, srcValue, pi.w);
                newValue = memRead(comp, addr, pi.w);
                dstName = asprintfcat(NULL, "[%d]", addr);
            } else {
                fprintf(stderr, "Unexpected MOV pi.dst.type=%d (%s)", pi.dst.type, OPERAND_TYPE_STRS[MAX(pi.dst.type, OPD_COUNT-1)]);
                assert(false);
            }
            execNotes = asprintfcat(execNotes, "%s:0x%04x->0x%04x ", dstName, oldValue, newValue);
        } break;
        case ADD: {
            const char* dstName = "";
            u16 oldValue = 0;
            u16 newValue = 0;
            if(pi.dst.type == OPD_REG) {
                oldValue = regRead(comp, REGISTER_PARENT_MAP[pi.dst.regOpd.regIdx]);
                regWrite(comp, pi.dst.regOpd.regIdx, oldValue + srcValue);
                newValue = regRead(comp, REGISTER_PARENT_MAP[pi.dst.regOpd.regIdx]);
                dstName = registerNameStrs[REGISTER_PARENT_MAP[pi.dst.regOpd.regIdx]];
                flagsSet(comp, calcFlags(true, oldValue, srcValue, newValue));
            } else if (pi.dst.type == OPD_DISP) {
                u16 addr = calcEffectiveAddress(comp, pi.dst.dispOpd.eaIdx, pi.dst.dispOpd.disp);
                if (checkDstAlign && addr % 2 == 1) {
                    pi.oddAlignClocks += 4;
                }
                oldValue = memRead(comp, addr, pi.w);
                memWrite(comp, addr, oldValue + srcValue, pi.w);
                newValue = memRead(comp, addr, pi.w);
                dstName = asprintfcat(NULL, "[%d]", addr);
            } else {
                fprintf(stderr, "Unexpected ADD pi.dst.type=%d (%s)", pi.dst.type, OPERAND_TYPE_STRS[MAX(pi.dst.type, OPD_COUNT-1)]);
                assert(false);
            }
            execNotes = asprintfcat(execNotes, "%s:0x%04x->0x%04x ", dstName, oldValue, newValue);
        } break;
        case SUB: {
            assert(pi.dst.type == OPD_REG);
            const char* regName = "";
            u16 oldValue = 0;
            u16 newValue = 0;
            if(pi.dst.type == OPD_REG) {
                oldValue = regRead(comp, REGISTER_PARENT_MAP[pi.dst.regOpd.regIdx]);
                regWrite(comp, pi.dst.regOpd.regIdx, oldValue - srcValue);
                newValue = regRead(comp, REGISTER_PARENT_MAP[pi.dst.regOpd.regIdx]);
                regName = registerNameStrs[REGISTER_PARENT_MAP[pi.dst.regOpd.regIdx]];
                flagsSet(comp, calcFlags(false, oldValue, srcValue, newValue));
            }
            execNotes = asprintfcat(execNotes, "%s:0x%04x->0x%04x ", regName, oldValue, newValue);
        } break;
        case CMP: {
            assert(pi.dst.type == OPD_REG);
            u16 oldValue = 0;
            u16 newValue = 0;
            if(pi.dst.type == OPD_REG) {
                oldValue = regRead(comp, REGISTER_PARENT_MAP[pi.dst.regOpd.regIdx]);
                newValue = oldValue - srcValue;
                flagsSet(comp, calcFlags(false, oldValue, srcValue, newValue));
            }
            execNotes = asprintfcat(execNotes, "0x%04x%s0x%04x ", oldValue, flagsRead(comp,ZF) ? "==" : "!=", newValue);
        } break;
        case JMP: {
            assert(pi.dst.type == OPD_INC8);
            const char* regName = "";
            u16 oldValue = 0;
            u16 newValue = 0;

            // For logic, look up Table 2-15
            if (pi.type == JMP_LOOP || pi.type == JMP_LOOPZ || pi.type == JMP_LOOPNZ) {
                oldValue = regRead(comp, REG_CX);
                newValue = oldValue - 1;
                regWrite(comp, REG_CX, newValue);
                regName = registerNameStrs[REG_CX];
                execNotes = asprintfcat(execNotes, "%s:0x%04x->0x%04x ", regName, oldValue, newValue);
            }
            u8 cf = flagsRead(comp, CF);
            u8 zf = flagsRead(comp, ZF);
            u8 sf = flagsRead(comp, SF);
            u8 of = flagsRead(comp, OF);
            u8 pf = flagsRead(comp, PF);
            u16 cx = regRead(comp, REG_CX);
            bool makeJump =
                    (pi.type == JMP_JZ     && zf == 1) ||
                    (pi.type == JMP_JNZ    && zf == 0) ||
                    (pi.type == JMP_JL     && (sf ^ of) == 1) ||
                    (pi.type == JMP_JNL    && (sf ^ of) == 0) ||
                    (pi.type == JMP_JG     && (zf | (sf ^ of)) == 0) ||
                    (pi.type == JMP_JNG    && (zf | (sf ^ of)) == 1) ||
                    (pi.type == JMP_JB     && cf == 1) ||
                    (pi.type == JMP_JNB    && cf == 0) ||
                    (pi.type == JMP_JA     && (cf | zf) == 0) ||
                    (pi.type == JMP_JNA    && (cf | zf) == 1) ||
                    (pi.type == JMP_JP     && pf == 1) ||
                    (pi.type == JMP_JNP    && pf == 0) ||
                    (pi.type == JMP_JO     && of == 1) ||
                    (pi.type == JMP_JNO    && of == 0) ||
                    (pi.type == JMP_JS     && sf == 1) ||
                    (pi.type == JMP_JNS    && sf == 0) ||
                    (pi.type == JMP_JCXZ   && cx == 0) ||
                    (pi.type == JMP_LOOP   && cx != 0) ||
                    (pi.type == JMP_LOOPZ  && (cx != 0 && zf == 1)) ||
                    (pi.type == JMP_LOOPNZ && (cx != 0 && zf == 0));
            if(!makeJump) {
                execNotes = asprintfcat(execNotes, "not taking jmp %d ", pi.dst.inc8Opd.inc8);
                break;
            }
            execNotes = asprintfcat(execNotes, "taking jmp %d ", pi.dst.inc8Opd.inc8);

            s8 incVal = pi.dst.inc8Opd.inc8;
            s8 incDir = incVal < 0 ? -1 : 1;
            incVal = incVal < 0 ? -incVal : incVal;
            s16 bytesJumped = 0;
            do {
                // Check up top to handle `jmp 0`
                if(bytesJumped == incVal) { break; }
                if(bytesJumped > incVal) {
                    fprintf(stderr, "ERROR: Jumped too far! Instruction '%s', jumped %+d bytes\n", parsedInstToStr(pi), bytesJumped);
                    exit(1);
                }
                u16 instSize = 0;
                if (incDir > 0) {
                    instSize = program[comp->ip.instIdx].bytesRead;
                    comp->ip.instIdx += incDir;
                } else {
                    comp->ip.instIdx += incDir;
                    instSize = program[comp->ip.instIdx].bytesRead;
                }
                comp->ip.byteOffset += incDir * instSize;
                bytesJumped += instSize;
            } while (comp->ip.instIdx >= 0 && comp->ip.instIdx < instCount);
        } break;
    }

    comp->totalClocks += pi.baseClocks + pi.eaClocks + pi.oddAlignClocks;
    program[currentInstIdx].oddAlignClocks = pi.oddAlignClocks;

    return execNotes;
}

char* statusToStr(Computer* comp, ParsedInst currentInst, bool ip, bool clocks, bool genReg, bool extReg, bool segReg, bool flags, u8 memCount, u16 addr) {
    char *str = (char *) next_scratch;
    next_scratch += 512;
    if(ip) {
        sprintfcat(str, "ip[0x%02x:%02d] ", comp->ip.byteOffset, comp->ip.instIdx);
    }
    if (clocks) {
        u16 piClocks = currentInst.baseClocks + currentInst.eaClocks + currentInst.oddAlignClocks;
        sprintfcat(str, "Clocks %3d [%2d clocks (base=%2u, ea=%2u, align=%2u)]; ", comp->totalClocks, piClocks, currentInst.baseClocks, currentInst.eaClocks, currentInst.oddAlignClocks);
    }
    if(genReg || extReg) {
        sprintfcat(str, "[");
        if(genReg) {
            u16 ax = regRead(comp, REG_AX),
                bx = regRead(comp, REG_BX),
                cx = regRead(comp, REG_CX),
                dx = regRead(comp, REG_DX);
            sprintfcat(str, "%04x %04x %04x %04x", ax, bx, cx, dx);
        }
        if(genReg && extReg) {
            sprintfcat(str, " ");
        }
        if(extReg) {
            u16 sp = regRead(comp, REG_SP),
                bp = regRead(comp, REG_BP),
                si = regRead(comp, REG_SI),
                di = regRead(comp, REG_DI);
            sprintfcat(str, "%04x %04x %04x %04x", sp, bp, si, di);
        }
        sprintfcat(str, "] ");
    }
    if(segReg) {
        u16 es = srgRead(comp, SRG_ES),
            cs = srgRead(comp, SRG_CS),
            ss = srgRead(comp, SRG_SS),
            ds = srgRead(comp, SRG_DS);
        sprintfcat(str, "[%04x %04x %04x %04x] ", es, cs, ss, ds);
    }
    if(flags) {
        const char* flagsStr = flagsToStr(comp);
        sprintfcat(str, "%s ", flagsStr);
    }
    if (memCount > 0) {
        sprintfcat(str, "%05d:[%s] ", addr, bytesToHexStr(&comp->mem[addr], memCount));
    }
    return str;
}

void dumpMem(Computer* comp, u16 offset, u16 size, const char* filename) {
    FILE* file = fopen(filename, "wb");
    if (file == NULL) {
        fprintf(stderr, "Failed to open file %s: %s\n", filename, strerror(errno));
        exit(1);
    }
    u32 writtenBytes = fwrite(&comp->mem[offset], 1, size, file);
    if (writtenBytes != size) {
        fprintf(stderr, "Failed writing to %s: expected %d bytes, wrote %d bytes\n", filename, size, writtenBytes);
        exit(1);
    }
}

int main(int argc, const char** argv) {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    const char* filename = DEFAULT_FILENAME;
    for(int i = 1; i < argc; i++) {
        const char* param = argv[i];
        if(strcmp(param, "-disasm") == 0) {
            ProgState.params_printDisasm = true;
        } else if(strcmp(param, "-clocks") == 0) {
            ProgState.params_printClocks = true;
        } else if(strcmp(param, "-exec") == 0) {
            ProgState.params_execComputer = true;
        } else if (strcmp(param, "-dump") == 0) {
            ProgState.params_dumpMemory = true;
            if (argc > i + 1) {
                const char* dumpFile = argv[i+1];
                strncpy(ProgState.dumpFilename, dumpFile, 256);
            }
        } else {
            filename = param;
        }
    }
    if(ProgState.params_printDisasm == false && ProgState.params_execComputer == false) {
        const char* progName = basename((char*) argv[0]);
        fprintf(stdout, "Usage: %s (-disasm|-exec) [-clocks] [-dump <dumpFilename>] [filename]\n", progName);
        fprintf(stdout, "  -disasm    Print disassembly\n");
        fprintf(stdout, "  -exec      Execute program\n");
        fprintf(stdout, "  -clocks    Print clock cycles (requires -disasm or -exec)\n");
        fprintf(stdout, "  -dump      Dump memory contents to <dumpFilename>\n");
        fprintf(stdout, "  filename   Input file (default: %s)\n", DEFAULT_FILENAME);
        exit(0);
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
    if(ProgState.params_printDisasm) {
        fprintf(out, ";;;; Disassembly of %s\n", filename);
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
            fprintf(stderr, "Failed to match instruction %d for bytes: %s\n", instCount, bytesToBinaryStr(data, 6));
            exit(1);
        }

        InstDesc desc = instructionDescriptors[descIdx];
        ParsedInst parsed = parseInstruction(data, desc);
        if (parsed.bytesRead == 0) {
            fprintf(stderr, "Failed to parse instruction: %s: %s", instTypeStrs[desc.type], bytesToHexStr(data, 6));
            exit(1);
        }
        if(ProgState.params_printDisasm) {
            u8 clocks = parsed.baseClocks + parsed.eaClocks + parsed.oddAlignClocks;
            fprintf(out, "%-30s; [0x%03x:%02d] [size %d] [%2d clocks (base=%u, ea=%u, align=%u)]\n", parsedInstToStr(parsed), fileOffset, instCount, parsed.bytesRead, clocks, parsed.baseClocks, parsed.eaClocks, parsed.oddAlignClocks);
        }
        fileOffset += parsed.bytesRead;
        program[instCount++] = parsed;
    }

    if(ProgState.params_execComputer) {
        fprintf(out, "%s:::: Execution of %s\n", ProgState.params_printDisasm ? "\n\n" : "", filename);

        // Run program
        Computer comp = {0};
        for (comp.ip.instIdx = 0; comp.ip.instIdx < instCount; /* INCREMENT IN LOOP/EXECUTE */) {
            memset(scratch, 0, BUFF_SIZE);
            next_scratch = scratch;
            u16 currentInstIdx = comp.ip.instIdx;
            char *asmLine = parsedInstToStr(program[currentInstIdx]);
            char *notes = executeInst(&comp, program, instCount);
            char *status = statusToStr(&comp, program[currentInstIdx], true, ProgState.params_printClocks, true, true, false, false, 0, 0);
            fprintf(out, "%-30s ; %s    %s\n", asmLine, status, notes != NULL ? notes : "");
        }

        // Print final state
        {
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
            fprintf(out, "    ip: 0x%04x (%03d) idx=%d\n", comp.ip.byteOffset, comp.ip.byteOffset, comp.ip.instIdx);
            fprintf(out, " flags: %s\n", flagsToStr(&comp));
        }

        // Dump state
        if (ProgState.params_dumpMemory) {
            int offset = 64*4;
            int size = 64*64*4;
            dumpMem(&comp, offset, size, ProgState.dumpFilename);
            fprintf(out, "Memory dumped: offset=0x%04x, size=%d bytes, filename=%s\n", offset, size, ProgState.dumpFilename);
        }
    }

    return 0;
}
