#ifndef EMULATOR_H
#define EMULATOR_H

#include "instructions.h"

#include <stdlib.h>

void emulate(uint8_t *rom, size_t rom_size);

#endif // !EMULATOR_H
