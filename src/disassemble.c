#include "disassemble.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

void decode_R_type(uint32_t instruction, char* result, size_t buf_size);
void decode_I_type(uint32_t instruction, char* result, size_t buf_size, uint32_t addr, struct symbols* symbols);
void decode_S_type(uint32_t instruction, char* result, size_t buf_size);
void decode_B_type(uint32_t instruction, char* result, size_t buf_size, uint32_t addr, struct symbols* symbols);
void decode_U_type(uint32_t instruction, char* result, size_t buf_size);
void decode_J_type(uint32_t instruction, char* result, size_t buf_size, uint32_t addr, struct symbols* symbols);

void disassemble(uint32_t addr, uint32_t instruction, char* result, size_t buf_size, struct symbols* symbols) {
    
    if (instruction == 0x00000000) {
        snprintf(result, buf_size, "nop");
        return;
    }
    uint32_t opcode = instruction & 0x7F;
    switch (opcode) {
        case 0x33:  // R-Type
            decode_R_type(instruction, result, buf_size);
            break;
        case 0x03:  // I-Type (loads)
        case 0x13:  // I-Type (arithmetic immediate)
        case 0x67:  // I-Type (jalr)
            decode_I_type(instruction, result, buf_size, addr, symbols);
            break;
        case 0x23:  // S-Type
            decode_S_type(instruction, result, buf_size);
            break;
        case 0x63:  // SB-Type
            decode_B_type(instruction, result, buf_size, addr, symbols);
            break;
        case 0x37:  // U-Type
        case 0x17:
            decode_U_type(instruction, result, buf_size);
            break;
        case 0x6F:  // UJ-Type
            decode_J_type(instruction, result, buf_size, addr, symbols);
            break;
        default:
            snprintf(result, buf_size, "Unknown instruction (0x%08x)", instruction);
    }
}

int32_t sign_extend(int32_t value, int bits) {
    int shift = 32 - bits;
    return (value << shift) >> shift;
}



//R-Type instructions
void decode_R_type(uint32_t instruction, char* result, size_t buf_size){
    uint32_t rd = (instruction >> 7) & 0x1F;
    uint32_t funct3 = (instruction >> 12) & 0x07;
    uint32_t rs1 = (instruction >> 15) & 0x1F;
    uint32_t rs2 = (instruction >> 20) & 0x1F;
    uint32_t funct7 =  (instruction >> 25) & 0x7F;

    //RV32I instructions
    if (funct7 == 0x00) {
        if (funct3 == 0x0) snprintf(result, buf_size, "add x%d, x%d, x%d", rd, rs1, rs2);
        else if (funct3 == 0x1) snprintf(result, buf_size, "sll x%d, x%d, x%d", rd, rs1, rs2);
        else if (funct3 == 0x2) snprintf(result, buf_size, "slt x%d, x%d, x%d", rd, rs1, rs2);
        else if (funct3 == 0x3) snprintf(result, buf_size, "sltu x%d, x%d, x%d", rd, rs1, rs2);
        else if (funct3 == 0x4) snprintf(result, buf_size, "xor x%d, x%d, x%d", rd, rs1, rs2);
        else if (funct3 == 0x5) snprintf(result, buf_size, "srl x%d, x%d, x%d", rd, rs1, rs2);
        else if (funct3 == 0x6) snprintf(result, buf_size, "or x%d, x%d, x%d", rd, rs1, rs2);
        else if (funct3 == 0x7) snprintf(result, buf_size, "and x%d, x%d, x%d", rd, rs1, rs2);
        else snprintf(result, buf_size, "Unknown R-Type");
    } else if (funct7 == 0x20) {
        if (funct3 == 0x0) snprintf(result, buf_size, "sub x%d, x%d, x%d", rd, rs1, rs2);
        else if (funct3 == 0x5) snprintf(result, buf_size, "sra x%d, x%d, x%d", rd, rs1, rs2);
        else snprintf(result, buf_size, "Unknown R-Type");
    }
    //RV32M instructions
    else if (funct7 == 0x01) {
        if (funct3 == 0x0) snprintf(result, buf_size, "mul x%d, x%d, x%d", rd, rs1, rs2);
        else if (funct3 == 0x1) snprintf(result, buf_size, "mulh x%d, x%d, x%d", rd, rs1, rs2);
        else if (funct3 == 0x2) snprintf(result, buf_size, "mulhsu x%d, x%d, x%d", rd, rs1, rs2);
        else if (funct3 == 0x3) snprintf(result, buf_size, "mulhu x%d, x%d, x%d", rd, rs1, rs2);
        else if (funct3 == 0x4) snprintf(result, buf_size, "div x%d, x%d, x%d", rd, rs1, rs2);
        else if (funct3 == 0x5) snprintf(result, buf_size, "divu x%d, x%d, x%d", rd, rs1, rs2);
        else if (funct3 == 0x6) snprintf(result, buf_size, "rem x%d, x%d, x%d", rd, rs1, rs2);
        else if (funct3 == 0x7) snprintf(result, buf_size, "remu x%d, x%d, x%d", rd, rs1, rs2);
        else snprintf(result, buf_size, "Unknown R-Type");
    } else {
        snprintf(result, buf_size, "Unknown R-Type");
    }
}

//I-Type instructions
void decode_I_type(uint32_t instruction, char* result, size_t buf_size, uint32_t addr, struct symbols* symbols) {
    uint32_t rd = (instruction >> 7) & 0x1F;
    uint32_t funct3 = (instruction >> 12) & 0x07;
    uint32_t rs1 = (instruction >> 15) & 0x1F;
    int32_t imm = sign_extend((instruction >> 20), 12); 

    if ((instruction & 0x7F) == 0x03) {  // Load instructions
        switch (funct3) {
            case 0x0: snprintf(result, buf_size, "lb x%d, %d(x%d)", rd, imm, rs1); break;
            case 0x1: snprintf(result, buf_size, "lh x%d, %d(x%d)", rd, imm, rs1); break;
            case 0x2: snprintf(result, buf_size, "lw x%d, %d(x%d)", rd, imm, rs1); break;
            case 0x4: snprintf(result, buf_size, "lbu x%d, %d(x%d)", rd, imm, rs1); break;
            case 0x5: snprintf(result, buf_size, "lhu x%d, %d(x%d)", rd, imm, rs1); break;
            default: snprintf(result, buf_size, "Unknown I-Type load");
        }
    } else if ((instruction & 0x7F) == 0x13) {  // Arithmetic Immediate
        switch (funct3) {
            case 0x0: snprintf(result, buf_size, "addi x%d, x%d, %d", rd, rs1, imm); break;
            case 0x2: snprintf(result, buf_size, "slti x%d, x%d, %d", rd, rs1, imm); break;
            case 0x3: snprintf(result, buf_size, "sltiu x%d, x%d, %d", rd, rs1, imm); break;
            case 0x4: snprintf(result, buf_size, "xori x%d, x%d, %d", rd, rs1, imm); break;
            case 0x6: snprintf(result, buf_size, "ori x%d, x%d, %d", rd, rs1, imm); break;
            case 0x7: snprintf(result, buf_size, "andi x%d, x%d, %d", rd, rs1, imm); break;
            default: snprintf(result, buf_size, "Unknown I-Type");
        } 
    } else if ((instruction & 0x7F) == 0x67) {  // jalr
    snprintf(result, buf_size, "jalr x%d, %d(x%d)", rd, imm, rs1);
    } else if ((instruction & 0x7F) == 0x73) { // ECALL
        if (imm == 0) {
            snprintf(result, buf_size, "ecall");
        } else {
            snprintf(result, buf_size, "Unknown I-Type");
        }
    }
}

//S-Type instructions
void decode_S_type(uint32_t instruction, char* result, size_t buf_size) {
    uint32_t funct3 = (instruction >> 12) & 0x07;
    uint32_t rs1 = (instruction >> 15) & 0x1F;
    uint32_t rs2 = (instruction >> 20) & 0x1F;
    int32_t imm = sign_extend(((instruction >> 7) & 0x1F) | ((instruction >> 25) << 5), 12);; //Combine imm[4:0] and imm[11:5]

    if (funct3 == 0x0) {
        snprintf(result, buf_size, "sb x%d, %d(x%d)", rs2, imm, rs1);
    } else if (funct3 == 0x1) {
        snprintf(result, buf_size, "sh x%d, %d(x%d)", rs2, imm, rs1);
    } else if (funct3 == 0x2) {
        snprintf(result, buf_size, "sw x%d, %d(x%d)", rs2, imm, rs1);
    } else {
        snprintf(result, buf_size, "Unknown S-Type");
    }   
}

//SB-Type instructions
void decode_B_type(uint32_t instruction, char* result, size_t buf_size, uint32_t addr, struct symbols* symbols) {
    uint32_t funct3 = (instruction >> 12) & 0x07;
    uint32_t rs1 = (instruction >> 15) & 0x1F;
    uint32_t rs2 = (instruction >> 20) & 0x1F;
    int32_t imm = sign_extend(((instruction & 0x80) << 4) | ((instruction >> 7) & 0x1E) |
                          ((instruction >> 20) & 0x7E0) | ((instruction >> 19) & 0x1000), 13);
    imm <<= 1;

    switch (funct3) {
        case 0x0: snprintf(result, buf_size, "beq x%d, x%d, 0x%x", rs1, rs2, addr + imm); break;
        case 0x1: snprintf(result, buf_size, "bne x%d, x%d, 0x%x", rs1, rs2, addr + imm); break;
        case 0x4: snprintf(result, buf_size, "blt x%d, x%d, 0x%x", rs1, rs2, addr + imm); break;
        case 0x5: snprintf(result, buf_size, "bge x%d, x%d, 0x%x", rs1, rs2, addr + imm); break;
        case 0x6: snprintf(result, buf_size, "bltu x%d, x%d, 0x%x", rs1, rs2, addr + imm); break;
        case 0x7: snprintf(result, buf_size, "bgeu x%d, x%d, 0x%x", rs1, rs2, addr + imm); break;
        default: snprintf(result, buf_size, "Unknown SB-Type");
    }
}

//U-Type instructions
void decode_U_type(uint32_t instruction, char* result, size_t buf_size) {
    uint32_t rd = (instruction >> 7) & 0x1F;
    uint32_t imm = instruction & 0xFFFFF000;
    snprintf(result, buf_size, "lui x%d, 0x%x", rd, imm);
}

//UJ-Type instructions
void decode_J_type(uint32_t instruction, char* result, size_t buf_size, uint32_t addr, struct symbols* symbols) {
    uint32_t rd = (instruction >> 7) & 0x1F;
    int32_t imm = ((instruction & 0x80000000) ? 0xFFF00000 : 0) | // imm[20] (sign-extend)
                  ((instruction >> 21) & 0x3FF) << 1 |           // imm[10:1]
                  ((instruction >> 20) & 0x1) << 11 |            // imm[11]
                  ((instruction >> 12) & 0xFF) << 12;            // imm[19:12]
    
    snprintf(result, buf_size, "jal x%d, 0x%x", rd, addr + imm);
}