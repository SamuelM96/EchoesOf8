#include "disassembler.h"
#include "instructions.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "common.h"

#include "stb_ds.h"

// Disassembly struct:
// Instruction
// Address

char *hexdump(void *buffer, size_t length, size_t base) {
	const char header[] = "Offset    0 1  2 3  4 5  6 7  8 9  A B  C D  E F";
	const int header_len = sizeof(header);

	// 00000000: 0000 0000 0000 0000 0000 0000 0000 0000  ................\n
	const int line_len = 68;
	size_t lines = length / 16;
	size_t whitespace_needed = 16 - (length % 16);
	lines = whitespace_needed == 16 ? lines : lines + 1;

	size_t result_len = lines * 68 + header_len; // null byte included from header
	char *result = malloc(result_len);

	memcpy(result, header, header_len);
	char *result_ptr = result + header_len - 1;
	char ascii[17] = { 0 };
	for (size_t i = 0; i < length; ++i) {
		char byte = ((char *)buffer)[i];
		if (isprint(byte)) {
			ascii[i % 16] = byte;
		} else {
			ascii[i % 16] = '.';
		}

		if (i % 16 == 0) {
			snprintf(result_ptr, 12, "\n%08zx: ", base + i);
			result_ptr += 11;
		}

		snprintf(result_ptr, 3, "%02hhx: ", byte);
		result_ptr += 2;

		if ((i + 1) % 16 == 0) {
			snprintf(result_ptr, sizeof(ascii) + 2, " %s", ascii);
			result_ptr += sizeof(ascii);
		} else if ((i + 1) % 2 == 0) {
			*result_ptr++ = ' ';
		}
	}

	if (whitespace_needed != 16) {
		for (size_t i = 0; i < whitespace_needed; ++i) {
			*result_ptr++ = ' ';
			*result_ptr++ = ' ';
			if ((i + 1) % 2 == 0) {
				*result_ptr++ = ' ';
			}
			ascii[15 - i] = ' ';
		}
		snprintf(result_ptr, sizeof(ascii) + 2, " %s\n", ascii);
		result_ptr += sizeof(ascii) + 1;
	}

	result[result_len - 1] = '\0';
	return result;
}

// Recursive descent disassembler
void disassemble_rd(uint8_t *code, size_t length, size_t base) {
	// TODO: Write to a buffer or return a heap string rather than printing
	// directly to stdout
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
			printf("\n");

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
	// TODO: Write to a buffer or return a heap string rather than printing
	// directly to stdout
	if (length % 2 != 0) {
		fprintf(stderr, "Cannot disassemble code: not aligned to 2 bytes");
		return;
	}

	for (size_t ip = 0; ip < length; ip += 2) {
		Chip8Instruction instruction = bytes2inst(code + ip);
		printf("0x%08zx  %04hx    ", base + ip, instruction.raw);
		print_asm(instruction);
		printf("\n");
	}
}
