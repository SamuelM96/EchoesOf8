#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include "disassembler.h"
#include <stdint.h>
#include <stdio.h>

Chip8Instruction *assemble(char *source_filename);

#endif // !ASSEMBLER_H
