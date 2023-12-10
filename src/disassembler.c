#include "disassembler.h"
#include "instructions.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "common.h"

#define STB_DS_IMPLEMENTATION
#include "../lib/stb_ds.h"

void hexdump(void *buffer, size_t length, size_t base) {
	char ascii[17] = { 0 };
	printf("%08zx: ", base);
	for (size_t i = 0; i < length; ++i) {
		char byte = ((char *)buffer)[i];
		if (isprint(byte)) {
			ascii[i % 16] = byte;
		} else {
			ascii[i % 16] = '.';
		}

		printf("%02hhx", byte);
		if ((i + 1) % 16 == 0) {
			printf("  %s\n", ascii);
			if (i + 1 != length) {
				printf("%08zx: ", base + i + 1);
			}
		} else if ((i + 1) % 2 == 0) {
			printf(" ");
		}
	}

	size_t whitespace_needed = 16 - (length % 16);
	if (whitespace_needed != 16) {
		for (size_t i = 0; i < whitespace_needed; ++i) {
			printf("  ");
			if ((i + 1) % 2 == 0) {
				printf(" ");
			}
			ascii[15 - i] = ' ';
		}
		printf(" %s\n", ascii);
	}
}

// Recursive descent disassembler
void disassemble_rd(uint8_t *code, size_t length, size_t base) {
	if (length % 2 != 0) {
		fprintf(stderr, "Cannot disassemble code: not aligned to 2 bytes");
		return;
	}

	size_t *queue = NULL;
	set_val *processed = NULL;
	arrput(queue, 0); // Entry point

	// First pass to discover instructions using standard recursive descent
	while (arrlen(queue)) {
		size_t ip = queue[0];
		arrdel(queue, 0);

		printf("===== BLOCK @ 0x%08zx =====\n", ip + PROG_BASE);

		while (ip < length) {
			hmputs(processed, (set_val){ ip });
			Chip8Instruction instruction = bytes2inst(code + ip);
			printf("0x%08zx  %04hx    ", base + ip, instruction.raw);

			print_asm(instruction);

			if (instruction.raw == 0x00EE) {
				break;
			}

			if (instruction.aformat.opcode == 1) { // JMP addr
				uint16_t addr = instruction.aformat.addr - PROG_BASE;
				if (!hmgets(processed, addr).key && addr != 0) {
					arrput(queue, addr);
				}
				break;
			} else if (instruction.aformat.opcode == 0xB) { // JMP V0, addr
				// Unconditional jump that requires runtime info
				// - can't disassemble unknown address
				break;
			} else if (instruction.aformat.opcode == 0x2) { // CALL addr
				uint16_t addr = instruction.aformat.addr - PROG_BASE;
				if (!hmgets(processed, addr).key && addr != 0) {
					arrput(queue, addr);
				}
			}

			ip += 2;
		}
		printf("\n");
	}

	arrfree(queue);

	// Second pass to discover data blocks based on unparsed sections
	size_t data_start = -1;
	for (size_t ip = 2; ip < length; ip += 2) {
		bool wasProcessed = hmgets(processed, ip).key;
		if (!wasProcessed && data_start == -1) {
			data_start = ip;
		} else if (wasProcessed && data_start != -1) {
			printf("==== DATA @ 0x%08zx =====\n", ip);
			size_t data_len = ip - data_start;
			hexdump(code + data_start, data_len, data_start);
			data_start = -1;
		}
	}

	hmfree(processed);
}

// Linear sweep disassembler
void disassemble_linear(uint8_t *code, size_t length, size_t base) {
	if (length % 2 != 0) {
		fprintf(stderr, "Cannot disassemble code: not aligned to 2 bytes");
		return;
	}

	for (size_t ip = 0; ip < length; ip += 2) {
		Chip8Instruction instruction = bytes2inst(code + ip);
		printf("0x%08zx  %04hx    ", base + ip, instruction.raw);
		print_asm(instruction);
	}
}
