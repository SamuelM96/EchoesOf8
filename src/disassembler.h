#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include <stdint.h>
#include <stdlib.h>

char *hexdump(void *buffer, size_t length, size_t base);
void disassemble_rd(uint8_t *code, size_t length, size_t base);
void disassemble_linear(uint8_t *code, size_t length, size_t base);

#endif // !DISASSEMBLER_H
