#ifndef EMULATOR_H
#define EMULATOR_H

#include "instructions.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#include <SDL_scancode.h>

#define CONFIG_CHIP8_SHIFT 0b1
#define CONFIG_CHIP8_JMP0 0b10
#define CONFIG_CHIP8_LD_I 0b100
#define CONFIG_CHIP8_CLIPPING 0b1000
#define CONFIG_CHIP8_DISP_WAIT 0b10000

#define CONFIG_CHIP8                                                                         \
	CONFIG_CHIP8_SHIFT | CONFIG_CHIP8_JMP0 | CONFIG_CHIP8_LD_I | CONFIG_CHIP8_CLIPPING | \
		CONFIG_CHIP8_DISP_WAIT

#define NANOSECONDS_PER_SECOND 1000000000
#define TARGET_HZ 60
#define CYCLES_PER_FRAME 100

#define PIXEL_COLOUR 0xFF97F1CD
#define pixel(emulator, x, y) (emulator)->display[(y) * TARGET_WIDTH + (x)]
#define FONT_BASE_ADDR 0x050

#define TARGET_WIDTH 64
#define TARGET_HEIGHT 32
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 640

extern const int SCALE_X;
extern const int SCALE_Y;

typedef struct EmulatorState {
	// 0x000 - 0x1FF = Interpreter memory, not for programs
	// Programs start at 0x200 (512)
	// Some start at 0x600 (1536) (ETI 660 computer)
	uint8_t memory[4096];

	// Stores return addresses
	// Allows for 16 levels of nested subroutines
	uint16_t stack[16];

	// 0-F general purpose registers
	uint8_t registers[16];

	// Stack pointer
	uint8_t sp;

	// For memory addresses, lower 12bits used
	uint16_t vi;

	// Program counter
	uint16_t pc;

	// Delay timer
	uint8_t dt;

	// Sound timer
	uint8_t st;

	// Display pixels
	uint32_t display[TARGET_WIDTH * TARGET_HEIGHT];

	// Keyboard state, 1 = Pressed
	uint8_t keyboard[16];

	// CHIP-8 vs SUPER-CHIP/CHIP-48 differences
	uint8_t configuration;

	bool display_interrupted;
} EmulatorState;

void emulate(uint8_t *rom, size_t rom_size, bool debug);

void handle_timers(EmulatorState *);
Chip8Instruction fetch_next(EmulatorState *, bool);
void dump_registers(EmulatorState *);
void dump_stack(EmulatorState *);
static inline void dump_memory(EmulatorState *);
void dump_state(EmulatorState *);
void reset_state(EmulatorState *);
void init_graphics();
void update_keyboard_state(EmulatorState *, SDL_Scancode, uint8_t);
bool handle_input(EmulatorState *);
void render(uint32_t *);
void cleanup();
void print_instruction_state(EmulatorState *, Chip8Instruction);
bool process_instruction(EmulatorState *, Chip8Instruction);

#endif // !EMULATOR_H
