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

int8_t parse_reg(char *arg, size_t line_num, char *line) {
	int8_t result = -1;
	if (*arg == 'V') {
		char reg = tolower(*(arg + 1));
		if ('a' <= reg && 'f' >= reg) {
			result = reg - 'a';
		} else if ('0' <= reg && '9' >= reg) {
			return reg - '0';
		}
	}

	if (result == -1) {
		fprintf(stderr, "[!] Invalid register on line %zu: %s\n", line_num, line);
	}

	return result;
}

Chip8Instruction parse_line(char *line, size_t line_num, char **out_label) {
	*out_label = NULL;
	char *opcode = line;
	while (!isspace((unsigned char)*opcode)) {
		++opcode;
	}

	uint8_t opcode_len = opcode - line;
	char *args = trim(opcode);
	opcode = line;

	// NOTE: Could be switched out for nested switch statements,
	// but the number of keywords is tiny that I doubt there'll
	// be many benfits over simple strncmp.
	Chip8Instruction instruction;
	if (strncmp("CLS", opcode, opcode_len) == 0) {
		instruction = INST_CLS;
	} else if (strncmp("RET", opcode, opcode_len) == 0) {
		instruction = INST_RET;
	} else if (strncmp("SYS", opcode, opcode_len) == 0) {
		if (*args < '0' || *args > '9') {
			*out_label = args;
		} else {
			instruction = INST_SYS_ADDR(strtol(args, NULL, 0));
		}
	} else if (strncmp("JMP", opcode, opcode_len) == 0) {
		if (*args == 'V') {
			args = next_arg(args);
		}

		uint16_t addr;
		if (*args < '0' || *args > '9') {
			*out_label = args;
			addr = 0;
		} else {
			addr = strtol(args, NULL, 0);
		}

		if (*args == 'V') {
			instruction = INST_JMP_V0_ADDR(addr);
		} else {
			instruction = INST_JMP_ADDR(addr);
		}
	} else if (strncmp("CALL", opcode, opcode_len) == 0) {
		uint16_t addr;
		if (*args < '0' || *args > '9') {
			*out_label = args;
			addr = 0;
		} else {
			addr = strtol(args, NULL, 0);
		}
		instruction = INST_CALL_ADDR(addr);
	} else if (strncmp("SE", opcode, opcode_len) == 0) {
		uint8_t vx = parse_reg(args, line_num, line);
		char *arg2 = next_arg(args);
		if (*arg2 == 'V') {
			instruction = INST_SE_VX_VY(vx, parse_reg(arg2, line_num, line));
		} else {
			instruction = INST_SE_VX_BYTE(vx, strtol(arg2, NULL, 0));
		}
	} else if (strncmp("SNE", opcode, opcode_len) == 0) {
		uint8_t vx = parse_reg(args, line_num, line);
		char *arg2 = next_arg(args);
		if (*arg2 == 'V') {
			instruction = INST_SNE_VX_VY(vx, parse_reg(arg2, line_num, line));
		} else {
			instruction = INST_SNE_VX_BYTE(vx, strtol(arg2, NULL, 0));
		}
	} else if (strncmp("LD", opcode, opcode_len) == 0) {
		if (*args == 'I') {
			uint16_t addr;
			args = next_arg(args);
			if (*args < '0' || *args > '9') {
				*out_label = args;
				addr = 0;
			} else {
				addr = strtol(args, NULL, 0);
			}
			instruction = INST_LD_I_ADDR(addr);
		} else if (*args == 'D') {
			instruction = INST_LD_DT_VX(parse_reg(next_arg(args), line_num, line));
		} else if (*args == 'S') {
			instruction = INST_LD_ST_VX(parse_reg(next_arg(args), line_num, line));
		} else if (*args == 'F') {
			instruction = INST_LD_F_VX(parse_reg(next_arg(args), line_num, line));
		} else if (*args == 'B') {
			instruction = INST_LD_B_VX(parse_reg(next_arg(args), line_num, line));
		} else if (*args == '[') {
			instruction = INST_LD_I_VX(parse_reg(next_arg(args), line_num, line));
		} else if (*args == 'V') {
			uint8_t vx = parse_reg(args, line_num, line);
			char *arg2 = next_arg(args);
			if (*arg2 == 'V') {
				instruction = INST_LD_VX_VY(vx, parse_reg(arg2, line_num, line));
			} else if (*arg2 == 'D') {
				instruction = INST_LD_VX_DT(vx);
			} else if (*arg2 == '[') {
				instruction = INST_LD_VX_I(vx);
			} else if (*arg2 == 'K') {
				instruction = INST_LD_VX_K(vx);
			} else {
				instruction = INST_LD_VX_BYTE(vx, strtol(arg2, NULL, 0));
			}
		}
	} else if (strncmp("ADD", opcode, opcode_len) == 0) {
		char *arg2 = next_arg(args);
		if (*arg2 == 'V') {
			if (*args == 'V') {
				instruction = INST_ADD_VX_VY(parse_reg(args, line_num, line),
							     parse_reg(arg2, line_num, line));
			} else {
				instruction = INST_ADD_I_VX(parse_reg(arg2, line_num, line));
			}
		} else {
			instruction = INST_ADD_VX_BYTE(parse_reg(args, line_num, line),
						       strtol(arg2, NULL, 0));
		}
	} else if (strncmp("OR", opcode, opcode_len) == 0) {
		instruction = INST_OR_VX_VY(parse_reg(args, line_num, line),
					    parse_reg(next_arg(args), line_num, line));
	} else if (strncmp("AND", opcode, opcode_len) == 0) {
		instruction = INST_AND_VX_VY(parse_reg(args, line_num, line),
					     parse_reg(next_arg(args), line_num, line));
	} else if (strncmp("XOR", opcode, opcode_len) == 0) {
		instruction = INST_XOR_VX_VY(parse_reg(args, line_num, line),
					     parse_reg(next_arg(args), line_num, line));
	} else if (strncmp("SUB", opcode, opcode_len) == 0) {
		instruction = INST_SUB_VX_VY(parse_reg(args, line_num, line),
					     parse_reg(next_arg(args), line_num, line));
	} else if (strncmp("SHR", opcode, opcode_len) == 0) {
		instruction = INST_SHR_VX(parse_reg(args, line_num, line),
					  parse_reg(next_arg(args), line_num, line));
	} else if (strncmp("SUBN", opcode, opcode_len) == 0) {
		instruction = INST_SUBN_VX_VY(parse_reg(args, line_num, line),
					      parse_reg(next_arg(args), line_num, line));
	} else if (strncmp("SHL", opcode, opcode_len) == 0) {
		instruction = INST_SHL_VX(parse_reg(args, line_num, line),
					  parse_reg(next_arg(args), line_num, line));
	} else if (strncmp("RND", opcode, opcode_len) == 0) {
		instruction = INST_RND_VX_BYTE(parse_reg(args, line_num, line),
					       strtol(next_arg(args), NULL, 0));
	} else if (strncmp("DRW", opcode, opcode_len) == 0) {
		char *arg2 = next_arg(args);
		instruction = INST_DRW_VX_VY_NIBBLE(parse_reg(args, line_num, line),
						    parse_reg(arg2, line_num, line),
						    parse_num(next_arg(arg2)));
	} else if (strncmp("SKP", opcode, opcode_len) == 0) {
		instruction = INST_SKP_VX(parse_reg(args, line_num, line));
	} else if (strncmp("SKNP", opcode, opcode_len) == 0) {
		instruction = INST_SKNP_VX(parse_reg(args, line_num, line));
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

		if (!*trimmed_line || *trimmed_line == '#') {
			continue;
		}

		if (*trimmed_line == '0') {
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
					byte = 0;
					first_nibble = true;
				}

				c++;
			}

			continue;
		}

		if (byte > 0) {
			arrput(data, byte >> 4);
		}

		char *label = NULL;
		Chip8Instruction instruction = parse_line(trimmed_line, line_num, &label);
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
		instruction.aformat.addr = shgets(labels, to_patch[i].value).value + PROG_BASE;
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
