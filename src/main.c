#include "assembler.h"
#include "common.h"
#include "disassembler.h"
#include "emulator.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_usage() {
	printf("Usage: eo8 <command> <rom>\n\n");
	printf("    <rom>                         Compiled CHIP-8 binary ROM\n\n");
	printf("Commands:\n");
	printf("    hexdump <rom>                 Outputs a hexdump of the ROM\n");
	printf("    disassemble <method> <rom>    Disassembles the ROM using the "
	       "selected method\n");
	printf("                                    Methods:\n");
	printf("                                      - linear       Linear sweep\n");
	printf("                                      - recursive    Recursive "
	       "descent\n");
	printf("    decompile <rom>               Decompiles the ROM into readable "
	       "source code\n");
	printf("    assessmble <asm> <rom>        Assessmbles the given assembly "
	       "code into a CHIP-8 ROM\n");
	printf("    compile <source> <rom>        Compiles the given source code "
	       "into a CHIP-8 ROM\n");
	printf("    emulate <rom> [--debug]       Emulates the ROM\n");
	printf("                                    --debug    Enables debug mode\n");
}

int main(int argc, char *argv[]) {
	size_t buffer_size;
	uint8_t *buffer = NULL;

	if (argc < 2) {
		print_usage();
		return EXIT_FAILURE;
	} else if (strcmp(argv[1], "hexdump") == 0) {
		if (argc != 3) {
			print_usage();
			return EXIT_FAILURE;
		}
		buffer = read_rom(argv[2], &buffer_size);
		char *dump = hexdump(buffer, buffer_size, 0);
		printf("%s", dump);
		free(dump);
		free(buffer);
	} else if (strcmp(argv[1], "disassemble") == 0) {
		if (argc != 4) {
			print_usage();
			return EXIT_FAILURE;
		}
		if (strcmp(argv[2], "linear") == 0) {
			buffer = read_rom(argv[3], &buffer_size);
			Disassembly disassembly =
				disassemble_linear(buffer, buffer_size, PROG_BASE);
			char *disasm_str = disassembly2str(&disassembly);
			printf("%s\n", disasm_str);
			free(disasm_str);
			free_disassembly(&disassembly);
			free(buffer);
		} else if (strcmp(argv[2], "recursive") == 0) {
			buffer = read_rom(argv[3], &buffer_size);
			Disassembly disassembly = disassemble_rd(buffer, buffer_size, PROG_BASE);
			char *disasm_str = disassembly2str(&disassembly);
			printf("%s\n", disasm_str);
			free(disasm_str);
			free_disassembly(&disassembly);
			free(buffer);
		}
	} else if (strcmp(argv[1], "decompile") == 0) {
		if (argc != 3) {
			print_usage();
			return EXIT_FAILURE;
		}
		// TODO: Implement decompiler
		printf("!!! NOT IMPLEMENTED !!!\n");
		return EXIT_FAILURE;
	} else if (strcmp(argv[1], "assemble") == 0) {
		if (argc != 4) {
			print_usage();
			return EXIT_FAILURE;
		}

		Chip8Instruction *instructions = assemble(argv[2]);
		FILE *output = fopen(argv[3], "w");
		for (int i = 0; i < arrlen(instructions); ++i) {
			Chip8Instruction instruction = instructions[i];
			uint8_t nibbles[2];
			nibbles[0] = (instruction.raw & 0xff00) >> 8;
			nibbles[1] = instruction.raw & 0xff;
			fwrite((void *)nibbles, sizeof(nibbles[0]), sizeof(nibbles), output);
		}

		fclose(output);
		arrfree(instructions);
	} else if (strcmp(argv[1], "compile") == 0) {
		if (argc != 4) {
			print_usage();
			return EXIT_FAILURE;
		}
		// TODO: Implement compiler
		printf("!!! NOT IMPLEMENTED !!!\n");
		return EXIT_FAILURE;
	} else if (strcmp(argv[1], "emulate") == 0) {
		if (argc != 3 && argc != 4) {
			print_usage();
			return EXIT_FAILURE;
		}

		int rom_idx = 2;
		bool debug = false;
		if (argc == 4) {
			if (strcmp("--debug", argv[2]) == 0) {
				debug = true;
				rom_idx = 3;
			} else if (strcmp("--debug", argv[3]) == 0) {
				debug = true;
			}
		}

		emulate_file(argv[rom_idx], debug);
	} else {
		print_usage();
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
