#include "emulator.h"
#include "instructions.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

// 0x000 - 0x1FF = Interpreter memory, not for programs
// Programs start at 0x200 (512)
// Some start at 0x600 (1536) (ETI 660 computer)
uint8_t EMULATOR_MEMORY[4096];

// Stores return addresses
// Allows for 16 levels of nested subroutines
uint16_t EMULATOR_STACK[16];

// Stack pointer
uint8_t EMULATOR_SP;

// 0-F general purpose registers
uint8_t EMULATOR_REGISTERS[16];

// For memory addresses, lower 12bits used
uint16_t EMULATOR_VI;

// Program counter
uint16_t EMULATOR_PC;

// Delay timer
uint8_t EMULATOR_DT;

// Sound timer
uint8_t EMULATOR_ST;

void reset_state() {
	memset(EMULATOR_MEMORY, 0, sizeof(EMULATOR_MEMORY));
	memset(EMULATOR_STACK, 0, sizeof(EMULATOR_STACK));
	memset(EMULATOR_REGISTERS, 0, sizeof(EMULATOR_REGISTERS));

	EMULATOR_SP = 0;
	EMULATOR_VI = 0;
	EMULATOR_PC = 0;
	EMULATOR_DT = 0;
	EMULATOR_ST = 0;
}

void emulate(uint8_t *rom, size_t rom_size) {
	printf("Emulating!\n");
	reset_state();
}
