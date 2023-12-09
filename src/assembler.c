#include "assembler.h"
#include "disassembler.h"

#include "../lib/stb_ds.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAX_LINE_LENGTH 1024

static inline char *trim(char *str) {
	while (isspace((unsigned char)*str))
		str++;
	return str;
}

int parse_num(char *str) {
	int num = 0;
	char *digit = str;
	while (*digit >= '0' && *digit <= '9') {
		num = num * 10 + (*digit - '0');
		digit++;
	}
	return num;
}

static inline char *next_arg(char *args) {
	while (!isspace((unsigned char)*args)) {
		++args;
	}
	return trim(args);
}

Chip8Instruction parse_line(char *line) {
	// TODO: Handle labels
	char *opcode = line;
	while (!isspace((unsigned char)*opcode)) {
		++opcode;
	}

	uint8_t opcode_len = opcode - line;
	char *args = trim(opcode);
	opcode = line;

	Chip8Instruction instruction;
	if (strncmp("CLS", opcode, opcode_len) == 0) {
		instruction.raw = 0x00E0;
	} else if (strncmp("RET", opcode, opcode_len) == 0) {
		instruction.raw = 0x00EE;
	} else if (strncmp("SYS", opcode, opcode_len) == 0) {
		instruction.aformat.opcode = 0x0;
		instruction.aformat.addr = strtol(args, NULL, 0);
	} else if (strncmp("JMP", opcode, opcode_len) == 0) {
		if (*args == 'V') {
			// Bnnn - JMP V0, addr
			instruction.aformat.opcode = 0xB;
			args = next_arg(args);
		} else {
			// 1nnn - JMP addr
			instruction.aformat.opcode = 0x1;
		}
		instruction.aformat.addr = strtol(args, NULL, 0);
	} else if (strncmp("CALL", opcode, opcode_len) == 0) {
		instruction.aformat.opcode = 0x2;
		instruction.aformat.addr = strtol(args, NULL, 0);
	} else if (strncmp("SE", opcode, opcode_len) == 0) {
		uint8_t xreg = parse_num(args + 1);
		char *arg2 = next_arg(args);
		if (*arg2 == 'V') {
			// 5xy0 - SE Vx, Vy
			instruction.rformat.opcode = 0x5;
			instruction.rformat.rx = xreg;
			instruction.rformat.ry = parse_num(arg2 + 1);
			instruction.rformat.imm = 0x0;
		} else {
			// 3xkk - SE Vx, byte
			instruction.iformat.opcode = 0x6;
			instruction.iformat.reg = xreg;
			instruction.iformat.imm = strtol(arg2, NULL, 0);
		}
	} else if (strncmp("SNE", opcode, opcode_len) == 0) {
		uint8_t xreg = parse_num(args + 1);
		char *arg2 = next_arg(args);
		if (*arg2 == 'V') {
			// 9xy0 - SNE Vx, Vy
			instruction.rformat.opcode = 0x9;
			instruction.rformat.rx = xreg;
			instruction.rformat.ry = parse_num(arg2 + 1);
			instruction.rformat.imm = 0x0;
		} else {
			// 4xkk - SNE Vx, byte
			instruction.iformat.opcode = 0x4;
			instruction.iformat.reg = xreg;
			instruction.iformat.imm = strtol(arg2, NULL, 0);
		}
	} else if (strncmp("LD", opcode, opcode_len) == 0) {
		if (*args == 'I') {
			// Annn - LD I, addr
			instruction.aformat.opcode = 0xA;
			instruction.aformat.addr = strtol(next_arg(args), NULL, 0);
		} else if (*args == 'D') {
			// Fx15 - LD DT, Vx
			instruction.iformat.opcode = 0xF;
			instruction.iformat.imm = 0x15;
			instruction.iformat.reg = parse_num(next_arg(args) + 1);
		} else if (*args == 'S') {
			// Fx18 - LD ST, Vx
			instruction.iformat.opcode = 0xF;
			instruction.iformat.imm = 0x18;
			instruction.iformat.reg = parse_num(next_arg(args) + 1);
		} else if (*args == 'F') {
			// Fx29 - LD F, Vx
			instruction.iformat.opcode = 0xF;
			instruction.iformat.imm = 0x29;
			instruction.iformat.reg = parse_num(next_arg(args) + 1);
		} else if (*args == 'B') {
			// Fx33 - LD B, Vx
			instruction.iformat.opcode = 0xF;
			instruction.iformat.imm = 0x33;
			instruction.iformat.reg = parse_num(next_arg(args) + 1);
		} else if (*args == '[') {
			// Fx55 - LD [I], Vx
			instruction.iformat.opcode = 0xF;
			instruction.iformat.imm = 0x55;
			instruction.iformat.reg = parse_num(next_arg(args) + 1);
		} else if (*args == 'V') {
			instruction.rformat.rx = parse_num(args + 1);
			char *arg2 = next_arg(args);
			if (*arg2 == 'V') {
				// 8xy0 - LD Vx, Vy
				instruction.rformat.opcode = 0x8;
				instruction.rformat.ry = parse_num(arg2 + 1);
			} else if (*arg2 == 'D') {
				// Fx07 - LD Vx, DT
				instruction.iformat.opcode = 0xF;
				instruction.iformat.imm = 0x07;
			} else if (*arg2 == '[') {
				// Fx65 - LD Vx, [I]
				instruction.iformat.opcode = 0xF;
				instruction.iformat.imm = 0x65;
			} else if (*arg2 == 'K') {
				// Fx0A - LD Vx, K
				instruction.iformat.opcode = 0xF;
				instruction.iformat.imm = 0x0A;
			} else {
				// 6xkk - LD Vx, byte
				instruction.iformat.opcode = 0x6;
				instruction.iformat.imm = strtol(arg2, NULL, 0);
			}
		}
	} else if (strncmp("ADD", opcode, opcode_len) == 0) {
		if (*args == 'V') {
			// 8xy4 - ADD Vx, Vy
			instruction.rformat.opcode = 0x8;
			instruction.rformat.rx = parse_num(args + 1);
			args = next_arg(args);
			instruction.rformat.ry = parse_num(args + 1);
			instruction.rformat.imm = 0x4;
		} else if (*args == 'I') {
			// Fx1E - ADD I, Vx
			instruction.iformat.opcode = 0xF;
			instruction.iformat.imm = 0x1E;
			instruction.iformat.reg = parse_num(next_arg(args) + 1);
		} else {
			// 7xkk - ADD Vx, byte
			instruction.iformat.opcode = 0x7;
			instruction.iformat.reg = parse_num(args + 1);
			instruction.iformat.imm = strtol(next_arg(args), NULL, 0);
		}
	} else if (strncmp("OR", opcode, opcode_len) == 0) {
		// 8xy1 - OR Vx, Vy
		instruction.rformat.opcode = 0x8;
		instruction.rformat.rx = parse_num(args + 1);
		args = next_arg(args);
		instruction.rformat.ry = parse_num(args + 1);
		instruction.rformat.imm = 0x1;
	} else if (strncmp("AND", opcode, opcode_len) == 0) {
		// 8xy2 - AND Vx, Vy
		instruction.rformat.opcode = 0x8;
		instruction.rformat.rx = parse_num(args + 1);
		args = next_arg(args);
		instruction.rformat.ry = parse_num(args + 1);
		instruction.rformat.imm = 0x2;
	} else if (strncmp("XOR", opcode, opcode_len) == 0) {
		// 8xy3 - XOR Vx, Vy
		instruction.rformat.opcode = 0x8;
		instruction.rformat.rx = parse_num(args + 1);
		args = next_arg(args);
		instruction.rformat.ry = parse_num(args + 1);
		instruction.rformat.imm = 0x3;
	} else if (strncmp("SUB", opcode, opcode_len) == 0) {
		// 8xy5 - SUB Vx, Vy
		instruction.rformat.opcode = 0x8;
		instruction.rformat.rx = parse_num(args + 1);
		args = next_arg(args);
		instruction.rformat.ry = parse_num(args + 1);
		instruction.rformat.imm = 0x5;
	} else if (strncmp("SHR", opcode, opcode_len) == 0) {
		// 8xy6 - SHR Vx {, Vy}
		instruction.rformat.opcode = 0x8;
		instruction.rformat.rx = parse_num(args + 1);
		args = next_arg(args);
		instruction.rformat.ry = parse_num(args + 1);
		instruction.rformat.imm = 0x6;
	} else if (strncmp("SUBN", opcode, opcode_len) == 0) {
		// 8xy7 - SUBN Vx, Vy
		instruction.rformat.opcode = 0x8;
		instruction.rformat.rx = parse_num(args + 1);
		args = next_arg(args);
		instruction.rformat.ry = parse_num(args + 1);
		instruction.rformat.imm = 0x7;
	} else if (strncmp("SHL", opcode, opcode_len) == 0) {
		// 8xyE - SHL Vx {, Vy}
		instruction.rformat.opcode = 8;
		instruction.rformat.rx = parse_num(args + 1);
		args = next_arg(args);
		instruction.rformat.ry = parse_num(args + 1);
		instruction.rformat.imm = 0xE;
	} else if (strncmp("RND", opcode, opcode_len) == 0) {
		// Cxkk - RND Vx, byte
		instruction.iformat.opcode = 0xC;
		instruction.iformat.reg = parse_num(args + 1);
		instruction.iformat.imm = strtol(next_arg(args), NULL, 0);
	} else if (strncmp("DRW", opcode, opcode_len) == 0) {
		// Dxyn - DRW Vx, Vy, nibble
		instruction.rformat.opcode = 0xD;
		instruction.rformat.rx = parse_num(args + 1);
		args = next_arg(args);
		instruction.rformat.ry = parse_num(args + 1);
		instruction.rformat.imm = parse_num(next_arg(args));
	} else if (strncmp("SKP", opcode, opcode_len) == 0) {
		// Ex9E - SKP Vx
		instruction.iformat.opcode = 0xE;
		instruction.iformat.reg = parse_num(args + 1);
		instruction.iformat.imm = 0x9E;
	} else if (strncmp("SKNP", opcode, opcode_len) == 0) {
		// ExA1 - SKNP Vx
		instruction.iformat.opcode = 0xE;
		instruction.iformat.reg = parse_num(args + 1);
		instruction.iformat.imm = 0xA1;
	}

	return instruction;
}

Chip8Instruction *assemble(char *source_filename) {
	Chip8Instruction *instructions = NULL;
	FILE *source = fopen(source_filename, "r");
	char line[MAX_LINE_LENGTH];

	while (fgets(line, sizeof(line), source)) {
		char *trimmed_line = trim(line);

		if (!*trimmed_line || *trimmed_line == '#') {
			continue;
		}

		Chip8Instruction instruction = parse_line(trimmed_line);
		arrput(instructions, instruction); // Entry point
	}

	fclose(source);

	return instructions;
}
