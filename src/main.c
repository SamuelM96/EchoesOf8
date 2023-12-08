#include "common.h"
#include "disassembler.h"

#include <stdio.h>

void parse_rom(char *rom_path) {
	FILE *rom = fopen(rom_path, "rb");

	if (rom == NULL) {
		fprintf(stderr, "Error opening ROM file");
		exit(EXIT_FAILURE);
	}

	fseek(rom, 0, SEEK_END);
	size_t file_size = ftell(rom);
	rewind(rom);

	uint8_t *buffer;
	buffer = malloc(file_size);

	printf("ROM dump\n");
	size_t ret, base = PROG_BASE;
	while ((ret = fread(buffer, sizeof(*buffer), file_size, rom))) {
		// hexdump(buffer, ret, base);
		// disassemble_linear(buffer, ret, base);

		// TODO: Let disassemble functions stream in data
		// There could be a JMP outside the current chunk that won't be handled well
		disassemble_rd(buffer, ret, base);
		base += ret;
	}

	fclose(rom);
	free(buffer);
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Usage: eo8 <rom>\n");
		return EXIT_FAILURE;
	}

	parse_rom(argv[1]);

	return EXIT_SUCCESS;
}
