#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFF_SIZE       10240
#define MAX_INST_COUNT  128

#include "types.c"
#include "utils.c"

//#define DEFAULT_FILENAME    "hw/listing_0037_single_register_mov"
//#define DEFAULT_FILENAME    "hw/listing_0038_many_register_mov"
//#define DEFAULT_FILENAME    "hw/listing_0039_more_movs"
//#define DEFAULT_FILENAME    "hw/listing_0040_challenge_movs"
//#define DEFAULT_FILENAME    "hw/listing_0041_add_sub_cmp_jnz"
//#define DEFAULT_FILENAME    "hw/listing_0043_immediate_movs"
#define DEFAULT_FILENAME    "hw/listing_0044_register_movs"
//#define DEFAULT_FILENAME    "hw/listing_0045_challenge_register_movs"

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
        sprintfcat(op->str, "[%d]", op->dispOpd.disp);
        return;
    }

    op->dispOpd.eaIdx = rgm;
    op->dispOpd.disp = mod == 0b01 ? (s8) dispLo : (dispHi << 8) | dispLo;
    op->dispOpd.sign = op->dispOpd.disp >= 0 ? '+' : '-';
    op->dispOpd.disp = op->dispOpd.disp < 0 ? -op->dispOpd.disp : op->dispOpd.disp;
    sprintfcat(op->str, "[%s", effectiveAddrStrs[op->dispOpd.eaIdx]);
    if(op->dispOpd.disp != 0) {
        sprintfcat(op->str, " %c %d", op->dispOpd.sign, op->dispOpd.disp);
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

u16 regRead(Computer* comp, RegisterEnum regIdx) {
    switch(regIdx) {
        case REG_AL: return (comp->ax & 0x00ff);
        case REG_BL: return (comp->bx & 0x00ff);
        case REG_CL: return (comp->cx & 0x00ff);
        case REG_DL: return (comp->dx & 0x00ff);
        case REG_AH: return (comp->ax & 0xff00) >> 8;
        case REG_BH: return (comp->bx & 0xff00) >> 8;
        case REG_CH: return (comp->cx & 0xff00) >> 8;
        case REG_DH: return (comp->dx & 0xff00) >> 8;
        case REG_AX: return comp->ax;
        case REG_BX: return comp->bx;
        case REG_CX: return comp->cx;
        case REG_DX: return comp->dx;
        case REG_SP: return comp->sp;
        case REG_BP: return comp->bp;
        case REG_SI: return comp->si;
        case REG_DI: return comp->di;
        case REG_COUNT:
            fprintf(stderr, "ERROR: Invalid register index %d (REG_COUNT=%d)\n", regIdx, REG_COUNT);
            exit(1);
    }
}
u16 regWrite(Computer* comp, RegisterEnum regIdx, u16 value) {
    s16 oldValue = 0;
    switch(regIdx) {
        case REG_AL: oldValue = (comp->ax & 0x00ff); comp->ax = (comp->ax & 0xff00) | (value & 0x00ff); break;
        case REG_BL: oldValue = (comp->bx & 0x00ff); comp->bx = (comp->bx & 0xff00) | (value & 0x00ff); break;
        case REG_CL: oldValue = (comp->cx & 0x00ff); comp->cx = (comp->cx & 0xff00) | (value & 0x00ff); break;
        case REG_DL: oldValue = (comp->dx & 0x00ff); comp->dx = (comp->dx & 0xff00) | (value & 0x00ff); break;
        case REG_AH: oldValue = (comp->ax & 0xff00) >> 8; comp->ax = (comp->ax & 0x00ff) | (value & 0xff00); break;
        case REG_BH: oldValue = (comp->bx & 0xff00) >> 8; comp->bx = (comp->bx & 0x00ff) | (value & 0xff00); break;
        case REG_CH: oldValue = (comp->cx & 0xff00) >> 8; comp->cx = (comp->cx & 0x00ff) | (value & 0xff00); break;
        case REG_DH: oldValue = (comp->dx & 0xff00) >> 8; comp->dx = (comp->dx & 0x00ff) | (value & 0xff00); break;
        case REG_AX: oldValue = comp->ax; comp->ax = value; break;
        case REG_BX: oldValue = comp->bx; comp->bx = value; break;
        case REG_CX: oldValue = comp->cx; comp->cx = value; break;
        case REG_DX: oldValue = comp->dx; comp->dx = value; break;
        case REG_SP: oldValue = comp->sp; comp->sp = value; break;
        case REG_BP: oldValue = comp->bp; comp->bp = value; break;
        case REG_SI: oldValue = comp->si; comp->si = value; break;
        case REG_DI: oldValue = comp->di; comp->di = value; break;
        case REG_COUNT:
            fprintf(stderr, "ERROR: Invalid register index %d (REG_COUNT=%d)\n", regIdx, REG_COUNT);
            exit(1);
    }
    return oldValue;
}



char* executeInst(Computer* comp, ParsedInst pi) {
    char* str = next_scratch;

    switch(pi.action) {
        case MOV:
            if(pi.dst.type == OPD_REG && pi.src.type == OPD_DATA) {
                RegisterEnum regIdx = pi.dst.regOpd.regIdx;
                u16 newValue = pi.src.dataOpd.data;
                u16 oldValue = regWrite(comp, regIdx, newValue);
                sprintfcat(str, "%s: 0x%02x -> 0x%02x", registerNameStrs[regIdx], oldValue, newValue);
            }
            if(pi.dst.type == OPD_REG && pi.src.type == OPD_REG) {
                RegisterEnum dstReg = pi.dst.regOpd.regIdx;
                RegisterEnum srcReg = pi.src.regOpd.regIdx;
                u16 srcValue = regRead(comp, srcReg);
                u16 oldValue = regWrite(comp, dstReg, srcValue);
                sprintfcat(str, "%s: 0x%02x -> 0x%02x", registerNameStrs[dstReg], oldValue, srcValue);
            }
            break;
        case ADD:
            break;
        case SUB:
            break;
        case CMP:
            break;
        case JMP:
            break;
    }

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
        fprintf(out, "%s    ; %s\n", asmLine, effect);
    }

    // Final state
    RegisterEnum registers[] = { REG_AX, REG_BX, REG_CX, REG_DX, REG_SP, REG_BP, REG_SI, REG_DI };
    fprintf(out, "\nFinal registers:\n");
    for(int i = 0; i < sizeof(registers) / sizeof (registers[0]); i++) {
        RegisterEnum ri = registers[i];
        u16 value = regRead(&comp, ri);
        fprintf(out, "    %s: 0x%04x (%d)\n", registerNameStrs[ri], value, value);
    }
    return 0;
}
