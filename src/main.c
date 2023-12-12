#include "assembler.h"
#include "common.h"
#include "disassembler.h"

#include "../lib/stb_ds.h"
#include "emulator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint8_t *read_rom(char *rom_path, size_t *out_size) {
	FILE *rom = fopen(rom_path, "rb");

	if (rom == NULL) {
		fprintf(stderr, "Error opening ROM file\n");
		exit(EXIT_FAILURE);
	}

	fseek(rom, 0, SEEK_END);
	size_t file_size = ftell(rom);
	rewind(rom);

	file_size += file_size % 2; // align to 2 bytes

	uint8_t *buffer;
	buffer = malloc(file_size);
	fread(buffer, sizeof(*buffer), file_size, rom);

	fclose(rom);

	*out_size = file_size;
	return buffer;
}

void print_usage() {
	printf("Usage: eo8 <command> <rom>\n\n");
	printf("    <rom>                         Compiled CHIP-8 binary ROM\n\n");
	printf("Commands:\n");
	printf("    hexdump <rom>                 Outputs a hexdump of the ROM\n");
	printf("    disassemble <method> <rom>    Disassembles the ROM using the selected method\n");
	printf("                                    Methods:\n");
	printf("                                      - linear       Linear sweep\n");
	printf("                                      - recursive    Recursive descent\n");
	printf("    assessmble <asm> <rom>        Assessmbles the given assembly code into a CHIP-8 ROM\n");
	printf("    compile <source> <rom>        Compiles the given source code into a CHIP-8 ROM\n");
	printf("    emulate <rom>                 Emulates the ROM\n");
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
		hexdump(buffer, buffer_size, 0);
	} else if (strcmp(argv[1], "disassemble") == 0) {
		if (argc != 4) {
			print_usage();
			return EXIT_FAILURE;
		}
		if (strcmp(argv[2], "linear") == 0) {
			buffer = read_rom(argv[3], &buffer_size);
			disassemble_linear(buffer, buffer_size, PROG_BASE);
		} else if (strcmp(argv[2], "recursive") == 0) {
			buffer = read_rom(argv[3], &buffer_size);
			disassemble_rd(buffer, buffer_size, PROG_BASE);
		}
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
		printf("!!! NOT IMPLEMENTED !!!\n");
		return EXIT_FAILURE;
	} else if (strcmp(argv[1], "emulate") == 0) {
		if (argc != 3) {
			print_usage();
			return EXIT_FAILURE;
		}
		buffer = read_rom(argv[2], &buffer_size);
		emulate(buffer, buffer_size, true);
	} else {
		print_usage();
		return EXIT_FAILURE;
	}

	if (buffer) {
		free(buffer);
	}

	return EXIT_SUCCESS;
}
