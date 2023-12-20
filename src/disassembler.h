#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include "instructions.h"
#include "sds.h"

#include <stdint.h>
#include <stdlib.h>

typedef struct DataBlock {
	uint8_t *data;
	size_t length;
	uint16_t address;
} DataBlock;

typedef struct DisassembledInstruction {
	sds asm_str;
	Chip8Instruction instruction;
	uint16_t address;
} DisassembledInstruction;

typedef struct InstructionBlock {
	DisassembledInstruction *instructions;
	size_t length;
} InstructionBlock;

typedef enum AddressType {
	ADDR_UNKNOWN = 0,
	ADDR_MARKED, // Marked for processing - like unknown, but acknowledged
	ADDR_DATA,
	ADDR_INSTRUCTION,
	ADDR_INST_HALF, // 2nd instruction byte - invalid by itself
} AddressType;

typedef struct AddressLookup {
	size_t block_offset;
	size_t array_offset;
	AddressType type;
} AddressLookup;

typedef struct Disassembly {
	AddressLookup *addressbook;
	size_t abook_length;
	InstructionBlock *instruction_blocks;
	size_t iblock_length;
	DataBlock *data_blocks;
	size_t dblock_length;
	uint16_t base;

} Disassembly;

sds hexdump(void *buffer, size_t length, size_t base);
Disassembly disassemble_rd(uint8_t *code, size_t length, size_t base);
Disassembly disassemble_linear(uint8_t *code, size_t length, size_t base);

sds disassembly2str(Disassembly *disassembly);
void free_disassembly(Disassembly *disassembly);

#endif // !DISASSEMBLER_H
