#include "disassembler.h"
#include "instructions.h"

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#include "stb_ds.h"

// TODO: Use a nice dynamic string builder implementation rather than this
// handcrafted mess
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
			snprintf(result_ptr, sizeof(ascii) + 3, "  %s", ascii);
			result_ptr += sizeof(ascii) + 1;
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
		snprintf(result_ptr, sizeof(ascii) + 3, "  %s\n", ascii);
		result_ptr += sizeof(ascii) + 1;
	}

	result[result_len - 1] = '\0';
	return result;
}

// Recursive descent disassembler
Disassembly disassemble_rd(uint8_t *code, size_t length, size_t base) {
	Disassembly disassembly = { 0 };
	disassembly.base = base;

	if (length % 2 != 0) {
		fprintf(stderr, "Cannot disassemble code: not aligned to 2 bytes");
		return disassembly;
	}

	size_t *queue = NULL;
	arrput(queue, 0); // Entry point

	disassembly.addressbook = malloc(sizeof(AddressLookup) * length);
	memset(disassembly.addressbook, 0, sizeof(AddressLookup) * length);

	// First pass to discover instructions using standard recursive descent
	while (arrlen(queue)) {
		size_t ip = queue[0];
		arrdel(queue, 0);

		InstructionBlock block = { 0 };

		while (ip < length) {
			Chip8Instruction instruction = bytes2inst(code + ip);
			DisassembledInstruction disasm = {
				.instruction = instruction,
				.asm_str = inst2str(instruction),
				.address = ip,
			};

			AddressLookup *lookup = &disassembly.addressbook[disasm.address];
			lookup->block_offset = disassembly.iblock_length;
			lookup->array_offset = block.length;
			lookup->type = ADDR_INSTRUCTION;
			disassembly.abook_length++;

			// Account for second half of instruction
			lookup++;
			lookup->block_offset = disassembly.iblock_length;
			lookup->array_offset = block.length++;
			lookup->type = ADDR_INST_HALF;
			disassembly.abook_length++;

			arrput(block.instructions, disasm);

			// Instructions that effect control flow
			// "Skipping" instructions can be parsed linearly due to their
			// predictable behaviour.
			if (instruction.raw == 0x00EE) {
				break;
			} else if (instruction.aformat.opcode == 0x1 // JMP addr
				   || instruction.aformat.opcode == 0x2) { // CALL addr
				uint16_t addr = instruction.aformat.addr - PROG_BASE;
				if (disassembly.addressbook[addr].type == ADDR_UNKNOWN) {
					arrput(queue, addr);
					disassembly.addressbook[addr].type = ADDR_MARKED;
				}
				if (instruction.aformat.opcode == 0x1) {
					break;
				}
			} else if (instruction.aformat.opcode == 0xB) { // JMP V0, addr
				// Unconditional jump that requires runtime info
				// - can't disassemble unknown address
				break;
			}

			ip += 2;
		}

		arrput(disassembly.instruction_blocks, block);
		disassembly.iblock_length++;
	}

	arrfree(queue);

	// Second pass to discover data blocks based on unparsed sections
	size_t data_start = -1;
	size_t data_len = 0;
	for (size_t ip = 0; ip < length; ++ip) {
		bool processed = disassembly.addressbook[ip].type != ADDR_UNKNOWN;
		if (!processed) {
			if (data_start == -1) {
				data_start = ip;
			}
			data_len++;
		}

		if ((ip + 1 == length || processed) && data_len > 0) {
			DataBlock block = {
				.data = code + data_start,
				.length = data_len,
				.address = data_start,
			};

			for (size_t i = 0; i < data_len; ++i) {
				AddressLookup *lookup = &disassembly.addressbook[data_start + i];
				lookup->block_offset = disassembly.dblock_length,
				lookup->array_offset = i;
				lookup->type = ADDR_DATA;
				disassembly.abook_length++;
			}

			data_start = -1;
			data_len = 0;

			arrput(disassembly.data_blocks, block);
			disassembly.dblock_length++;
		}
	}

	assert(disassembly.abook_length == length);

	return disassembly;
}

// Linear sweep disassembler
Disassembly disassemble_linear(uint8_t *code, size_t length, size_t base) {
	Disassembly disassembly = { 0 };
	disassembly.base = base;

	if (length % 2 != 0 || length == 0) {
		return disassembly;
	}

	disassembly.addressbook = malloc(sizeof(AddressLookup) * length);

	InstructionBlock block = { 0 };

	for (size_t ip = 0; ip < length; ip += 2) {
		Chip8Instruction instruction = bytes2inst(code + ip);
		DisassembledInstruction disasm = {
			.asm_str = inst2str(instruction),
			.instruction = instruction,
			.address = ip,
		};

		AddressLookup lookup = { .block_offset = disassembly.iblock_length,
					 .array_offset = block.length,
					 .type = ADDR_INSTRUCTION };
		disassembly.addressbook[disasm.address] = lookup;
		disassembly.abook_length++;

		arrput(block.instructions, disasm);
		block.length++;
	}

	arrput(disassembly.instruction_blocks, block);
	disassembly.iblock_length = 1;

	return disassembly;
}

char *disassembly2str(Disassembly *disassembly) {
	uint16_t base = disassembly->base;
	size_t buffer_len = 1024;
	size_t remainder = buffer_len;
	char *buffer = malloc(buffer_len);
	char *ptr = buffer;

#define WRITE(str, ...) \
	do { \
		if (remainder < 64) { \
			size_t offset = ptr - buffer; \
			size_t prev_len = buffer_len - remainder; \
			buffer_len *= 1.5; \
			buffer = realloc(buffer, buffer_len); \
			ptr = buffer + offset; \
			remainder = buffer_len - prev_len; \
		} \
		size_t bytes_written = snprintf(ptr, remainder, (str), ##__VA_ARGS__); \
		remainder -= bytes_written; \
		ptr += bytes_written; \
	} while (false)

	for (int j = 0; j < disassembly->iblock_length; ++j) {
		InstructionBlock *block = &disassembly->instruction_blocks[j];
		WRITE("===== BLOCK @ 0x%08hx =====\n", block->instructions[0].address + base);

		for (int i = 0; i < block->length; ++i) {
			DisassembledInstruction *disasm = &block->instructions[i];
			WRITE("0x%08hx  %04hx    %s\n", disasm->address + base,
			      disasm->instruction.raw, disasm->asm_str);
		}
		WRITE("\n");
	}

	for (int i = 0; i < disassembly->dblock_length; ++i) {
		DataBlock *block = &disassembly->data_blocks[i];
		WRITE("===== DATA @ 0x%08hx =====\n", block->address + base);
		char *dump = hexdump(block->data, block->length, block->address + base);
		WRITE("%s\n\n", dump);
		free(dump);
	}

#undef WRITE

	return realloc(buffer, ptr - buffer);
}

void free_disassembly(Disassembly *disassembly) {
	if (disassembly->instruction_blocks) {
		for (int i = 0; i < disassembly->iblock_length; ++i) {
			InstructionBlock *block = &disassembly->instruction_blocks[i];
			for (int j = 0; j < block->length; ++j) {
				free(block->instructions[j].asm_str);
			}
		}
		arrfree(disassembly->instruction_blocks);
	}

	if (disassembly->data_blocks) {
		arrfree(disassembly->data_blocks);
	}

	if (disassembly->addressbook) {
		free(disassembly->addressbook);
	}
}
