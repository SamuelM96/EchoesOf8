#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include "instructions.h"

#include <stdint.h>
#include <stdio.h>

Chip8Instruction *assemble(char *source_filename);

#endif // !ASSEMBLER_H
