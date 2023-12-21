#include "disassembler.h"
#include "instructions.h"

#include "sds.h"
#include "stb_ds.h"

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void _disassemble_rd(Disassembly *disassembly, uint8_t *code, size_t length, size_t offset);

sds hexdump(void *buffer, size_t length, size_t base) {
	sds result = sdsnew("Offset    0 1  2 3  4 5  6 7  8 9  A B  C D  E F");

	size_t lines = length / 16;
	size_t whitespace_needed = 16 - (length % 16);
	lines = whitespace_needed == 16 ? lines : lines + 1;

	char ascii[17] = { 0 };
	for (size_t i = 0; i < length; ++i) {
		char byte = ((char *)buffer)[i];
		if (isprint(byte)) {
			ascii[i % 16] = byte;
		} else {
			ascii[i % 16] = '.';
		}

		if (i % 16 == 0) {
			result = sdscatprintf(result, "\n%08zx: ", base + i);
		}

		result = sdscatprintf(result, "%02hhx", byte);

		if ((i + 1) % 16 == 0) {
			result = sdscatprintf(result, "  %s", ascii);
		} else if ((i + 1) % 2 == 0) {
			result = sdscat(result, " ");
		}
	}

	if (whitespace_needed != 16) {
		for (size_t i = 0; i < whitespace_needed; ++i) {
			result = sdscat(result, "  ");
			if ((i + 1) % 2 == 0) {
				result = sdscat(result, " ");
			}
			ascii[15 - i] = ' ';
		}
		result = sdscatprintf(result, " %s\n", ascii);
	}

	return result;
}

// Recursive descent disassembler
Disassembly disassemble_rd(uint8_t *code, size_t length, size_t base, size_t offset) {
	Disassembly disassembly = { 0 };

	disassembly.base = base;
	disassembly.addressbook = malloc(sizeof(AddressLookup) * length);

	_disassemble_rd(&disassembly, code, length, offset);

	return disassembly;
}

void disassemble_rd_update(Disassembly *disassembly, uint8_t *code, size_t length, size_t offset) {
	// If data blocks exists, we're updating an existing disassembly.
	// During this disassembly, bytes that were previously designated
	// as data may now become instructions. Need to reevaluate data
	// blocks as a result.
	if (disassembly->data_blocks) {
		for (size_t i = 0; i < disassembly->dblock_length; ++i) {
			disassembly->abook_length -= disassembly->data_blocks[i].length;
		}
		arrfree(disassembly->data_blocks);
		disassembly->dblock_length = 0;
	} else {
		// There's no data blocks to check for new instructions.
		// For a full refresh, better to just request an entire new disassembly
		return;
	}

	_disassemble_rd(disassembly, code, length, offset);
}

void _disassemble_rd(Disassembly *disassembly, uint8_t *code, size_t length, size_t offset) {
	size_t base = disassembly->base;
	size_t *queue = NULL;
	arrput(queue, offset); // Known entry point

	size_t new_instructions = 0;

	// First pass to discover instructions using standard recursive descent
	while (arrlen(queue)) {
		size_t ip = queue[0];
		arrdel(queue, 0);

		InstructionBlock block = { 0 };

		while ((ip + 1) < length && disassembly->addressbook[ip].type != ADDR_INSTRUCTION) {
			Chip8Instruction instruction = bytes2inst(code + ip);
			DisassembledInstruction disasm = {
				.instruction = instruction,
				.asm_str = inst2str(instruction),
				.address = ip,
			};

			AddressLookup *lookup = &disassembly->addressbook[disasm.address];
			if (lookup->type != ADDR_INSTRUCTION && lookup->type != ADDR_INST_HALF) {
				disassembly->abook_length++;
			}
			lookup->block_offset = disassembly->iblock_length;
			lookup->array_offset = block.length;
			lookup->type = ADDR_INSTRUCTION;
			new_instructions++;

			// Account for second half of instruction
			lookup++;
			if (lookup->type != ADDR_INSTRUCTION && lookup->type != ADDR_INST_HALF) {
				disassembly->abook_length++;
			}
			lookup->block_offset = disassembly->iblock_length;
			lookup->array_offset = block.length++;
			lookup->type = ADDR_INST_HALF;

			arrput(block.instructions, disasm);

			bool should_break = false;
			Chip8InstructionType inst_type = instruction_type(instruction);
			switch (inst_type) {
			case CHIP8_RET:
				should_break = true;
				break;
			case CHIP8_JMP_ADDR:
			case CHIP8_CALL_ADDR: {
				uint16_t addr = instruction.aformat.addr - base;
				AddressType type = disassembly->addressbook[addr].type;
				if (type == ADDR_UNKNOWN || type == ADDR_DATA) {
					disassembly->addressbook[addr].type = ADDR_MARKED;
					arrput(queue, addr);
				}
				if (inst_type == CHIP8_JMP_ADDR) {
					should_break = true;
				}
				break;
			}
			case CHIP8_SE_VX_BYTE:
			case CHIP8_SNE_VX_BYTE:
			case CHIP8_SE_VX_VY:
			case CHIP8_SNE_VX_VY:
			case CHIP8_SKP_VX:
			case CHIP8_SKNP_VX: {
				// Add address that would be skipped to if condition is true.
				// This catches JMP statements that may halt disassembly.
				uint16_t addr = ip + 4;
				AddressType type = disassembly->addressbook[addr].type;
				if (type == ADDR_UNKNOWN || type == ADDR_DATA) {
					disassembly->addressbook[addr].type = ADDR_MARKED;
					arrput(queue, addr);
				}
			} break;
			case CHIP8_JMP_V0_ADDR:
				// Unconditional jump that requires runtime info
				// - can't disassemble unknown address
				should_break = true;
				break;
			default:
				break;
			}

			if (should_break) {
				break;
			}

			ip += 2;
		}

		if (block.length) {
			arrput(disassembly->instruction_blocks, block);
			disassembly->iblock_length++;
		}
	}

	arrfree(queue);

	// Second pass to discover data blocks based on unparsed sections
	size_t data_start = -1;
	size_t data_len = 0;
	for (size_t ip = 0; ip < length; ++ip) {
		AddressType type = disassembly->addressbook[ip].type;
		assert(type != ADDR_MARKED);
		bool processed = type == ADDR_INSTRUCTION || type == ADDR_INST_HALF;
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
				AddressLookup *lookup = &disassembly->addressbook[data_start + i];
				lookup->block_offset = disassembly->dblock_length,
				lookup->array_offset = i;
				lookup->type = ADDR_DATA;
				disassembly->abook_length++;
			}

			data_start = -1;
			data_len = 0;

			arrput(disassembly->data_blocks, block);
			disassembly->dblock_length++;
		}
	}

	// printf("Length: %zu\nAddressbook: %zu\n", length, disassembly->abook_length);
	// printf("Discovered %zu instructions!\n", new_instructions);
	assert(disassembly->abook_length == length);
}

// Linear sweep disassembler
Disassembly disassemble_linear(uint8_t *code, size_t length, size_t base) {
	Disassembly disassembly = { 0 };
	disassembly.base = base;

	disassembly.addressbook = malloc(sizeof(AddressLookup) * length);

	InstructionBlock block = { 0 };

	for (size_t ip = 0; (ip + 1) < length; ip += 2) {
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

sds disassembly2str(Disassembly *disassembly) {
	uint16_t base = disassembly->base;
	sds buffer = sdsempty();

	for (int j = 0; j < disassembly->iblock_length; ++j) {
		InstructionBlock *block = &disassembly->instruction_blocks[j];
		buffer = sdscatprintf(buffer, "===== BLOCK @ 0x%08hx =====\n",
				      block->instructions[0].address + base);

		for (int i = 0; i < block->length; ++i) {
			DisassembledInstruction *disasm = &block->instructions[i];
			buffer = sdscatprintf(buffer, "0x%08hx  %04hx    %s\n",
					      disasm->address + base, disasm->instruction.raw,
					      disasm->asm_str);
		}
		buffer = sdscat(buffer, "\n");
	}

	for (int i = 0; i < disassembly->dblock_length; ++i) {
		DataBlock *block = &disassembly->data_blocks[i];
		buffer =
			sdscatprintf(buffer, "===== DATA @ 0x%08hx =====\n", block->address + base);
		sds dump = hexdump(block->data, block->length, block->address + base);
		buffer = sdscatfmt(buffer, "%s\n\n", dump);
		sdsfree(dump);
	}

	return buffer;
}

void free_disassembly(Disassembly *disassembly) {
	if (disassembly->instruction_blocks) {
		for (int i = 0; i < disassembly->iblock_length; ++i) {
			InstructionBlock *block = &disassembly->instruction_blocks[i];
			for (int j = 0; j < block->length; ++j) {
				sdsfree(block->instructions[j].asm_str);
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
