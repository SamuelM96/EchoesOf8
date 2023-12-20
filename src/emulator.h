#ifndef EMULATOR_H
#define EMULATOR_H

#include "common.h"

#include <SDL_scancode.h>

#include <stdbool.h>
#include <stdint.h>

void emulate(uint8_t *rom, size_t rom_size, bool debug, char *rom_path);
static inline void emulate_file(char *rom_path, bool debug) {
	size_t rom_size = 0;
	uint8_t *rom = read_rom(rom_path, &rom_size);
	emulate(rom, rom_size, debug, rom_path);
}

#endif // !EMULATOR_H
