#ifndef INSTRUCTIONS_H
#define INSTRUCTIONS_H

#include "sds.h"
#include <stdint.h>

#define OP_CLS 0x00E0
#define OP_RET 0x00EE
#define OP_SYS_ADDR 0x0
#define OP_JMP_ADDR 0x1
#define OP_CALL_ADDR 0x2
#define OP_SE_VX_BYTE 0x3
#define OP_SNE_VX_BYTE 0x4
#define OP_SE_VX_VY 0x5
#define IMM_SE_VX_VY 0x0
#define OP_LD_VX_BYTE 0x6
#define OP_ADD_VX_BYTE 0x7
#define OP_LD_VX_VY 0x8
#define IMM_LD_VX_VY 0x0
#define OP_OR_VX_VY 0x8
#define IMM_OR_VX_VY 0x1
#define OP_AND_VX_VY 0x8
#define IMM_AND_VX_VY 0x2
#define OP_XOR_VX_VY 0x8
#define IMM_XOR_VX_VY 0x3
#define OP_ADD_VX_VY 0x8
#define IMM_ADD_VX_VY 0x4
#define OP_SUB_VX_VY 0x8
#define IMM_SUB_VX_VY 0x5
#define OP_SHR_VX 0x8
#define IMM_SHR_VX 0x6
#define OP_SUBN_VX_VY 0x8
#define IMM_SUBN_VX_VY 0x7
#define OP_SHL_VX 0x8
#define IMM_SHL_VX 0xE
#define OP_SNE_VX_VY 0x9
#define IMM_SNE_VX_VY 0x0
#define OP_LD_I_ADDR 0xA
#define OP_JMP_V0_ADDR 0xB
#define OP_RND_VX_BYTE 0xC
#define OP_DRW_VX_VY_NIBBLE 0xD
#define OP_SKP_VX 0xE
#define IMM_SKP_VX 0x9E
#define OP_SKNP_VX 0xE
#define IMM_SKNP_VX 0xA1
#define OP_LD_VX_DT 0xF
#define IMM_LD_VX_DT 0x07
#define OP_LD_VX_K 0xF
#define IMM_LD_VX_K 0x0A
#define OP_LD_DT_VX 0xF
#define IMM_LD_DT_VX 0x15
#define OP_LD_ST_VX 0xF
#define IMM_LD_ST_VX 0x18
#define OP_ADD_I_VX 0xF
#define IMM_ADD_I_VX 0x1E
#define OP_LD_F_VX 0xF
#define IMM_LD_F_VX 0x29
#define OP_LD_B_VX 0xF
#define IMM_LD_B_VX 0x33
#define OP_LD_I_VX 0xF
#define IMM_LD_I_VX 0x55
#define OP_LD_VX_I 0xF
#define IMM_LD_VX_I 0x65

// Standard instructions
typedef enum {
	CHIP8_CLS, // 00E0
	CHIP8_RET, // 00EE
	CHIP8_SYS_ADDR, // 0nnn
	CHIP8_JMP_ADDR, // 1nnn
	CHIP8_CALL_ADDR, // 2nnn
	CHIP8_SE_VX_BYTE, // 3xkk
	CHIP8_SNE_VX_BYTE, // 4xkk
	CHIP8_SE_VX_VY, // 5xy0
	CHIP8_LD_VX_BYTE, // 6xkk
	CHIP8_ADD_VX_BYTE, // 7xkk
	CHIP8_LD_VX_VY, // 8xy0
	CHIP8_OR_VX_VY, // 8xy1
	CHIP8_AND_VX_VY, // 8xy2
	CHIP8_XOR_VX_VY, // 8xy3
	CHIP8_ADD_VX_VY, // 8xy4
	CHIP8_SUB_VX_VY, // 8xy5
	CHIP8_SHR_VX, // 8xy6
	CHIP8_SUBN_VX_VY, // 8xy7
	CHIP8_SHL_VX, // 8xyE
	CHIP8_SNE_VX_VY, // 9xy0
	CHIP8_LD_I_ADDR, // Annn
	CHIP8_JMP_V0_ADDR, // Bnnn
	CHIP8_RND_VX_BYTE, // Cxkk
	CHIP8_DRW_VX_VY_NIBBLE, // Dxyn
	CHIP8_SKP_VX, // Ex9E
	CHIP8_SKNP_VX, // ExA1
	CHIP8_LD_VX_DT, // Fx07
	CHIP8_LD_VX_K, // Fx0A
	CHIP8_LD_DT_VX, // Fx15
	CHIP8_LD_ST_VX, // Fx18
	CHIP8_ADD_I_VX, // Fx1E
	CHIP8_LD_F_VX, // Fx29
	CHIP8_LD_B_VX, // Fx33
	CHIP8_LD_I_VX, // Fx55
	CHIP8_LD_VX_I, // Fx65
	CHIP8_UNKNOWN,
} Chip8InstructionType;

typedef struct {
	uint16_t imm : 4;
	uint16_t ry : 4;
	uint16_t rx : 4;
	uint16_t opcode : 4;
} RFormat;

typedef struct {
	uint16_t imm : 8;
	uint16_t reg : 4;
	uint16_t opcode : 4;
} IFormat;

typedef struct {
	uint16_t addr : 12;
	uint16_t opcode : 4;
} AFormat;

typedef enum {
	R_FORMAT,
	I_FORMAT,
	A_FORMAT,
	UNKNOWN_FORMAT,
} Chip8InstructionFormat;

typedef union {
	uint16_t raw;
	RFormat rformat;
	IFormat iformat;
	AFormat aformat;
} Chip8Instruction;

static inline Chip8Instruction bytes2inst(uint8_t *bytes) {
	// Instruction size = 2 bytes (big endian)
	Chip8Instruction instruction = { .raw = (uint16_t)((bytes[0] << 8) | bytes[1]) };
	return instruction;
}

static inline Chip8Instruction rformat(uint8_t opcode, uint8_t rx, uint8_t ry, uint8_t imm) {
	Chip8Instruction instruction = { .rformat = {
						 .opcode = opcode,
						 .rx = rx,
						 .ry = ry,
						 .imm = imm,
					 } };
	return instruction;
}

static inline Chip8Instruction iformat(uint8_t opcode, uint8_t reg, uint8_t imm) {
	Chip8Instruction instruction = { .iformat = {
						 .opcode = opcode,
						 .reg = reg,
						 .imm = imm,
					 } };
	return instruction;
}

static inline Chip8Instruction aformat(uint8_t opcode, uint16_t addr) {
	Chip8Instruction instruction = { .aformat = {
						 .opcode = opcode,
						 .addr = addr,
					 } };
	return instruction;
}

#define INST_CLS \
	(Chip8Instruction) { \
		.raw = OP_CLS \
	}
#define INST_RET \
	(Chip8Instruction) { \
		.raw = OP_RET \
	}
#define INST_SYS_ADDR(addr) aformat(OP_SYS_ADDR, (addr))
#define INST_JMP_ADDR(addr) aformat(OP_JMP_ADDR, (addr))
#define INST_CALL_ADDR(addr) aformat(OP_CALL_ADDR, (addr))
#define INST_SE_VX_BYTE(vx, byte) iformat(OP_SE_VX_BYTE, (vx), (byte))
#define INST_SNE_VX_BYTE(vx, byte) iformat(OP_SNE_VX_BYTE, (vx), (byte))
#define INST_SE_VX_VY(vx, vy) rformat(OP_SE_VX_VY, (vx), (vy), IMM_SE_VX_VY)
#define INST_LD_VX_BYTE(vx, byte) iformat(OP_LD_VX_BYTE, (vx), (byte))
#define INST_ADD_VX_BYTE(vx, byte) iformat(OP_ADD_VX_BYTE, (vx), (byte))
#define INST_LD_VX_VY(vx, vy) rformat(OP_LD_VX_VY, (vx), (vy), IMM_LD_VX_VY)
#define INST_OR_VX_VY(vx, vy) rformat(OP_OR_VX_VY, (vx), (vy), IMM_OR_VX_VY)
#define INST_AND_VX_VY(vx, vy) rformat(OP_AND_VX_VY, (vx), (vy), IMM_AND_VX_VY)
#define INST_XOR_VX_VY(vx, vy) rformat(OP_XOR_VX_VY, (vx), (vy), IMM_XOR_VX_VY)
#define INST_ADD_VX_VY(vx, vy) rformat(OP_ADD_VX_VY, (vx), (vy), IMM_AND_VX_VY)
#define INST_SUB_VX_VY(vx, vy) rformat(OP_SUB_VX_VY, (vx), (vy), IMM_SUB_VX_VY)
#define INST_SHR_VX(vx, vy) rformat(OP_SHR_VX, (vx), (vy), IMM_SHR_VX)
#define INST_SUBN_VX_VY(vx, vy) rformat(OP_SUBN_VX_VY, (vx), (vy), IMM_SUBN_VX_VY)
#define INST_SHL_VX(vx, vy) rformat(OP_SHL_VX, (vx), (vy), IMM_SHL_VX)
#define INST_SNE_VX_VY(vx, vy) rformat(OP_SNE_VX_VY, (vx), (vy), IMM_SNE_VX_VY)
#define INST_LD_I_ADDR(addr) aformat(OP_LD_I_ADDR, (addr))
#define INST_JMP_V0_ADDR(addr) aformat(OP_JMP_V0_ADDR, (addr))
#define INST_RND_VX_BYTE(vx, byte) iformat(OP_RND_VX_BYTE, (vx), (byte))
#define INST_DRW_VX_VY_NIBBLE(vx, vy, nibble) rformat(OP_DRW_VX_VY_NIBBLE, (vx), (vy), (nibble))
#define INST_SKP_VX(vx) iformat(OP_SKP_VX, (vx), IMM_SKP_VX)
#define INST_SKNP_VX(vx) iformat(OP_SKNP_VX, (vx), IMM_SKNP_VX)
#define INST_LD_VX_DT(vx) iformat(OP_LD_VX_DT, (vx), IMM_LD_VX_DT)
#define INST_LD_VX_K(vx) iformat(OP_LD_VX_K, (vx), IMM_LD_VX_K)
#define INST_LD_DT_VX(vx) iformat(OP_LD_DT_VX, (vx), IMM_LD_DT_VX)
#define INST_LD_ST_VX(vx) iformat(IMM_LD_ST_VX, (vx), IMM_LD_ST_VX)
#define INST_ADD_I_VX(vx) iformat(OP_ADD_I_VX, (vx), IMM_ADD_I_VX)
#define INST_LD_F_VX(vx) iformat(OP_LD_F_VX, (vx), IMM_LD_F_VX)
#define INST_LD_B_VX(vx) iformat(OP_LD_B_VX, (vx), IMM_LD_B_VX)
#define INST_LD_I_VX(vx) iformat(OP_LD_I_VX, (vx), IMM_LD_I_VX)
#define INST_LD_VX_I(vx) iformat(OP_LD_VX_I, (vx), IMM_LD_VX_I)

sds inst2str(Chip8Instruction instruction);
void print_instruction(Chip8Instruction instruction, Chip8InstructionFormat format);
Chip8InstructionType instruction_type(Chip8Instruction instruction);
Chip8InstructionFormat instruction_format(Chip8InstructionType type);

#endif // !INSTRUCTIONS_H
