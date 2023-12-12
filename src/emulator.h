#ifndef EMULATOR_H
#define EMULATOR_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

void emulate(uint8_t *rom, size_t rom_size, bool debug);

#endif // !EMULATOR_H
