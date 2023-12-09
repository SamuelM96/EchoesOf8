#include "disassembler.h"

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
		printf("%02hhx", byte);
		if ((i + 1) % 16 == 0) {
			printf("  %s\n", ascii);
			if (i + 1 != length) {
				printf("%08zx: ", base + i + 1);
			}
		} else if ((i + 1) % 2 == 0) {
			printf(" ");
		}
		if (!isprint(byte)) {
			byte = '.';
		}
		ascii[i % 16] = byte;
	}

	size_t whitespace_needed = 16 - (length % 16);
	if (whitespace_needed != 16) {
		for (size_t i = 0; i < whitespace_needed; ++i) {
			printf("  ");
			if ((i + 1) % 2 == 0) {
				printf(" ");
			}
		}
		printf(" %s\n", ascii);
	}
}

void print_instruction(Chip8Instruction instruction, Chip8InstructionFormat format) {
	switch (format) {
	case R_FORMAT:
		printf("Opcode: %01hx\n", instruction.rformat.opcode);
		printf("Rx: %01hx\n", instruction.rformat.rx);
		printf("Ry: %01hx\n", instruction.rformat.ry);
		printf("Imm: %01hx\n", instruction.rformat.imm);
		break;
	case I_FORMAT:
		printf("Opcode: %01hx\n", instruction.iformat.opcode);
		printf("Reg: %01hx\n", instruction.iformat.reg);
		printf("Imm: %02hx\n", instruction.iformat.imm);
		break;
	case A_FORMAT:
		printf("Opcode: %01hx\n", instruction.aformat.opcode);
		printf("Addr: %03hx\n", instruction.aformat.addr);
		break;
	default:
		printf("Unknown instruction format: %d\n", format);
		break;
	}
}

void print_asm(Chip8Instruction instruction) {
	switch (instruction.aformat.opcode) {
	case 0:
		switch (instruction.aformat.addr) {
		case 0x0E0:
			printf("CLS");
			break;
		case 0x0EE:
			printf("RET");
			break;
		default:
			printf("SYS %#03x", instruction.aformat.addr);
			break;
		}
		break;
	case 1:
		printf("JMP %#03x", instruction.aformat.addr);
		break;
	case 2:
		printf("CALL %#03x", instruction.aformat.addr);
		break;
	case 3:
		printf("SE V%d, %#02x", instruction.iformat.reg, instruction.iformat.imm);
		break;
	case 4:
		printf("SNE V%d, %#02x", instruction.iformat.reg, instruction.iformat.imm);
		break;
	case 5:
		printf("SE V%d, V%d", instruction.rformat.rx, instruction.rformat.ry);
		break;
	case 6:
		printf("LD V%d, %#02x", instruction.iformat.reg, instruction.iformat.imm);
		break;
	case 7:
		printf("ADD V%d, %#02x", instruction.iformat.reg, instruction.iformat.imm);
		break;
	case 8:
		switch (instruction.rformat.imm) {
		case 0:
			printf("LD V%d, V%d", instruction.rformat.rx, instruction.rformat.ry);
			break;
		case 1:
			printf("OR V%d, V%d", instruction.rformat.rx, instruction.rformat.ry);
			break;
		case 2:
			printf("AND V%d, V%d", instruction.rformat.rx, instruction.rformat.ry);
			break;
		case 3:
			printf("XOR V%d, V%d", instruction.rformat.rx, instruction.rformat.ry);
			break;
		case 4:
			printf("ADD V%d, V%d", instruction.rformat.rx, instruction.rformat.ry);
			break;
		case 5:
			printf("SUB V%d, V%d", instruction.rformat.rx, instruction.rformat.ry);
			break;
		case 6:
			printf("SHR V%d, V%d", instruction.rformat.rx, instruction.rformat.ry);
			break;
		case 7:
			printf("SUBN V%d, V%d", instruction.rformat.rx, instruction.rformat.ry);
			break;
		case 0xE:
			printf("SHL V%d, V%d", instruction.rformat.rx, instruction.rformat.ry);
			break;
		}
		break;
	case 9:
		printf("SNE V%d, V%d", instruction.rformat.rx, instruction.rformat.ry);
		break;
	case 0xA:
		printf("LD I, %#03x", instruction.aformat.addr);
		break;
	case 0xB:
		printf("JMP V0, %#03x", instruction.aformat.addr);
		break;
	case 0xC:
		printf("RND V%d, %#02x", instruction.iformat.reg, instruction.iformat.imm);
		break;
	case 0xD:
		printf("DRW V%d, V%d, %d", instruction.rformat.rx, instruction.rformat.ry,
		       instruction.rformat.imm);
		break;
	case 0xE:
		switch (instruction.iformat.imm) {
		case 0x9E:
			printf("SKP V%d", instruction.iformat.reg);
			break;
		case 0xA1:
			printf("SKNP V%d", instruction.iformat.reg);
			break;
		default:
			printf("unknown");
			break;
		}
		break;
	case 0xF:
		switch (instruction.iformat.imm) {
		case 0x07:
			printf("LD V%d", instruction.iformat.reg);
			break;
		case 0x0A:
			printf("LD V%d", instruction.iformat.reg);
			break;
		case 0x15:
			printf("LD DT, V%d", instruction.iformat.reg);
			break;
		case 0x18:
			printf("LD ST, V%d", instruction.iformat.reg);
			break;
		case 0x1E:
			printf("ADD I, V%d", instruction.iformat.reg);
			break;
		case 0x29:
			printf("LD F, V%d", instruction.iformat.reg);
			break;
		case 0x33:
			printf("LD B, V%d", instruction.iformat.reg);
			break;
		case 0x55:
			printf("LD [I], V%d", instruction.iformat.reg);
			break;
		case 0x65:
			printf("LD V%d, [I]", instruction.iformat.reg);
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
	set_val *processed = NULL;
	arrput(queue, 0); // Entry point

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
