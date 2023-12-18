#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define PROG_BASE 0x200

typedef struct {
	size_t key;
} set_val;

static uint8_t *read_rom(char *rom_path, size_t *out_size) {
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

#endif // !COMMON_H
