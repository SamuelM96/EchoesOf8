#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include <stdint.h>
#include <stdlib.h>

/*
 * Standard instructions
 *   00E0 - CLS
 *   00EE - RET
 *   0nnn - SYS addr
 *   1nnn - JP addr
 *   2nnn - CALL addr
 *   3xkk - SE Vx, byte
 *   4xkk - SNE Vx, byte
 *   5xy0 - SE Vx, Vy
 *   6xkk - LD Vx, byte
 *   7xkk - ADD Vx, byte
 *   8xy0 - LD Vx, Vy
 *   8xy1 - OR Vx, Vy
 *   8xy2 - AND Vx, Vy
 *   8xy3 - XOR Vx, Vy
 *   8xy4 - ADD Vx, Vy
 *   8xy5 - SUB Vx, Vy
 *   8xy6 - SHR Vx {, Vy}
 *   8xy7 - SUBN Vx, Vy
 *   8xyE - SHL Vx {, Vy}
 *   9xy0 - SNE Vx, Vy
 *   Annn - LD I, addr
 *   Bnnn - JP V0, addr
 *   Cxkk - RND Vx, byte
 *   Dxyn - DRW Vx, Vy, nibble
 *   Ex9E - SKP Vx
 *   ExA1 - SKNP Vx
 *   Fx07 - LD Vx, DT
 *   Fx0A - LD Vx, K
 *   Fx15 - LD DT, Vx
 *   Fx18 - LD ST, Vx
 *   Fx1E - ADD I, Vx
 *   Fx29 - LD F, Vx
 *   Fx33 - LD B, Vx
 *   Fx55 - LD [I], Vx
 *   Fx65 - LD Vx, [I]
 */

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
} Chip8InstructionFormat;

typedef union {
	uint16_t raw;
	RFormat rformat;
	IFormat iformat;
	AFormat aformat;
} Chip8Instruction;

static inline Chip8Instruction bytes2inst(uint8_t *bytes) {
	// Instruction size = 2 bytes (big endian)
	return (Chip8Instruction)(uint16_t)((bytes[0] << 8) | bytes[1]);
}

void hexdump(void *buffer, size_t length, size_t base);
void print_asm(Chip8Instruction instruction);
void print_instruction(Chip8Instruction instruction, Chip8InstructionFormat format);
void disassemble_rd(uint8_t *code, size_t length, size_t base);
void disassemble_linear(uint8_t *code, size_t length, size_t base);

#endif // !DISASSEMBLER_H
