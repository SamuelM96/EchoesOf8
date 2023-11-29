#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>

#define STB_DS_IMPLEMENTATION
#include "../lib/stb_ds.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define PROG_BASE 0x200

// Instruction size = 2 bytes (big endian)
#define INST(code, addr) (((code)[(addr)] << 8) | (code)[(addr) + 1])

void hexdump(void *buffer, size_t length, size_t base) {
	printf("%08zx: ", base);
	for (size_t i = 0; i < length; ++i) {
		printf("%02hhx", ((char *)buffer)[i]);
		if ((i + 1) % 16 == 0) {
			printf("\n%08zx: ", base + i + 1);
		} else if ((i + 1) % 2 == 0) {
			printf(" ");
		}
	}
	printf("\n");
}

void print_instruction(uint16_t instruction) {
	switch (instruction >> 12) {
	case 0:
		switch (0xfff & instruction) {
		case 0x0E0:
			printf("CLS");
			break;
		case 0x0EE:
			printf("RET");
			break;
		default:
			printf("SYS %#03x", 0xfff & instruction);
			break;
		}
		break;
	case 1:
		printf("JMP %#03x", 0xfff & instruction);
		break;
	case 2:
		printf("CALL %#03x", 0xfff & instruction);
		break;
	case 3:
		printf("SE V%d, %#02x", (0xf00 & instruction) >> 8, 0xff & instruction);
		break;
	case 4:
		printf("SNE V%d, %#02x", (0xf00 & instruction) >> 8, 0xff & instruction);
		break;
	case 5:
		printf("SE V%d, V%d", (0xf00 & instruction) >> 8, (0xf0 & instruction) >> 4);
		break;
	case 6:
		printf("LD V%d, %#02x", (0xf00 & instruction) >> 8, 0xff & instruction);
		break;
	case 7:
		printf("ADD V%d, %#02x", (0xf00 & instruction) >> 8, 0xff & instruction);
		break;
	case 8:
		switch (0xf & instruction) {
		case 0:
			printf("LD V%d, V%d", (0xf00 & instruction) >> 8,
			       (0xf0 & instruction) >> 4);
			break;
		case 1:
			printf("OR V%d, V%d", (0xf00 & instruction) >> 8,
			       (0xf0 & instruction) >> 4);
			break;
		case 2:
			printf("AND V%d, V%d", (0xf00 & instruction) >> 8,
			       (0xf0 & instruction) >> 4);
			break;
		case 3:
			printf("XOR V%d, V%d", (0xf00 & instruction) >> 8,
			       (0xf0 & instruction) >> 4);
			break;
		case 4:
			printf("ADD V%d, V%d", (0xf00 & instruction) >> 8,
			       (0xf0 & instruction) >> 4);
			break;
		case 5:
			printf("SUB V%d, V%d", (0xf00 & instruction) >> 8,
			       (0xf0 & instruction) >> 4);
			break;
		case 6:
			printf("SHR V%d, V%d", (0xf00 & instruction) >> 8,
			       (0xf0 & instruction) >> 4);
			break;
		case 7:
			printf("SUBN V%d, V%d", (0xf00 & instruction) >> 8,
			       (0xf0 & instruction) >> 4);
			break;
		case 0xE:
			printf("SHL V%d, V%d", (0xf00 & instruction) >> 8,
			       (0xf0 & instruction) >> 4);
			break;
		}
		break;
	case 9:
		printf("SNE V%d, V%d", (0xf00 & instruction) >> 8, (0xf0 & instruction) >> 4);
		break;
	case 0xA:
		printf("LD I, %#03x", 0xfff & instruction);
		break;
	case 0xB:
		printf("JMP V0, %#03x", 0xfff & instruction);
		break;
	case 0xC:
		printf("RND V%d, %#02x", (0xf00 & instruction) >> 8, 0xff & instruction);
		break;
	case 0xD:
		printf("DRW V%d, V%d, %d", (0xf00 & instruction) >> 8, (0xf0 & instruction) >> 4,
		       0xf & instruction);
		break;
	case 0xE:
		switch (0xff & instruction) {
		case 0x9E:
			printf("SKP V%d", (0xf00 & instruction) >> 8);
			break;
		case 0xA1:
			printf("SKNP V%d", (0xf00 & instruction) >> 8);
			break;
		default:
			printf("unknown");
			break;
		}
		break;
	case 0xF:
		switch (0xff & instruction) {
		case 0x07:
			printf("LD V%d", (0xf00 & instruction) >> 8);
			break;
		case 0x0A:
			printf("LD V%d", (0xf00 & instruction) >> 8);
			break;
		case 0x15:
			printf("LD DT, V%d", (0xf00 & instruction) >> 8);
			break;
		case 0x18:
			printf("LD ST, V%d", (0xf00 & instruction) >> 8);
			break;
		case 0x1E:
			printf("ADD I, V%d", (0xf00 & instruction) >> 8);
			break;
		case 0x29:
			printf("LD F, V%d", (0xf00 & instruction) >> 8);
			break;
		case 0x33:
			printf("LD B, V%d", (0xf00 & instruction) >> 8);
			break;
		case 0x55:
			printf("LD [I], V%d", (0xf00 & instruction) >> 8);
			break;
		case 0x65:
			printf("LD V%d, [I]", (0xf00 & instruction) >> 8);
			break;
		default:
			printf("unknown");
			break;
		}
		break;
	default:
		printf("unknown");
		break;
	}
	printf("\n");
}

// Recursive descent disassembler
void disassemble_rd(uint8_t *code, size_t length, size_t base) {
	if (length % 2 != 0) {
		fprintf(stderr, "Cannot disassemble code: not aligned to 2 bytes");
		return;
	}

	size_t *queue = NULL;
	size_t *processed = NULL;
	arrput(queue, 0); // Entry point

	while (arrlen(queue)) {
		size_t ip = queue[0];
		arrdel(queue, 0);

		printf("===== BLOCK @ 0x%08zx =====\n", ip + PROG_BASE);

		while (ip < length) {
			arrput(processed, ip);
			uint16_t instruction = INST(code, ip);
			printf("0x%08zx  %04hx    ", base + ip, instruction);

			print_instruction(instruction);

			if (instruction == 0x00EE) {
				break;
			}

			uint8_t inst_type = instruction >> 12;
			if (inst_type == 1) { // JMP addr
				// PERF: Better way to check? Sorted array + binary search?
				uint16_t addr = instruction & 0xfff - PROG_BASE;
				bool should_process = true;
				for (size_t i = 0; i < arrlen(processed); ++i) {
					if (processed[i] == addr) {
						should_process = false;
						break;
					}
				}
				if (should_process) {
					arrput(queue, addr);
				}
				break;
			} else if (inst_type == 0xB) { // JMP V0, addr
				// Unconditional jump that requires runtime info
				// - can't disassemble unknown address
				break;
			} else if (inst_type == 0x2) { // CALL addr
				// TODO: Refactor as per JMP instruction
				uint16_t addr = instruction & 0xfff - PROG_BASE;
				bool should_process = true;
				for (size_t i = 0; i < arrlen(processed); ++i) {
					if (processed[i] == addr) {
						should_process = false;
						break;
					}
				}
				if (should_process) {
					arrput(queue, addr);
				}
			}

			ip += 2;
		}
		printf("\n");
	}

	arrfree(queue);
	arrfree(processed);
}

// Linear sweep disassembler
void disassemble_linear(uint8_t *code, size_t length, size_t base) {
	if (length % 2 != 0) {
		fprintf(stderr, "Cannot disassemble code: not aligned to 2 bytes");
		return;
	}

	for (size_t i = 0; i < length; i += 2) {
		uint16_t instruction = INST(code, i);
		printf("0x%08zx  %04hx    ", base + i, instruction);
		print_instruction(instruction);
	}
}

void parse_rom(char *rom_path) {
	FILE *rom = fopen(rom_path, "rb");

	if (rom == NULL) {
		fprintf(stderr, "Error opening ROM file");
		exit(EXIT_FAILURE);
	}

	uint8_t buffer[4096];

	printf("ROM dump\n");
	size_t ret, base = PROG_BASE;
	while ((ret = fread(buffer, sizeof(*buffer), ARRAY_SIZE(buffer), rom))) {
		// hexdump(buffer, ret, base);
		// disassemble_linear(buffer, ret, base);

		// TODO: Let disassemble functions stream in data
		// There could be a JMP outside the current chunk that won't be handled well
		disassemble_rd(buffer, ret, base);
		base += ret;
	}
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Usage: eo8 <rom>\n");
		return EXIT_FAILURE;
	}

	parse_rom(argv[1]);

	return EXIT_SUCCESS;
}
