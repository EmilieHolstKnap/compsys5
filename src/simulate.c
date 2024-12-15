


#include "simulate.h"
#include "disassemble.h"
#include "memory.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static inline int32_t sign_extend(uint32_t value, int bits) {
    int shift = 32 - bits; // Number of bits to shift
    return (int32_t)(value << shift) >> shift; // Shift left, then arithmetic right
}


// Helper macros for extracting instruction fields
#define OPCODE(instr)    ((instr) & 0x7F)
#define FUNCT3(instr)    (((instr) >> 12) & 0x07)
#define FUNCT7(instr)    (((instr) >> 25) & 0x7F)
#define RD(instr)        (((instr) >> 7) & 0x1F)
#define RS1(instr)       (((instr) >> 15) & 0x1F)
#define RS2(instr)       (((instr) >> 20) & 0x1F)
#define IMM_I(instr)     ((int32_t)(instr) >> 20)
#define IMM_S(instr)     (((int32_t)((instr) & 0xFE000000) >> 20) | (((instr) >> 7) & 0x1F))
#define IMM_B(instr) \
    (sign_extend((((instr) & 0x80000000) >> 19) | (((instr) & 0x7E000000) >> 20) | \
                  (((instr) & 0x00000F00) >> 7) | (((instr) & 0x00000080) << 4), 13))

#define IMM_U(instr)     ((instr) & 0xFFFFF000)
#define IMM_J(instr) \
    (((int32_t)(((instr) & 0x80000000) ? 0xFFF00000 : 0)) | /* Sign extend */ \
     (((instr) >> 12) & 0xFF) << 12 |                        /* Bits 19:12 */ \
     (((instr) >> 20) & 0x1) << 11 |                         /* Bit 11 */ \
     (((instr) >> 21) & 0x3FF) << 1)                         /* Bits 10:1 */

struct Stat simulate(struct memory *mem, int start_addr, FILE *log_file, struct symbols* symbols) {
    struct Stat stats = {0}; // Initialize instruction counter
    uint32_t pc = start_addr; // Program Counter
    uint32_t reg[32] = {0}; // General-purpose registers
    reg[0] = 0; // x0 is always zero
    int running = 1; // Simulation control flag
    char disassembly[100]; // Buffer for disassembled instructions
    uint32_t next_pc = 0;
    
    while (running) {
        // Fetch
        uint32_t instruction = memory_rd_w(mem, pc);
        uint32_t old_pc = pc;
        if (instruction == 0x00000000) { // NOP
            // Count consecutive NOPs
            int nop_count = 0;
            while (instruction == 0x00000000) {
                pc += 4;
                stats.insns++;
                nop_count++;
                instruction = memory_rd_w(mem, pc);
            }
            if (nop_count > 1000) { // Example threshold
                fprintf(log_file, "Too many NOPs executed. Terminating simulation.\n");
                running = 0; // Stop simulation
                break;
            }
            // Log the NOP sequence
            if (log_file) {
                fprintf(log_file, "PC: 0x%08x, NOP sequence of %d instructions\n", old_pc, nop_count);
            }

            continue; // Skip to the next non-NOP instruction
        }
        pc += 4; // Increment PC to next instruction

        // Decode and log
        disassemble(old_pc, instruction, disassembly, sizeof(disassembly), symbols);
        
        
        // Prepare log entry
        char log_entry[300];
        snprintf(log_entry, sizeof(log_entry), "%5ld %s %08x : %08x     %-20s",
                 stats.insns, pc == next_pc ? "    " : "=>", old_pc, instruction, disassembly);

        // Decode
        uint32_t opcode = OPCODE(instruction);
        uint32_t rd, rs1, rs2, imm;
        int branch_taken = 0; // Flag for conditional branch
        
        switch (opcode) {
            case 0x33: { // R-Type instructions
                rd = RD(instruction);
                rs1 = RS1(instruction);
                rs2 = RS2(instruction);
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
                
                snprintf(log_entry + strlen(log_entry), sizeof(log_entry) - strlen(log_entry),
                         " R[%d] <- 0x%x", rd, reg[rd]);
                break;
            }
            case 0x03: { // I-Type (Load)
                rd = RD(instruction);
                rs1 = RS1(instruction);
                imm = IMM_I(instruction);
                uint32_t addr = reg[rs1] + imm;

                uint32_t funct3 = FUNCT3(instruction);
                if (funct3 == 0x0) reg[rd] = (int8_t)memory_rd_b(mem, addr); // LB
                else if (funct3 == 0x1) reg[rd] = (int16_t)memory_rd_h(mem, addr); // LH
                else if (funct3 == 0x2) reg[rd] = memory_rd_w(mem, addr); // LW
                else if (funct3 == 0x4) reg[rd] = memory_rd_b(mem, addr); // LBU
                else if (funct3 == 0x5) reg[rd] = memory_rd_h(mem, addr); // LHU
                
                snprintf(log_entry + strlen(log_entry), sizeof(log_entry) - strlen(log_entry),
                         " R[%d] <- 0x%x", rd, reg[rd]);
                break;
            }
            case 0x13: { // I-Type (Arithmetic Immediate)
                rd = RD(instruction);
                rs1 = RS1(instruction);
                imm = IMM_I(instruction);

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
                snprintf(log_entry + strlen(log_entry), sizeof(log_entry) - strlen(log_entry),
                         " R[%d] <- 0x%x", rd, reg[rd]);

                break;
            }
            case 0x67: { // I-Type (JALR)
                rd = RD(instruction);
                rs1 = RS1(instruction);
                imm = IMM_I(instruction);

                uint32_t next_pc = pc;              // Store the current PC as the return address
                pc = (reg[rs1] + imm) & ~1;         // Compute the target address and mask the LSB
                if (rd != 0) {                      // Write the return address to rd only if rd != 0
                    reg[rd] = next_pc;
                }
                snprintf(log_entry + strlen(log_entry), sizeof(log_entry) - strlen(log_entry), 
                        " JALR: PC=0x%x, Target=0x%x, Rd=x%d, Value=0x%x", 
                        next_pc, pc, rd, reg[rd]);
                break;
            }
            case 0x37: { // U-Type (LUI)
                rd = RD(instruction);
                imm = IMM_U(instruction); 
                reg[rd] = imm; 
                snprintf(log_entry + strlen(log_entry), sizeof(log_entry) - strlen(log_entry)," R[%d] <- 0x%x", rd, reg[rd]);
                break;
            }
            case 0x23: { // S-Type (Stores)
                rs1 = RS1(instruction);
                rs2 = RS2(instruction);
                imm = IMM_S(instruction); // Extract signed immediate
                uint32_t addr = reg[rs1] + imm;   // Compute memory address

                uint32_t funct3 = FUNCT3(instruction);
                if (funct3 == 0x0) { // SB
                    memory_wr_b(mem, addr, reg[rs2] & 0xFF); // Write least significant byte
                } else if (funct3 == 0x1) { // SH
                    memory_wr_h(mem, addr, reg[rs2] & 0xFFFF); // Write least significant 2 bytes
                } else if (funct3 == 0x2) { // SW
                    memory_wr_w(mem, addr, reg[rs2]); // Write all 4 bytes
                }
                snprintf(log_entry + strlen(log_entry), sizeof(log_entry) - strlen(log_entry)," M[0x%x] <- 0x%x", addr, reg[rs2]);
                break;
            }
            case 0x63: { // SB-Type (Branches)
                imm = IMM_B(instruction);
                rs1 = RS1(instruction);
                rs2 = RS2(instruction);

                uint32_t funct3 = FUNCT3(instruction);

                switch (funct3) {
                        case 0x0: // BEQ 
                            branch_taken = (reg[rs1] == reg[rs2]);
                            break;
                        case 0x1: // BNE 
                            branch_taken = (reg[rs1] != reg[rs2]);
                            break;
                        case 0x4: // BLT 
                            branch_taken = ((int32_t)reg[rs1] < (int32_t)reg[rs2]);
                            break;
                        case 0x5: // BGE 
                            branch_taken = ((int32_t)reg[rs1] >= (int32_t)reg[rs2]);
                            break;
                        case 0x6: // BLTU 
                            branch_taken = (reg[rs1] < reg[rs2]);
                            break;
                        case 0x7: // BGEU 
                            branch_taken = (reg[rs1] >= reg[rs2]);
                            break;
                        default: // Unknown branch type
                            branch_taken = 0;
                            break;
                    }
                    if (branch_taken) {
                        pc = old_pc + imm;
                        snprintf(log_entry + strlen(log_entry), sizeof(log_entry) - strlen(log_entry), " {T}");
                    } else {
                        snprintf(log_entry + strlen(log_entry), sizeof(log_entry) - strlen(log_entry), " {F}");
                    }
                    break;
            }

            case 0x6F: { // UJ-Type (JAL)
                rd = RD(instruction);
                imm = IMM_J(instruction);

                // Store return address (address of the next instruction)
                if (rd != 0) { // Ensure x0 is not written
                    reg[rd] = pc + 4;
                }

                // Jump to target address
                pc += imm;

                snprintf(log_entry + strlen(log_entry), sizeof(log_entry) - strlen(log_entry),
                        " JAL: PC=0x%x, Target=0x%x, Rd=x%d, Value=0x%x",
                        old_pc, pc, rd, reg[rd]);
                break;
            }
            case 0x17: {
                rd = RD(instruction);
                imm = IMM_U(instruction);
                reg[rd] = pc + imm;
                break;
            }
            case 0x73: { // ECALL
                uint32_t syscall = reg[17]; // A7 register holds the syscall number
                uint32_t arg0 = reg[10];   // A0 register holds the first argument

                if (log_file) {
                    fprintf(log_file, "ECALL: A7=0x%x, A0=0x%x, PC=0x%x\n", syscall, arg0, pc);
                }

                switch (syscall) {
                    case 1: // getchar
                        reg[10] = getchar();
                        if (log_file) {
                            fprintf(log_file, "Syscall getchar() returned 0x%x\n", reg[10]);
                        }
                        break;

                    case 2: // putchar
                        putchar(arg0);
                        if (log_file) {
                            fprintf(log_file, "Syscall putchar('%c') executed\n", (char)arg0);
                        }
                        break;

                    case 3: // exit
                    case 93: // exit with success
                        running = 0; // Stop simulation
                        if (log_file) {
                            fprintf(log_file, "Exit ECALL %d received. Terminating simulation with A0=0x%x.\n", syscall, arg0);
                        }
                        break;

                    default: // Unknown ECALL
                        if (log_file) {
                            fprintf(log_file, "Unknown ECALL: A7=0x%x, A0=0x%x, PC=0x%x\n", syscall, arg0, pc);
                            fprintf(log_file, "Suggestion: Check the ECALL implementation or the program's syscall number.\n");
                        }
                        running = 0; // Stop simulation for unknown ECALL
                        break;
                }

                pc += 4; // Increment the program counter to avoid an infinite loop
                break;
            }

            default: {
                snprintf(log_entry + strlen(log_entry), sizeof(log_entry) - strlen(log_entry), " Unknown");
                running = 0; // Stop simulation
            }
        }

        // Log execution
        if (log_file) {
            fprintf(log_file, "%s\n", log_entry);
        }

        stats.insns++; // Increment instruction counter
    }

    return stats; // Return statistics
}

