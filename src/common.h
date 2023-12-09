#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define PROG_BASE 0x200

typedef struct {
	size_t key;
} set_val;

#endif // !COMMON_H
