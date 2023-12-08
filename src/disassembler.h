#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include <stdlib.h>

void hexdump(void *buffer, size_t length, size_t base);
void print_instruction(uint16_t instruction);
void disassemble_rd(uint8_t *code, size_t length, size_t base);
void disassemble_linear(uint8_t *code, size_t length, size_t base);

#endif // !DISASSEMBLER_H
