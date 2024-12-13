#include "disassemble.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Shift instruction to start and apply mask 
// Mask is 1 shifted by length - 1:  0x00001 -> (shifted by fx length = 3) -> 0x01000 -> (-1) -> 0x00111
uint32_t extract_bits(uint32_t instruction, int start, int length) {
    return (instruction >> start) & ((1 << length) - 1);
}

int32_t decode_imm(uint32_t instruction, InstructionType type) {
    int32_t imm = 0;

    switch (type) {
        case I_TYPE:
            // Extract bits 20-31 (12-bit immediate, sign-extended)
            imm = extract_bits(instruction, 20, 12);
            if (imm & 0x800) { // Check sign bit (bit 11)
                imm |= 0xFFFFF000; // Sign-extend to 32 bits
            }
            break;

        case S_TYPE:
            // Combine bits [25-31] and [7-11] (12-bit immediate, sign-extended)
            imm = (extract_bits(instruction, 25, 7) << 5) | // Bits 25-31 shifted to positions 5-11
                  extract_bits(instruction, 7, 5);          // Bits 7-11
            if (imm & 0x800) { // Check sign bit (bit 11)
                imm |= 0xFFFFF000; // Sign-extend to 32 bits
            }
            break;

        case SB_TYPE:
            // Combine bits [31], [7], [25-30], and [8-11] (12-bit immediate, sign-extended)
            imm = (extract_bits(instruction, 31, 1) << 11) |  // Bit 31 (sign bit)
                  (extract_bits(instruction, 7, 1) << 10) |   // Bit 7
                  (extract_bits(instruction, 25, 6) << 4) |   // Bits 25-30
                  extract_bits(instruction, 8, 4);            // Bits 8-11
            imm <<= 1; // Multiply by 2 (SB immediate is always shifted left by 1)
            if (imm & 0x1000) { // Check sign bit (bit 12 after left shift)
                imm |= 0xFFFFE000; // Sign-extend to 32 bits
            }
            break;

        case U_TYPE:
            imm = extract_bits(instruction, 12, 20) << 12; // Shift upper 20 bits to their position
            break;

        case UJ_TYPE:
            // Combine bits [31], [21-30], [20], and [12-19] (20-bit immediate, sign-extended)
            imm = (extract_bits(instruction, 31, 1) << 19) |  // Bit 31 (sign bit)
                  (extract_bits(instruction, 21, 10) << 1) |  // Bits 21-30
                  (extract_bits(instruction, 20, 1) << 11) |  // Bit 20
                  extract_bits(instruction, 12, 8);           // Bits 12-19
            imm <<= 1; // Multiply by 2 (UJ immediate is always shifted left by 1)
            if (imm & 0x100000) { // Check sign bit (bit 20 after left shift)
                imm |= 0xFFE00000; // Sign-extend to 32 bits
            }
            break;

        default:
            imm = 0; // Invalid type
            break;
    }

    return imm;
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
                case 0x0: // 0b000
                    return (decoded.funct7 == 0x00) ? "ADD" :
                           (decoded.funct7 == 0x20) ? "SUB" : "UNKNOWN_R";
                case 0x1: // 0b001
                    return "SLL"; 
                case 0x2: // 0b010
                    return "SLT"; 
                case 0x3: // 0b011
                    return "SLTU"; 
                case 0x4: // 0b100
                    return "XOR";
                case 0x5: // 0b101
                    return (decoded.funct7 == 0x00) ? "SRL" :
                           (decoded.funct7 == 0x20) ? "SRA" : "UNKNOWN_R";
                case 0x6: // 0b110
                    return "OR";
                case 0x7: // 0b111
                    return "AND";
                default:
                    return "UNKNOWN_R";
            }

        case OPCODE_I_LOAD: 
            switch (decoded.funct3) {
                case 0x0: // 0b000
                    return "LB";  
                case 0x1: // 0b001
                    return "LH";  
                case 0x2: // 0b010
                    return "LW";  
                case 0x4: // 0b100
                    return "LBU"; 
                case 0x5: // 0b101
                    return "LHU"; 
                default:
                    return "UNKNOWN_LOAD";
            }

        case OPCODE_I_IMM: 
            switch (decoded.funct3) {
                case 0x0: // 0b000
                    return "ADDI";
                case 0x2: // 0b010
                    return "SLTI";
                case 0x3: // 0b011
                    return "SLTIU";
                case 0x4: // 0b100
                    return "XORI";
                case 0x6: // 0b110
                    return "ORI";
                case 0x7: // 0b111
                    return "ANDI";
                case 0x1: // 0b001
                    return "SLLI";
                case 0x5: // 0b101
                    return (decoded.funct7 == 0x00) ? "SRLI" :
                           (decoded.funct7 == 0x20) ? "SRAI" : "UNKNOWN_IMM";
                default:
                    return "UNKNOWN_IMM";
            }

        case OPCODE_S: 
            switch (decoded.funct3) {
                case 0x0: // 0b000
                    return "SB"; 
                case 0x1: // 0b001
                    return "SH"; 
                case 0x2: // 0b010
                    return "SW"; 
                default:
                    return "UNKNOWN_STORE";
            }

        case OPCODE_SB: 
            switch (decoded.funct3) {
                case 0x0: // 0b000
                    return "BEQ";  
                case 0x1: // 0b001
                    return "BNE";  
                case 0x4: // 0b100
                    return "BLT";  
                case 0x5: // 0b101
                    return "BGE";  
                case 0x6: // 0b110
                    return "BLTU"; 
                case 0x7: // 0b111
                    return "BGEU"; 
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


// Helper function to map registers to symbolic names
const char* get_register_name(uint8_t reg) {
    static const char* register_names[] = {
        "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
        "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
        "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
        "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
    };
    if (reg < 32) {
        return register_names[reg];
    }
    return "unknown";
}

// Main disassemble function
void disassemble(uint32_t addr, uint32_t instruction, char* result, size_t buf_size, struct symbols* symbols) {
    DecodedInstruction decoded = decode_instruction(instruction);
    const char* mnemonic = get_mnemonic(decoded);

    // Handle disassembly for different instruction types
    switch (decoded.type) {
        case R_TYPE:
            snprintf(result, buf_size, "%08x: %08x    %-8s %s, %s, %s",
                     addr, instruction, mnemonic,
                     get_register_name(decoded.rd),
                     get_register_name(decoded.rs1),
                     get_register_name(decoded.rs2));
            break;

        case I_TYPE:
            snprintf(result, buf_size, "%08x: %08x    %-8s %s, %s, %d",
                     addr, instruction, mnemonic,
                     get_register_name(decoded.rd),
                     get_register_name(decoded.rs1),
                     decoded.imm);
            break;

        case S_TYPE:
            snprintf(result, buf_size, "%08x: %08x    %-8s %s, %d(%s)",
                     addr, instruction, mnemonic,
                     get_register_name(decoded.rs2),
                     decoded.imm,
                     get_register_name(decoded.rs1));
            break;

        case SB_TYPE: {
            // Calculate target address for branches
            uint32_t target_addr = addr + decoded.imm;
            snprintf(result, buf_size, "%08x: %08x    %-8s %s, %s, %08x",
                     addr, instruction, mnemonic,
                     get_register_name(decoded.rs1),
                     get_register_name(decoded.rs2),
                     target_addr);
            break;
        }

        case U_TYPE:
            snprintf(result, buf_size, "%08x: %08x    %-8s %s, %d",
                     addr, instruction, mnemonic,
                     get_register_name(decoded.rd),
                     decoded.imm);
            break;

        case UJ_TYPE: {
            // Calculate target address for jumps
            uint32_t target_addr = addr + decoded.imm;
            snprintf(result, buf_size, "%08x: %08x    %-8s %s, %08x",
                     addr, instruction, mnemonic,
                     get_register_name(decoded.rd),
                     target_addr);
            break;
        }

        default:
            snprintf(result, buf_size, "%08x: %08x    UNKNOWN", addr, instruction);
            break;
    }
}



