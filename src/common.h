#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define PROG_BASE 0x200

// Instruction size = 2 bytes (big endian)
#define INST(code, addr) (((code)[(addr)] << 8) | (code)[(addr) + 1])

typedef struct {
	size_t key;
} set_val;

#endif // !COMMON_H
