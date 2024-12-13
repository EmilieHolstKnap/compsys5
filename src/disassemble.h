#ifndef DISASSEMBLE_H
#define DISASSEMBLE_H
#include <stddef.h>
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
    OPCODE_R      = 0x33,
    OPCODE_I_LOAD = 0x03,
    OPCODE_I_IMM  = 0x13,
    OPCODE_S      = 0x23,
    OPCODE_SB     = 0x63,
    OPCODE_U      = 0x37,
    OPCODE_UJ     = 0x6F,
} Opcode;

// Decoded instruction structure
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


struct symbols;
void disassemble(uint32_t addr, uint32_t instruction, char* result, size_t buf_size, struct symbols* symbols);

const char* get_mnemonic(DecodedInstruction decoded);
DecodedInstruction decode_instruction(uint32_t instruction);
uint32_t extract_bits(uint32_t instruction, int start, int length);
int32_t decode_imm(uint32_t instruction, InstructionType type);

#endif 
