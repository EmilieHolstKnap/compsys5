#include "./assembly.h"
#include "./memory.h"
#include "./simulate.h"
#include "./read_exec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


// Enum for instruction formats
typedef enum {
    R_TYPE,
    I_TYPE,
    S_TYPE,
    SB_TYPE,
    U_TYPE,
    UJ_TYPE
} InstructionType;

// Enum for opcodes 
typedef enum {
    OPCODE_R      = 0b00110011, 
    OPCODE_I_LOAD = 0b00000011, 
    OPCODE_I_IMM  = 0b00010011, 
    OPCODE_S      = 0b00100011, 
    OPCODE_SB     = 0b01100011, 
    OPCODE_U      = 0b00110111, 
    OPCODE_UJ     = 0b01101111, 
} Opcode;

typedef struct {
    InstructionType type;
    Opcode opcode;
    uint8_t funct3;
    uint8_t funct7;
    uint8_t rs1;
    uint8_t rs2;
    uint8_t rd;
    int32_t imm; 
} DecodedInstruction;

// Shift instruction to start and apply mask 
// Mask is 1 shifted by length - 1:  0x00001 -> (shifted by fx length = 3) -> 0x01000 -> (-1) -> 0x00111
uint32_t extract_bits(uint32_t instruction, int start, int length) {
    return (instruction >> start) & ((1 << length) - 1);
}

DecodedInstruction decode_instruction(uint32_t instruction) {
    DecodedInstruction decoded = {0};

    decoded.opcode = extract_bits(instruction, 0, 7);
    switch (decoded.opcode) {
        case OPCODE_R:
            decoded.type = R_TYPE;
            decoded.rd = extract_bits(instruction, 7, 5);
            decoded.funct3 = extract_bits(instruction, 12, 3);
            decoded.rs1 = extract_bits(instruction, 15, 5);
            decoded.rs2 = extract_bits(instruction, 20, 5);
            decoded.funct7 = extract_bits(instruction, 25, 7);
            break;
        case OPCODE_I_LOAD:
        case OPCODE_I_IMM:
            decoded.type = I_TYPE;
            decoded.rd = extract_bits(instruction, 7, 5);
            decoded.funct3 = extract_bits(instruction, 12, 3);
            decoded.rs1 = extract_bits(instruction, 15, 5);
            decoded.imm = decode_imm(instruction, I_TYPE);
            break;
        case OPCODE_S:
            decoded.type = S_TYPE;
            decoded.funct3 = extract_bits(instruction, 12, 3);
            decoded.rs1 = extract_bits(instruction, 15, 5);
            decoded.rs2 = extract_bits(instruction, 20, 5);
            decoded.imm = decode_imm(instruction, S_TYPE);
            break;
        case OPCODE_SB:
            decoded.type = SB_TYPE;
            decoded.funct3 = extract_bits(instruction, 12, 3);
            decoded.rs1 = extract_bits(instruction, 15, 5);
            decoded.rs2 = extract_bits(instruction, 20, 5);
            decoded.imm = decode_imm(instruction, SB_TYPE);
            break;
        case OPCODE_U:
            decoded.type = U_TYPE;
            decoded.rd = extract_bits(instruction, 7, 5);
            decoded.imm = decode_imm(instruction, U_TYPE);
            break;
        case OPCODE_UJ:
            decoded.type = UJ_TYPE;
            decoded.rd = extract_bits(instruction, 7, 5);
            decoded.imm = decode_imm(instruction, UJ_TYPE);
            break;
        default:
            break;
    }
    return decoded;
}

const char* get_mnemonic(DecodedInstruction decoded) {
    switch (decoded.opcode) {
        case OPCODE_R: 
            switch (decoded.funct3) {
                case 0b000:
                    return (decoded.funct7 == 0x00) ? "ADD" :
                           (decoded.funct7 == 0x20) ? "SUB" : "UNKNOWN_R";
                case 0b001:
                    return "SLL"; 
                case 0b010:
                    return "SLT"; 
                case 0b011:
                    return "SLTU"; 
                case 0b100:
                    return "XOR";
                case 0b101:
                    return (decoded.funct7 == 0x00) ? "SRL" :
                           (decoded.funct7 == 0x20) ? "SRA" : "UNKNOWN_R";
                case 0b110:
                    return "OR";
                case 0b111:
                    return "AND";
                default:
                    return "UNKNOWN_R";
            }

        case OPCODE_I_LOAD: 
            switch (decoded.funct3) {
                case 0b000: return "LB";  
                case 0b001: return "LH";  
                case 0b010: return "LW";  
                case 0b100: return "LBU"; 
                case 0b101: return "LHU"; 
                default:
                    return "UNKNOWN_LOAD";
            }

        case OPCODE_I_IMM: 
            switch (decoded.funct3) {
                case 0b000: return "ADDI";
                case 0b010: return "SLTI";
                case 0b011: return "SLTIU";
                case 0b100: return "XORI";
                case 0b110: return "ORI";
                case 0b111: return "ANDI";
                case 0b001: return "SLLI";
                case 0b101:
                    return (decoded.funct7 == 0x00) ? "SRLI" :
                           (decoded.funct7 == 0x20) ? "SRAI" : "UNKNOWN_IMM";
                default:
                    return "UNKNOWN_IMM";
            }

        case OPCODE_S: 
            switch (decoded.funct3) {
                case 0b000: return "SB"; 
                case 0b001: return "SH"; 
                case 0b010: return "SW"; 
                default:
                    return "UNKNOWN_STORE";
            }

        case OPCODE_SB: 
            switch (decoded.funct3) {
                case 0b000: return "BEQ";  
                case 0b001: return "BNE";  
                case 0b100: return "BLT";  
                case 0b101: return "BGE";  
                case 0b110: return "BLTU"; 
                case 0b111: return "BGEU"; 
                default:
                    return "UNKNOWN_BRANCH";
            }

        case OPCODE_U: // U-Type (LUI and AUIPC)
            return (decoded.rd != 0) ? "LUI" : "AUIPC";

        case OPCODE_UJ: // UJ-Type (Jump instructions)
            return "JAL";

        default:
            return "UNKNOWN";
    }
}


