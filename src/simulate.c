#include "simulate.h"
#include "memory.h"
#include <stdint.h>
#include <stdio.h>

// Helper macros for extracting instruction fields
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

// Number of general-purpose registers
#define NUM_REGS 32

struct Stat simulate(struct memory *mem, int start_addr, FILE *log_file, struct symbols* symbols) {
    struct Stat stats = {0}; // Initialize instruction counter
    uint32_t pc = start_addr; // Program Counter
    uint32_t reg[NUM_REGS] = {0}; // General-purpose registers
    reg[0] = 0;
    int running = 1; // Simulation control flag

    while (running) {
        // Fetch
        uint32_t instruction = memory_rd_w(mem, pc);
        pc += 4; // Increment PC to next instruction
        // Handle NOP (0x00000000)
        if (instruction == 0x00000000) {
            pc += 4; // Move to the next instruction
            continue;
        }
        // Decode
        uint32_t opcode = OPCODE(instruction);

        switch (opcode) {
            case 0x33: { // R-Type instructions
                uint32_t rd = RD(instruction);
                uint32_t rs1 = RS1(instruction);
                uint32_t rs2 = RS2(instruction);
                uint32_t funct3 = FUNCT3(instruction);
                uint32_t funct7 = FUNCT7(instruction);

                if (funct7 == 0x00) {
                    if (funct3 == 0x0) reg[rd] = reg[rs1] + reg[rs2]; // ADD
                    else if (funct3 == 0x1) reg[rd] = reg[rs1] << (reg[rs2] & 0x1F); // SLL
                    else if (funct3 == 0x2) reg[rd] = (int32_t)reg[rs1] < (int32_t)reg[rs2]; // SLT
                    else if (funct3 == 0x3) reg[rd] = reg[rs1] < reg[rs2]; // SLTU
                    else if (funct3 == 0x4) reg[rd] = reg[rs1] ^ reg[rs2]; // XOR
                    else if (funct3 == 0x5) reg[rd] = reg[rs1] >> (reg[rs2] & 0x1F); // SRL
                    else if (funct3 == 0x6) reg[rd] = reg[rs1] | reg[rs2]; // OR
                    else if (funct3 == 0x7) reg[rd] = reg[rs1] & reg[rs2]; // AND
                } else if (funct7 == 0x20) {
                    if (funct3 == 0x0) reg[rd] = reg[rs1] - reg[rs2]; // SUB
                    else if (funct3 == 0x5) reg[rd] = (int32_t)reg[rs1] >> (reg[rs2] & 0x1F); // SRA
                } else if (funct7 == 0x01) {
                    if (funct3 == 0x0) reg[rd] = reg[rs1] * reg[rs2]; // MUL
                    else if (funct3 == 0x4) reg[rd] = (int32_t)reg[rs1] / (int32_t)reg[rs2]; // DIV
                    else if (funct3 == 0x5) reg[rd] = reg[rs1] / reg[rs2]; // DIVU
                    else if (funct3 == 0x6) reg[rd] = reg[rs1] % reg[rs2]; // REM
                    else if (funct3 == 0x7) reg[rd] = reg[rs1] % reg[rs2]; // REMU
                }
                break;
            }
            case 0x03: { // I-Type (Load)
                uint32_t rd = RD(instruction);
                uint32_t rs1 = RS1(instruction);
                int32_t imm = IMM_I(instruction);
                uint32_t addr = reg[rs1] + imm;

                uint32_t funct3 = FUNCT3(instruction);
                if (funct3 == 0x0) reg[rd] = (int8_t)memory_rd_b(mem, addr); // LB
                else if (funct3 == 0x1) reg[rd] = (int16_t)memory_rd_h(mem, addr); // LH
                else if (funct3 == 0x2) reg[rd] = memory_rd_w(mem, addr); // LW
                else if (funct3 == 0x4) reg[rd] = memory_rd_b(mem, addr); // LBU
                else if (funct3 == 0x5) reg[rd] = memory_rd_h(mem, addr); // LHU
                break;
            }
            case 0x13: { // I-Type (Arithmetic Immediate)
                uint32_t rd = RD(instruction);
                uint32_t rs1 = RS1(instruction);
                int32_t imm = IMM_I(instruction);

                uint32_t funct3 = FUNCT3(instruction);
                if (funct3 == 0x0) reg[rd] = reg[rs1] + imm; // ADDI
                else if (funct3 == 0x2) reg[rd] = (int32_t)reg[rs1] < imm; // SLTI
                else if (funct3 == 0x3) reg[rd] = reg[rs1] < (uint32_t)imm; // SLTIU
                else if (funct3 == 0x4) reg[rd] = reg[rs1] ^ imm; // XORI
                else if (funct3 == 0x6) reg[rd] = reg[rs1] | imm; // ORI
                else if (funct3 == 0x7) reg[rd] = reg[rs1] & imm; // ANDI
                else if (funct3 == 0x5) { // SRLI or SRAI
                    uint32_t a = instruction >> 20 & 0x1F; // Extract shift amount (5 bits)
                    uint32_t funct7 = FUNCT7(instruction);
                    if (funct7 == 0x00) {
                        reg[rd] = reg[rs1] >> a; // SRLI (Logical Shift)
                    } else if (funct7 == 0x20) {
                        reg[rd] = (int32_t)reg[rs1] >> a; // SRAI (Arithmetic Shift)
                    }
                }
                break;
            }
            case 0x67: { // I-Type (JALR)
                uint32_t rd = RD(instruction);
                uint32_t rs1 = RS1(instruction);
                int32_t imm = IMM_I(instruction);

                uint32_t next_pc = pc;
                pc = (reg[rs1] + imm) & ~1;
                reg[rd] = pc+4;
                // rd == 0 means no return address is stored
                if (rd != 0) reg[rd] = next_pc; // Store the return address
                break;
            }
            case 0x37: { // U-Type (LUI)
                uint32_t rd = RD(instruction);
                uint32_t imm = IMM_U(instruction); 
                reg[rd] = imm; 
                break;
            }
            case 0x23: { // S-Type (Stores)
                uint32_t rs1 = RS1(instruction);
                uint32_t rs2 = RS2(instruction);
                int32_t imm = IMM_S(instruction); // Extract signed immediate
                uint32_t addr = reg[rs1] + imm;   // Compute memory address

                uint32_t funct3 = FUNCT3(instruction);
                if (funct3 == 0x0) { // SB
                    memory_wr_b(mem, addr, reg[rs2] & 0xFF); // Write least significant byte
                } else if (funct3 == 0x1) { // SH
                    memory_wr_h(mem, addr, reg[rs2] & 0xFFFF); // Write least significant 2 bytes
                } else if (funct3 == 0x2) { // SW
                    memory_wr_w(mem, addr, reg[rs2]); // Write all 4 bytes
                }
                break;
            }
            case 0x63: { // SB-Type (Branches)
                int32_t imm = IMM_B(instruction);
                uint32_t rs1 = RS1(instruction);
                uint32_t rs2 = RS2(instruction);

                uint32_t funct3 = FUNCT3(instruction);
                if (funct3 == 0x0 && reg[rs1] == reg[rs2]) pc += imm - 4; // BEQ
                else if (funct3 == 0x1 && reg[rs1] != reg[rs2]) pc += imm - 4; // BNE
                else if (funct3 == 0x4 && (int32_t)reg[rs1] < (int32_t)reg[rs2]) pc += imm - 4; // BLT
                else if (funct3 == 0x5 && (int32_t)reg[rs1] >= (int32_t)reg[rs2]) pc += imm - 4; // BGE
                else if (funct3 == 0x6 && reg[rs1] < reg[rs2]) pc += imm - 4; // BLTU
                else if (funct3 == 0x7 && reg[rs1] >= reg[rs2]) pc += imm - 4; // BGEU
                break;
            }
            case 0x6F: { // UJ-Type (JAL)
                uint32_t rd = RD(instruction);
                int32_t imm = IMM_J(instruction);

                reg[rd] = pc; // Store return address
                pc += imm - 4; // Jump
                break;
            }
            case 0x17: {
                uint32_t rd = RD(instruction);
                uint32_t imm = IMM_U(instruction);
                reg[rd] = pc + imm;
                break;
            }
            case 0x73: { // ECALL
                switch (reg[17]) { // System call type in A7 (x17)
                    case 1: //getchar
                        reg[10] = getchar();
                        break;
                    case 2: //putchar
                        putchar(reg[10]);
                        break;
                    case 3: //exit
                    case 93:
                        running = 0; // Stop simulation
                        break;
                }
                break;
            }
            default: {
                fprintf(stderr, "Unknown instruction: 0x%08X\n", instruction);
                running = 0; // Stop simulation
                break;
            }
        }

        // Log execution
        if (log_file) {
            fprintf(log_file, "PC: 0x%08X, Instruction: 0x%08X\n", pc, instruction);
        }

        stats.insns++; // Increment instruction counter
    }

    return stats; // Return statistics
}
