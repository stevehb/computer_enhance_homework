#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFF_SIZE           10240

#include "types.c"
#include "utils.c"


//#define DEFAULT_FILENAME    "hw/listing_0037_single_register_mov"
//#define DEFAULT_FILENAME    "hw/listing_0038_many_register_mov"
//#define DEFAULT_FILENAME    "hw/listing_0039_more_movs"
#define DEFAULT_FILENAME    "hw/listing_0040_challenge_movs"
//#define DEFAULT_FILENAME    "hw/listing_0041_add_sub_cmp_jnz"
//#define DEFAULT_FILENAME "tester"


const char* getRegStr(u8 reg, u8 w) {
    u8 ident = (w << 3) | reg;
    switch (ident) {
        case 0b0000: return "al";
        case 0b0001: return "cl";
        case 0b0010: return "dl";
        case 0b0011: return "bl";
        case 0b0100: return "ah";
        case 0b0101: return "ch";
        case 0b0110: return "dh";
        case 0b0111: return "bh";
        case 0b1000: return "ax";
        case 0b1001: return "cx";
        case 0b1010: return "dx";
        case 0b1011: return "bx";
        case 0b1100: return "sp";
        case 0b1101: return "bp";
        case 0b1110: return "si";
        case 0b1111: return "di";
        default: {
            fprintf(stderr, "Unknown values for reg=%d and w=%d", reg, w);
            exit(1);
        }
    }
}
int writeRegStr(u8 reg, u8 w, char *outStr) {
    strcpy(outStr, getRegStr(reg, w));
    return 0;
}


int writeRgmStr(u8 *ptr, u8 w, u8 mod, u8 rgm, u8 writeSize, char *outStr) {
    int extraBytesRead = 0;

    if(writeSize) {
        sprintf(outStr + strlen(outStr), "%s ", w ? "word" : "byte");
    }

    // Handle simple register interpretation
    if(mod == 0b11) {
        extraBytesRead = writeRegStr(rgm, w, outStr + strlen(outStr));
        return extraBytesRead;
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
        sprintf(outStr + strlen(outStr), "[%d]", (byte4 << 8) | byte3);
        return extraBytesRead;
    }


    // Set prefix
    switch(rgm) {
    case 0b000: sprintf(outStr + strlen(outStr), "[bx + si"); break;
    case 0b001: sprintf(outStr + strlen(outStr), "[bx + di"); break;
    case 0b010: sprintf(outStr + strlen(outStr), "[bp + si"); break;
    case 0b011: sprintf(outStr + strlen(outStr), "[bp + di"); break;
    case 0b100: sprintf(outStr + strlen(outStr), "[si"); break;
    case 0b101: sprintf(outStr + strlen(outStr), "[di"); break;
    case 0b110: sprintf(outStr + strlen(outStr), "[bp"); break;
    case 0b111: sprintf(outStr + strlen(outStr), "[bx"); break;
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
s16 getImmediateValue(u8* ptr, u8 isWord) {
    s16 imm = 0;
    s8 loByte = *ptr;
    if(!isWord) {
        imm = loByte;
    } else {
        s8 hiByte = *(ptr+1);
        imm = (hiByte << 8) | (u8) loByte;
    }
    return imm;
}
int writeImmediate(u8* ptr, u8 s, u8 w, bool isImmAddr, bool writeSize, char* outStr) {
    int extraBytesRead = 0;
    char* addrL = isImmAddr ? "[" : "";
    char* addrR = isImmAddr ? "]" : "";
    char* sizeSpec = "";
    bool isWord = w == 1 && s == 0;
    s16 imm = 0;
    s8 byte1 = *ptr;
    extraBytesRead++;
    if(!isWord) {
        imm = byte1;
        sizeSpec = writeSize ? "byte " : "";
    } else {
        s8 byte2 = *(ptr+1);
        extraBytesRead++;
        imm = (s16) ((byte2 << 8) | (u8) byte1);
        sizeSpec = writeSize ? "word " : "";
    }
    sprintf(outStr, "%s%s%d%s", addrL, sizeSpec, imm, addrR);
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

ParsedInst parseInstruction(u8* ptr, InstDesc desc) {
    ParsedInst pi = { 0 };
    pi.id = desc.type;

    u8 opCodeBytes = 0;
    for(int i = 0; i < BF_COUNT; i++) {
        Bits bitField = desc.fields[i];
        if(bitField.type == BF_NONE || bitField.type == BF_COUNT) continue;
        u8 value = getBitValue(ptr, bitField);
        switch(bitField.type) {
            case BF_NONE:
            case BF_LIT:
            case BF_COUNT:  break;
            case BF_SIGN:   pi.hasS = true;   pi.s = value;   break;
            case BF_WORD:   pi.hasW = true;   pi.w = value;   break;
            case BF_DIR:    pi.hasD = true;   pi.d = value;   break;
            case BF_MOD:    pi.hasMod = true; pi.mod = value; break;
            case BF_REG:    pi.hasReg = true; pi.reg = value; break;
            case BF_RGM:    pi.hasRgm = true; pi.rgm = value; break;
        }
        opCodeBytes = MAX(opCodeBytes, (bitField.byte + 1));
    }

    u8 additionalBytes = 0;
    if(pi.hasReg && pi.hasRgm) {
        if(pi.d == 0) {
            additionalBytes += writeRegStr(pi.reg, pi.w, pi.src);
            additionalBytes += writeRgmStr(ptr, pi.w, pi.mod, pi.rgm, desc.forceDstSize, pi.dst);
        } else {
            additionalBytes += writeRgmStr(ptr, pi.w, pi.mod, pi.rgm, false, pi.src);
            additionalBytes += writeRegStr(pi.reg, pi.w, pi.dst);
        }
    }

    // IMMEDIATE is always the last value, so read the REG/RGM first
    else if(desc.hasImm && pi.hasReg) {
        additionalBytes += writeRegStr(pi.reg, pi.w, pi.dst);
        additionalBytes += writeImmediate(ptr + opCodeBytes + additionalBytes, pi.s, pi.w, desc.isImmAddr, false, pi.src);
    }

    else if(desc.hasImm && pi.hasRgm) {
        // FIXME: the srcNeedsSize needs to be false if dstNeedsSize is true, otherwise depends on pi.mod
        bool dstNeedsSize = desc.forceDstSize;
        bool srcNeedsSize = !dstNeedsSize && (pi.mod != 0b11);
        additionalBytes += writeRgmStr(ptr, pi.w, pi.mod, pi.rgm, dstNeedsSize, pi.dst);
        additionalBytes += writeImmediate(ptr + opCodeBytes + additionalBytes, pi.s, pi.w, desc.isImmAddr, srcNeedsSize, pi.src);
    }

    else if(desc.hasImm && desc.isAccSrc) {
        additionalBytes += writeRegStr(0b000, pi.w, pi.src);
        additionalBytes += writeImmediate(ptr + opCodeBytes + additionalBytes, pi.s, pi.w, desc.isImmAddr, false, pi.dst);
    }

    else if(desc.hasImm && desc.isAccDst) {
        additionalBytes += writeRegStr(0b000, pi.w, pi.dst);
        additionalBytes += writeImmediate(ptr + opCodeBytes + additionalBytes, pi.s, pi.w, desc.isImmAddr, false, pi.src);
    }

    else if(desc.isAccSrc || desc.isAccDst) {
        additionalBytes += writeRegStr(0b000, pi.w, desc.isAccSrc ? pi.src : pi.dst);
        additionalBytes += writeImmediate(ptr + opCodeBytes + additionalBytes, pi.s, pi.w, desc.isImmAddr, pi.hasRgm, desc.isAccSrc ? pi.dst : pi.src);
    }

    pi.bytesRead = opCodeBytes + additionalBytes;
    return pi;
}

char* parsedInstToStr(ParsedInst parsed) {
    InstDesc desc = instructionDescriptors[parsed.id];
    char* str = (char*) next_scratch;
    sprintf(str, "%s %s, %s", desc.instStr, parsed.dst, parsed.src);
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
    int instCount = 0;
    for(int i = 0; i < fileDataSize; /* INCREMENT IN LOOP */) {
        memset(scratch, 0, BUFF_SIZE);
        next_scratch = scratch;

        u8* data = fileData + i;
//        fprintf(out, "[%d] Next inst: %s %s %s %s\n", i, byteToBinaryStr(*data), byteToBinaryStr(*(data+1)), byteToBinaryStr(*(data+2)), byteToBinaryStr(*(data+3)));
        int descIdx = findInstOpCodeMatch(data);
        if (descIdx == -1) {
            fprintf(stderr, "Failed to match instruction for bytes: %s\n", bytesToBinaryStr(data, 6));
            exit(1);
        }
//        fprintf(out, "[%d] Found %s\n", i, instTypeStrs[descIdx]);

        InstDesc desc = instructionDescriptors[descIdx];
        ParsedInst parsed = parseInstruction(data, desc);
        if (parsed.bytesRead == 0) {
            fprintf(stderr, "Failed to parse instruction: %s: %s", instTypeStrs[desc.type], bytesToHexStr(data, 6));
            exit(1);
        }
        i += parsed.bytesRead;

        char* line = parsedInstToStr(parsed);
//        fprintf(out, "%03d: %s\n", instCount, line);
        fprintf(out, "%s\n", line);
        instCount++;
    }

    return 0;
}
