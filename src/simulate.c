#include "./memory.h"
#include "./simulate.h"
#include "./disassemble.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

// Helper macros for instruction decoding
#define OPCODE(instr)    ((instr) & 0x7F)
#define FUNCT3(instr)    (((instr) >> 12) & 0x07)
#define FUNCT7(instr)    (((instr) >> 25) & 0x7F)
#define RD(instr)        (((instr) >> 7) & 0x1F)
#define RS1(instr)       (((instr) >> 15) & 0x1F)
#define RS2(instr)       (((instr) >> 20) & 0x1F)
#define IMM_I(instr)     ((int32_t)(instr) >> 20)
#define IMM_S(instr)     (((int32_t)((instr) & 0xFE000000) >> 20) | (((instr) >> 7) & 0x1F))
#define IMM_B(instr)     ((((instr) & 0x80000000) >> 19) | (((instr) & 0x7E000000) >> 20) | \
                          (((instr) & 0x00000F00) >> 7) | (((instr) & 0x00000080) << 4))
#define IMM_U(instr)     ((instr) & 0xFFFFF000)
#define IMM_J(instr)     ((((instr) & 0x80000000) >> 11) | ((instr) & 0xFF000) | \
                          (((instr) & 0x100000) >> 9) | (((instr) & 0x7FE00000) >> 20))

struct Stat simulate(struct memory *mem, int start_addr, FILE *log_file, struct symbols *symbols) {
    struct Stat stat = {0}; // Initialize statistics
    int pc = start_addr;    // Program counter
    int32_t registers[32] = {0}; // Simulated register file
    registers[0] = 0; // x0 is always 0 in RISC-V

    while (1) {
        // Validate PC
        if (pc < 0 || pc % 4 != 0) {
            fprintf(stderr, "Invalid PC: 0x%08x\n", pc);
            break;
        }

        // Fetch instruction
        uint32_t instruction = memory_rd_w(mem, pc);
        pc += 4; // Increment PC to next instruction
        if (instruction == 0x00000000) { // NOP
            // pc += 4;
            stat.insns++;
            continue;
        }

        // Decode instruction fields
        uint32_t opcode = OPCODE(instruction);
        uint32_t funct3 = FUNCT3(instruction);
        uint32_t funct7 = FUNCT7(instruction);
        uint32_t rd = RD(instruction);
        uint32_t rs1 = RS1(instruction);
        uint32_t rs2 = RS2(instruction);
        int32_t imm_i = IMM_I(instruction);
        int32_t imm_s = IMM_S(instruction);
        int32_t imm_b = IMM_B(instruction);
        int32_t imm_u = IMM_U(instruction);
        int32_t imm_j = IMM_J(instruction);

        // Disassemble and log
        char disassembly[128];
        disassemble(pc, instruction, disassembly, sizeof(disassembly), symbols);
        if (log_file) {
            fprintf(log_file, "PC: 0x%08x, Instruction: 0x%08x, %s\n", pc, instruction, disassembly);
        }

        // Execute instruction
        switch (opcode) {
            case 0x33: // R-Type instructions
                switch (funct3) {
                    case 0x0: // ADD or SUB
                        if (funct7 == 0x00) {
                            registers[rd] = registers[rs1] + registers[rs2]; // ADD
                        } else if (funct7 == 0x20) {
                            registers[rd] = registers[rs1] - registers[rs2]; // SUB
                        }
                        break;
                    case 0x1: registers[rd] = registers[rs1] << (registers[rs2] & 0x1F); break; // SLL
                    case 0x2: registers[rd] = (int32_t)registers[rs1] < (int32_t)registers[rs2]; break; // SLT
                    case 0x3: registers[rd] = registers[rs1] < registers[rs2]; break; // SLTU
                    case 0x4: registers[rd] = registers[rs1] ^ registers[rs2]; break; // XOR
                    case 0x5:
                        if (funct7 == 0x00) {
                            registers[rd] = registers[rs1] >> (registers[rs2] & 0x1F); // SRL
                        } else if (funct7 == 0x20) {
                            registers[rd] = (int32_t)registers[rs1] >> (registers[rs2] & 0x1F); // SRA
                        }
                        break;
                    case 0x6: registers[rd] = registers[rs1] | registers[rs2]; break; // OR
                    case 0x7: registers[rd] = registers[rs1] & registers[rs2]; break; // AND
                    default:
                        fprintf(stderr, "Unknown R-Type funct3: 0x%x\n", funct3);
                        break;
                }
                break;

            case 0x03: // I-Type Load instructions
                switch (funct3) {
                    case 0x0: registers[rd] = (int8_t)memory_rd_b(mem, registers[rs1] + imm_i); break; // LB
                    case 0x1: registers[rd] = (int16_t)memory_rd_h(mem, registers[rs1] + imm_i); break; // LH
                    case 0x2: registers[rd] = memory_rd_w(mem, registers[rs1] + imm_i); break; // LW
                    case 0x4: registers[rd] = memory_rd_b(mem, registers[rs1] + imm_i); break; // LBU
                    case 0x5: registers[rd] = memory_rd_h(mem, registers[rs1] + imm_i); break; // LHU
                    default:
                        fprintf(stderr, "Unknown I-Type funct3: 0x%x\n", funct3);
                        break;
                }
                break;

            case 0x13: // I-Type Arithmetic
                switch (funct3) {
                    case 0x0: registers[rd] = registers[rs1] + imm_i; break; // ADDI
                    case 0x2: registers[rd] = (int32_t)registers[rs1] < imm_i; break; // SLTI
                    case 0x3: registers[rd] = registers[rs1] < (uint32_t)imm_i; break; // SLTIU
                    case 0x4: registers[rd] = registers[rs1] ^ imm_i; break; // XORI
                    case 0x6: registers[rd] = registers[rs1] | imm_i; break; // ORI
                    case 0x7: registers[rd] = registers[rs1] & imm_i; break; // ANDI
                    default:
                        fprintf(stderr, "Unknown I-Type funct3: 0x%x\n", funct3);
                        break;
                }
                break;
            
            case 0x23: // S-Type (Store instructions)
                switch (funct3) {
                    case 0x0: // SB
                        memory_wr_b(mem, registers[rs1] + imm_s, registers[rs2] & 0xFF);
                        break;
                    case 0x1: // SH
                        memory_wr_h(mem, registers[rs1] + imm_s, registers[rs2] & 0xFFFF);
                        break;
                    case 0x2: // SW
                        memory_wr_w(mem, registers[rs1] + imm_s, registers[rs2]);
                        break;
                    default:
                        fprintf(stderr, "Unknown S-Type funct3: 0x%x\n", funct3);
                        break;
                }
                break;

            case 0x63: // SB-Type Branches
                switch (funct3) {
                    case 0x0: // BEQ
                        if (registers[rs1] == registers[rs2]) {
                            int target_addr = pc + imm_b;
                            if (target_addr < 0 || target_addr >= 0x80000000) {
                                fprintf(stderr, "Invalid branch target: 0x%08x\n", target_addr);
                                return stat;
                            }
                            pc = target_addr;
                            continue; // Skip PC increment
                        }
                        break;
                    case 0x1: // BNE
                        if (registers[rs1] != registers[rs2]) {
                            int target_addr = pc + imm_b;
                            if (target_addr < 0 || target_addr >= 0x80000000) {
                                fprintf(stderr, "Invalid branch target: 0x%08x\n", target_addr);
                                return stat;
                            }
                            pc = target_addr;
                            continue; // Skip PC increment
                        }
                        break;
                    case 0x4: // BLT
                        if ((int32_t)registers[rs1] < (int32_t)registers[rs2]) {
                            int target_addr = pc + imm_b;
                            if (target_addr < 0 || target_addr >= 0x80000000) {
                                fprintf(stderr, "Invalid branch target: 0x%08x\n", target_addr);
                                return stat;
                            }
                            pc = target_addr;
                            continue; // Skip PC increment
                        }
                        break;
                    case 0x5: // BGE
                        if ((int32_t)registers[rs1] >= (int32_t)registers[rs2]) {
                            int target_addr = pc + imm_b;
                            if (target_addr < 0 || target_addr >= 0x80000000) {
                                fprintf(stderr, "Invalid branch target: 0x%08x\n", target_addr);
                                return stat;
                            }
                            pc = target_addr;
                            continue; // Skip PC increment
                        }
                        break;
                    case 0x6: // BLTU
                        if (registers[rs1] < registers[rs2]) {
                            int target_addr = pc + imm_b;
                            if (target_addr < 0 || target_addr >= 0x80000000) {
                                fprintf(stderr, "Invalid branch target: 0x%08x\n", target_addr);
                                return stat;
                            }
                            pc = target_addr;
                            continue; // Skip PC increment
                        }
                        break;
                    case 0x7: // BGEU
                        if (registers[rs1] >= registers[rs2]) {
                            int target_addr = pc + imm_b;
                            if (target_addr < 0 || target_addr >= 0x80000000) {
                                fprintf(stderr, "Invalid branch target: 0x%08x\n", target_addr);
                                return stat;
                            }
                            pc = target_addr;
                            continue; // Skip PC increment
                        }
                        break;
                    default:
                        fprintf(stderr, "Unknown SB-Type funct3: 0x%x\n", funct3);
                        break;
                }
                break;
            
            case 0x67: // I-Type JALR
                {
                    int next_pc = pc + 4; // Save the address of the next instruction
                    pc = (registers[rs1] + imm_i) & ~1; // Jump to target address (aligned to 2 bytes)
                    registers[rd] = next_pc; // Store return address in rd
                    continue; // Skip PC increment
                }

            case 0x37: registers[rd] = imm_u; break; // U-Type LUI
            case 0x17: registers[rd] = pc + (imm_u & 0xFFFFF000); fprintf(stderr, "AUIPC: PC=0x%08x, imm_u=0x%08x, result=0x%08x\n", pc, imm_u, registers[rd]); break; // Add upper immediate (shifted) to PC
            case 0x6F: // UJ-Type JAL
                registers[rd] = pc + 4; 
                pc += imm_j - 4; 
                continue; 

            case 0x73: 
                switch (registers[17]) { 
                    case 1: registers[10] = getchar(); break; 
                    case 2: putchar(registers[10]); break; 
                    case 3: return stat; 
                }
                break;

            default:
                fprintf(stderr, "Unknown instruction: 0x%08x at PC: 0x%08x\n", instruction, pc);
                break;
        }

        // Ensure x0 remains 0
        registers[0] = 0;
        if (pc < 0 || pc >= 0x80000000) {
            fprintf(stderr, "Invalid PC after jump/branch: 0x%08x\n", pc);
            return stat;
        }

        // Increment PC and update stats
        // pc += 4;
        stat.insns++;
        fprintf(stderr, "Decoded instruction: opcode=0x%x, funct3=0x%x, funct7=0x%x, imm=0x%x\n", OPCODE(instruction), FUNCT3(instruction), FUNCT7(instruction), IMM_I(instruction));
    }

    return stat;
}
