#include "assembler.h"
#include "common.h"

#include "instructions.h"
#include "stb_ds.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAX_LINE_LENGTH 1024
#define MAX_LABEL_LENGTH 256

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

Chip8Instruction parse_line(char *line, char **out_label) {
	// TODO: Handle registers as hex, e.g., VF vs V15
	*out_label = NULL;
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
		if (*args < '0' || *args > '9') {
			*out_label = args;
		} else {
			instruction.aformat.addr = strtol(args, NULL, 0);
		}
	} else if (strncmp("JMP", opcode, opcode_len) == 0) {
		if (*args == 'V') {
			// Bnnn - JMP V0, addr
			instruction.aformat.opcode = 0xB;
			args = next_arg(args);
		} else {
			// 1nnn - JMP addr
			instruction.aformat.opcode = 0x1;
		}
		if (*args < '0' || *args > '9') {
			*out_label = args;
		} else {
			instruction.aformat.addr = strtol(args, NULL, 0);
		}
	} else if (strncmp("CALL", opcode, opcode_len) == 0) {
		instruction.aformat.opcode = 0x2;
		if (*args < '0' || *args > '9') {
			*out_label = args;
		} else {
			instruction.aformat.addr = strtol(args, NULL, 0);
		}
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
			args = next_arg(args);
			if (*args < '0' || *args > '9') {
				*out_label = args;
			} else {
				instruction.aformat.addr = strtol(args, NULL, 0);
			}
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
		char *arg2 = next_arg(args);
		if (*arg2 == 'V') {
			if (*args == 'V') {
				// 8xy4 - ADD Vx, Vy
				instruction.rformat.opcode = 0x8;
				instruction.rformat.rx = parse_num(args + 1);
				instruction.rformat.ry = parse_num(arg2 + 1);
				instruction.rformat.imm = 0x4;
			} else {
				// Fx1E - ADD I, Vx
				instruction.iformat.opcode = 0xF;
				instruction.iformat.imm = 0x1E;
				instruction.iformat.reg = parse_num(arg2 + 1);
			}
		} else {
			// 7xkk - ADD Vx, byte
			instruction.iformat.opcode = 0x7;
			instruction.iformat.reg = parse_num(args + 1);
			instruction.iformat.imm = strtol(arg2, NULL, 0);
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

uint8_t *assemble(char *source_filename) {
	// TODO: Debugging symbols
	bool valid = true;

	uint8_t *data = NULL;

	struct {
		char *key; // label
		size_t value; // index of instruction
	} *labels = NULL;
	sh_new_arena(labels);

	struct {
		size_t key; // index of instruction
		char *value; // label
	} *to_patch = NULL;

	FILE *source = fopen(source_filename, "r");
	char line[MAX_LINE_LENGTH];
	size_t line_num = 0;
	uint8_t byte = 0;
	bool last_processed_label = false;
	while (fgets(line, sizeof(line), source)) {
		line_num++;
		char *trimmed_line = trim(line);

		// TODO: Handle raw hex data
		if (!*trimmed_line || *trimmed_line == '#') {
			continue;
		}

		if (*trimmed_line == '0') {
			// TODO: Patch labels to point to correct addresses
			// TODO: Patch instructions to point to addresses
			if (!last_processed_label) {
				// Data has no label assigned to it since the last label
				// was assigned to an instruction, or there is no label
				fprintf(stderr,
					"[!] No label has been assigned to the data on line %zu: %s",
					line_num, trimmed_line);
				valid = false;
			}

			char *c = trimmed_line;
			bool first_nibble = true;
			while (*c && *c != '\n') {
				*c = tolower(*c);
				if (*c == ' ') {
					c++;
					continue;
				} else if (*c == '0' && (*(c + 1) == 'x' || *(c + 1) == 'X')) {
					c += 2;
					continue;
				}

				uint8_t nibble = 0;
				if (*c >= 'a' && *c <= 'f') {
					nibble = *c - 'a' + 10;
				} else if (*c >= '0' && *c <= '9') {
					nibble = *c - '0';
				} else {
					fprintf(stderr, "[!] Invalid hex on line %zu: %s", line_num,
						trimmed_line);
					valid = false;
					break;
				}

				if (first_nibble) {
					byte = nibble << 4;
					first_nibble = false;
				} else {
					byte |= nibble;
					arrput(data, 0xff & byte);
					printf("%d ", byte);
					byte = 0;
					first_nibble = true;
				}

				c++;
			}

			printf("\n");

			continue;
		}

		if (byte > 0) {
			arrput(data, byte >> 4);
		}

		char *label = NULL;
		Chip8Instruction instruction = parse_line(trimmed_line, &label);
		if (!instruction.raw) {
			char *last_char = trimmed_line;
			while (!isspace((unsigned char)*last_char)) {
				++last_char;
			}
			if (*(last_char - 1) == ':') {
				// Label handling
				last_processed_label = true;
				size_t label_len = last_char - trimmed_line - 1;
				if (label_len < MAX_LABEL_LENGTH) {
					char *new_label = malloc(label_len + 1);
					memcpy(new_label, trimmed_line, label_len);
					shput(labels, new_label, arrlen(data));
				} else {
					fprintf(stderr,
						"[!] Label on line %zu is too long! Max %d characters allowed.",
						line_num, MAX_LABEL_LENGTH);
					valid = false;
				}
			} else {
				last_processed_label = false;
				fprintf(stderr, "[!] Unknown instruction on line %zu: %s", line_num,
					trimmed_line);
				valid = false;
			}
		} else {
			arrput(data, (instruction.raw & 0xff00) >> 8);
			arrput(data, instruction.raw & 0xff);
			last_processed_label = false;
			if (label) {
				// Track instructions with labels
				char *temp = label;
				while (!isspace((unsigned char)*temp)) {
					temp++;
				}

				size_t label_len = temp - label;
				if (label_len < MAX_LABEL_LENGTH) {
					char *new_label = malloc(label_len + 1);
					memcpy(new_label, label, label_len);
					hmput(to_patch, arrlen(data) - 2, new_label);
				}
			}
		}
	}

	for (size_t i = 0; i < hmlen(to_patch); i++) {
		// Patch labels with hardcoded addresses
		size_t offset = to_patch[i].key;
		Chip8Instruction instruction = bytes2inst(data + offset);
		printf("%04hx\n", instruction.raw);
		instruction.aformat.addr = shgets(labels, to_patch[i].value).value + PROG_BASE;
		printf("Patching %s to be 0x%hx\n", to_patch[i].value, instruction.aformat.addr);
		printf("%04hx\n", instruction.raw);
		data[offset] = (instruction.raw & 0xff00) >> 8;
		data[offset + 1] = instruction.raw & 0xff;
	}

	fclose(source);
	shfree(labels);
	hmfree(to_patch);

	if (!valid) {
		return NULL;
	}

	return data;
}
