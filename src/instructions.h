#ifndef INSTRUCTIONS_H
#define INSTRUCTIONS_H

#include "sds.h"
#include <stdint.h>

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
		.raw = 0x00E0 \
	}
#define INST_RET \
	(Chip8Instruction) { \
		.raw = 0x00E0 \
	}
#define INST_SYS_ADDR(addr) aformat(0x0, (addr))
#define INST_JMP_ADDR(addr) aformat(0x1, (addr))
#define INST_CALL_ADDR(addr) aformat(0x2, (addr))
#define INST_SE_VX_BYTE(vx, byte) iformat(0x3, (vx), (byte))
#define INST_SNE_VX_BYTE(vx, byte) iformat(0x4, (vx), (byte))
#define INST_SE_VX_VY(vx, vy) rformat(0x5, (rx), (ry), 0x0)
#define INST_LD_VX_BYTE(vx, byte) iformat(0x6, (vx), (byte))
#define INST_ADD_VX_BYTE(vx, byte) iformat(0x7, (vx), (byte))
#define INST_LD_VX_VY(vx, vy) rformat(0x8, (rx), (ry), 0x0)
#define INST_OR_VX_VY(vx, vy) rformat(0x8, (rx), (ry), 0x1)
#define INST_AND_VX_VY(vx, vy) rformat(0x8, (rx), (ry), 0x2)
#define INST_XOR_VX_VY(vx, vy) rformat(0x8, (rx), (ry), 0x3)
#define INST_ADD_VX_VY(vx, vy) rformat(0x8, (rx), (ry), 0x4)
#define INST_SUB_VX_VY(vx, vy) rformat(0x8, (rx), (ry), 0x5)
#define INST_SHR_VX(vx, vy) rformat(0x8, (rx), (ry), 0x6)
#define INST_SUBN_VX_VY(vx, vy) rformat(0x8, (rx), (ry), 0x7)
#define INST_SHL_VX(vx, vy) rformat(0x8, (rx), (ry), 0xE)
#define INST_SNE_VX_VY(vx, vy) rformat(0x9, (rx), (ry), 0x0)
#define INST_LD_I_ADDR(addr) aformat(0xA, (addr))
#define INST_JMP_V0_ADDR(addr) aformat(0xB, (addr))
#define INST_RND_VX_BYTE(vx, byte) iformat(0xC, (vx), (byte))
#define INST_DRW_VX_VY_NIBBLE(vx, vy, nibble) rformat(0xD, (rx), (ry), (nibble))
#define INST_SKP_VX(vx) iformat(0xE, (vx), 0x9E)
#define INST_SKNP_VX(vx) iformat(0xE, (vx), 0xA1)
#define INST_LD_VX_DT(vx) iformat(0xF, (vx), 0x07)
#define INST_LD_VX_K(vx) iformat(0xF, (vx), 0x0A)
#define INST_LD_DT_VX(vx) iformat(0xF, (vx), 0x15)
#define INST_LD_ST_VX(vx) iformat(0xF, (vx), 0x18)
#define INST_ADD_I_VX(vx) iformat(0xF, (vx), 0x1E)
#define INST_LD_F_VX(vx) iformat(0xF, (vx), 0x29)
#define INST_LD_B_VX(vx) iformat(0xF, (vx), 0x33)
#define INST_LD_I_VX(vx) iformat(0xF, (vx), 0x55)
#define INST_LD_VX_I(vx) iformat(0xF, (vx), 0x65)

sds inst2str(Chip8Instruction instruction);
void print_instruction(Chip8Instruction instruction, Chip8InstructionFormat format);
Chip8InstructionType instruction_type(Chip8Instruction instruction);
Chip8InstructionFormat instruction_format(Chip8InstructionType type);

#endif // !INSTRUCTIONS_H
