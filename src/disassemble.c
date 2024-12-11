#include "disassemble.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef enum {
    R_TYPE = 0x33,
    I_TYPE = 0x13,
    S_TYPE = 0x23,
    SB_TYPE = 0x63,
    U_TYPE = 0x37,
    UJ_TYPE = 0x6F
} types;

void decode_R_type(uint32_t instruction, char* result, size_t buf_size);
void decode_I_type(uint32_t instruction, char* result, size_t buf_size, uint32_t addr, struct symbols* symbols);
void decode_S_type(uint32_t instruction, char* result, size_t buf_size);
void decode_B_type(uint32_t instruction, char* result, size_t buf_size, uint32_t addr, struct symbols* symbols);
void decode_U_type(uint32_t instruction, char* result, size_t buf_size);
void decode_J_type(uint32_t instruction, char* result, size_t buf_size, uint32_t addr, struct symbols* symbols);


void disassemble(uint32_t addr, uint32_t instruction, char* result, size_t buf_size, struct symbols* symbols){
    uint32_t opcode = instruction & 0x7F;
    switch (opcode) {
        case R_TYPE:
            decode_R_type(instruction, result, buf_size);
            break;
        case I_TYPE:
            decode_I_type(instruction, result, buf_size, addr, symbols);
            break;
        case S_TYPE:
            decode_S_type(instruction, result, buf_size);
            break;
        case SB_TYPE:
            decode_B_type(instruction, result, buf_size, addr, symbols);
            break;
        case U_TYPE:
            decode_U_type(instruction, result, buf_size);
            break;
        case UJ_TYPE:
            decode_J_type(instruction, result, buf_size, addr, symbols);
            break;
        default:
            snprintf(result, buf_size, "Unknown instruction (0x%08x)", instruction);
    }
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
    int32_t imm = (int32_t)(instruction >> 20); 

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
        if (funct3 == 0x0) snprintf(result, buf_size, "addi x%d, x%d, %d", rd, rs1, imm);
        else snprintf(result, buf_size, "Unknown I-Type");
    } else {
        snprintf(result, buf_size, "Unknown I-Type");
    }

}

//S-Type instructions
void decode_S_type(uint32_t instruction, char* result, size_t buf_size) {
    uint32_t funct3 = (instruction >> 12) & 0x07;
    uint32_t rs1 = (instruction >> 15) & 0x1F;
    uint32_t rs2 = (instruction >> 20) & 0x1F;
    int32_t imm = ((instruction >> 7) & 0x1F) | ((instruction >> 25) << 5); //Combine imm[4:0] and imm[11:5]

    if (funct3 == 0x2) {
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
    int32_t imm = ((instruction & 0x80) >> 7) | ((instruction & 0x7E000000) >> 20) |
                  ((instruction & 0xF00) >> 7) | ((instruction & 0x80000000) >> 19);
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
    int32_t imm = ((instruction & 0xFF000) >> 12) | ((instruction & 0x100000) >> 9) |
                  ((instruction & 0x7FE00000) >> 20) | ((instruction & 0x80000000) >> 11);
    imm <<= 1;

    snprintf(result, buf_size, "jal x%d, 0x%x", rd, addr + imm);
}
