#include "emulator.h"
#include "common.h"
#include "disassembler.h"
#include "instructions.h"

#include <SDL.h>
#include <SDL_events.h>
#include <SDL_keyboard.h>
#include <SDL_pixels.h>
#include <SDL_render.h>
#include <SDL_scancode.h>
#include <SDL_surface.h>
#include <SDL_video.h>

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TARGET_HZ 60
const clock_t EMULATOR_INTERVAL = CLOCKS_PER_SEC / TARGET_HZ;
clock_t g_last_time;
clock_t g_current_time;

#define TARGET_WIDTH 64
#define TARGET_HEIGHT 32
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 640
const int SCALE_X = SCREEN_WIDTH / TARGET_WIDTH;
const int SCALE_Y = SCREEN_HEIGHT / TARGET_HEIGHT;

const uint8_t EMULATOR_FONTS[80] = {
	// https://tobiasvl.github.io/blog/write-a-chip-8-emulator/#fx29-font-character
	0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
	0x20, 0x60, 0x20, 0x20, 0x70, // 1
	0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
	0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
	0x90, 0x90, 0xF0, 0x10, 0x10, // 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
	0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
	0xF0, 0x10, 0x20, 0x40, 0x40, // 7
	0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
	0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
	0xF0, 0x90, 0xF0, 0x90, 0x90, // A
	0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
	0xF0, 0x80, 0x80, 0x80, 0xF0, // C
	0xE0, 0x90, 0x90, 0x90, 0xE0, // D
	0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
	0xF0, 0x80, 0xF0, 0x80, 0x80 // F
};

typedef struct EMULATOR_STATE {
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

	int8_t waiting_for_key;
	bool has_pressed;
} EMULATOR_STATE;

EMULATOR_STATE g_emulator;
bool g_debug = false;

// SDL state
SDL_Window *g_window = NULL;
SDL_Renderer *g_renderer = NULL;
SDL_Texture *g_texture = NULL;

#define font(f) g_emulator.memory[0x050 + (f) * 5]
#define pixel(x, y) g_emulator.display[(y) * TARGET_WIDTH + (x)]
#define draw(x, y, state)                                  \
	do {                                               \
		pixel((x), (y)) ^= state ? 0xFF97F1CD : 0; \
	} while (false)

bool next_instruction(bool trace);

void dump_registers() {
	fprintf(stderr, "===== REGISTERS DUMP ====\n");
	for (int i = 0; i < sizeof(g_emulator.registers); ++i) {
		fprintf(stderr, "V%X = 0x%02hx  ", i, g_emulator.registers[i]);
		if ((i + 1) % 4 == 0) {
			fprintf(stderr, "\n");
		}
	}
	fprintf(stderr, "SP = 0x%02hx  ", g_emulator.sp);
	fprintf(stderr, "DT = 0x%02hx  ", g_emulator.dt);
	fprintf(stderr, "ST = 0x%02hx\n", g_emulator.st);
	fprintf(stderr, "VI = 0x%04hx\n", g_emulator.vi);
	fprintf(stderr, "PC = 0x%04hx\n", g_emulator.pc);
}

void dump_stack() {
	fprintf(stderr, "\n===== STACK DUMP ====\n");
	for (int i = 0; i < ARRAY_SIZE(g_emulator.stack); ++i) {
		fprintf(stderr, "[%02hhd] = 0x%02hx", i, g_emulator.stack[i]);
		if (i == g_emulator.sp) {
			printf("  <-- SP");
		}
		printf("\n");
	}
}

static inline void dump_memory() {
	fprintf(stderr, "\n===== MEMORY DUMP ====\n");
	hexdump(g_emulator.memory, sizeof(g_emulator.memory), 0);
}

void dump_state() {
	dump_registers();
	dump_stack();
	// dump_memory();
}

void reset_state() {
	memset(g_emulator.memory, 0, sizeof(g_emulator.memory));
	memset(g_emulator.stack, 0, sizeof(g_emulator.stack));
	memset(g_emulator.registers, 0, sizeof(g_emulator.registers));
	memset(g_emulator.display, 0, sizeof(g_emulator.display));
	memset(g_emulator.keyboard, 0, sizeof(g_emulator.keyboard));

	memcpy(g_emulator.memory + 0x050, EMULATOR_FONTS, sizeof(EMULATOR_FONTS));

	g_emulator.sp = 0;
	g_emulator.vi = 0;
	g_emulator.pc = PROG_BASE;
	g_emulator.dt = 0;
	g_emulator.st = 0;

	g_debug = false;
	g_emulator.waiting_for_key = -1;
	g_emulator.has_pressed = false;

	g_last_time = clock();
	g_current_time = g_last_time;
}

void init_graphics() {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "[!] SDL could not initialise! SDL error: %s\n", SDL_GetError());
	} else {
		g_window = SDL_CreateWindow("EchoesOf8 - CHIP-8 Emulator", SDL_WINDOWPOS_UNDEFINED,
					    SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT,
					    SDL_WINDOW_SHOWN);
		if (g_window == NULL) {
			fprintf(stderr, "[!] Window could not be created! SDL error: %s\n",
				SDL_GetError());
		} else {
			g_renderer = SDL_CreateRenderer(g_window, -1, SDL_TEXTUREACCESS_TARGET);

			g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888,
						      SDL_TEXTUREACCESS_TARGET, TARGET_WIDTH,
						      TARGET_HEIGHT);
		}
	}
}

void update_keyboard_state(SDL_Scancode scancode, uint8_t state) {
	bool keypad_pressed = true;
	switch (scancode) {
	case SDL_SCANCODE_1: // 1
		g_emulator.keyboard[0x1] = state;
		break;
	case SDL_SCANCODE_2: // 2
		g_emulator.keyboard[0x2] = state;
		break;
	case SDL_SCANCODE_3: // 3
		g_emulator.keyboard[0x3] = state;
		break;
	case SDL_SCANCODE_4: // C
		g_emulator.keyboard[0xC] = state;
		break;
	case SDL_SCANCODE_Q: // 4
		g_emulator.keyboard[0x4] = state;
		break;
	case SDL_SCANCODE_W: // 5
		g_emulator.keyboard[0x5] = state;
		break;
	case SDL_SCANCODE_E: // 6
		g_emulator.keyboard[0x6] = state;
		break;
	case SDL_SCANCODE_R: // D
		g_emulator.keyboard[0xD] = state;
		break;
	case SDL_SCANCODE_A: // 7
		g_emulator.keyboard[0x7] = state;
		break;
	case SDL_SCANCODE_S: // 8
		g_emulator.keyboard[0x8] = state;
		break;
	case SDL_SCANCODE_D: // 9
		g_emulator.keyboard[0x9] = state;
		break;
	case SDL_SCANCODE_F: // E
		g_emulator.keyboard[0xE] = state;
		break;
	case SDL_SCANCODE_Z: // A
		g_emulator.keyboard[0xA] = state;
		break;
	case SDL_SCANCODE_X: // 0
		g_emulator.keyboard[0x0] = state;
		break;
	case SDL_SCANCODE_C: // B
		g_emulator.keyboard[0xB] = state;
		break;
	case SDL_SCANCODE_V: // F
		g_emulator.keyboard[0xF] = state;
		break;
	default:
		keypad_pressed = false;
		break;
	}

	if (keypad_pressed) {
		printf("Key %s: %s\n", state ? "down" : "up", SDL_GetScancodeName(scancode));
	}
}

bool handle_input() {
	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		switch (e.type) {
		case SDL_QUIT:
			printf("Quitting...\n");
			return false;
		case SDL_KEYUP: {
			update_keyboard_state(e.key.keysym.scancode, 0);
			break;
		}
		case SDL_KEYDOWN: {
			switch (e.key.keysym.scancode) {
			case SDL_SCANCODE_ESCAPE:
				printf("Quitting...\n");
				return false;
			case SDL_SCANCODE_SPACE:
				g_debug = !g_debug;
				printf("%s emulator\n", g_debug ? "Paused" : "Unpaused");
				break;
			case SDL_SCANCODE_N:
				if (g_debug) {
					next_instruction(true);
					dump_registers();
					dump_stack();
					printf("\n");
				}
				break;
			default:
				update_keyboard_state(e.key.keysym.scancode, 1);
				break;
			}
			break;
		}
		}
	}

	return true;
}

void render() {
	SDL_SetRenderTarget(g_renderer, g_texture);
	SDL_UpdateTexture(g_texture, NULL, g_emulator.display, TARGET_WIDTH * sizeof(uint32_t));

	SDL_SetRenderTarget(g_renderer, NULL);
	SDL_RenderClear(g_renderer);
	SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);
	SDL_RenderPresent(g_renderer);
}

void cleanup() {
	SDL_DestroyTexture(g_texture);
	SDL_DestroyRenderer(g_renderer);
	SDL_DestroyWindow(g_window);
	SDL_Quit();
}

void print_instruction_state(Chip8Instruction instruction) {
	switch (instruction_format(instruction_type(instruction))) {
	case R_FORMAT:
		printf("# V%hX = %02hhx, V%hX = %02hhx", instruction.rformat.rx,
		       g_emulator.registers[instruction.rformat.rx], instruction.rformat.ry,
		       g_emulator.registers[instruction.rformat.ry]);
		break;
	case I_FORMAT:
		printf("# V%hX = %02hhx", instruction.iformat.reg,
		       g_emulator.registers[instruction.iformat.reg]);
		break;
	case A_FORMAT:
	case UNKNOWN_FORMAT:
		break;
	}
}

bool next_instruction(bool trace) {
	static uint16_t previous;
	if (g_emulator.pc < 0 || g_emulator.pc > sizeof(g_emulator.memory)) {
		fprintf(stderr, "[!] PC exceeds memory boundaries\n");
		return false;
	}

	Chip8Instruction instruction = bytes2inst(&g_emulator.memory[g_emulator.pc]);
	if (g_emulator.pc != previous && trace) {
		print_asm(instruction);
		printf("\t");
		print_instruction_state(instruction);
		printf("\n");
	}
	previous = g_emulator.pc;
	g_emulator.pc += 2;

	switch (instruction_type(instruction)) {
	case CHIP8_CLS:
		memset(g_emulator.display, 0, sizeof(g_emulator.display));
		break;
	case CHIP8_RET:
		g_emulator.pc = g_emulator.stack[--g_emulator.sp];
		break;
	case CHIP8_SYS_ADDR:
		// Ignore
		break;
	case CHIP8_JMP_ADDR:
		g_emulator.pc = instruction.aformat.addr;
		break;
	case CHIP8_CALL_ADDR:
		g_emulator.stack[g_emulator.sp++] = g_emulator.pc;
		g_emulator.pc = instruction.aformat.addr;
		break;
	case CHIP8_SE_VX_BYTE:
		if (g_emulator.registers[instruction.iformat.reg] == instruction.iformat.imm) {
			g_emulator.pc += 2;
		}
		break;
	case CHIP8_SNE_VX_BYTE:
		if (g_emulator.registers[instruction.iformat.reg] != instruction.iformat.imm) {
			g_emulator.pc += 2;
		}
		break;
	case CHIP8_SE_VX_VY:
		if (g_emulator.registers[instruction.rformat.rx] ==
		    g_emulator.registers[instruction.rformat.ry]) {
			g_emulator.pc += 2;
		}
		break;
	case CHIP8_LD_VX_BYTE:
		g_emulator.registers[instruction.iformat.reg] = instruction.iformat.imm;
		break;
	case CHIP8_ADD_VX_BYTE:
		g_emulator.registers[instruction.iformat.reg] += instruction.iformat.imm;
		break;
	case CHIP8_LD_VX_VY:
		g_emulator.registers[instruction.rformat.rx] =
			g_emulator.registers[instruction.rformat.ry];
		break;
	case CHIP8_OR_VX_VY:
		g_emulator.registers[instruction.rformat.rx] |=
			g_emulator.registers[instruction.rformat.ry];
		break;
	case CHIP8_AND_VX_VY:
		g_emulator.registers[instruction.rformat.rx] &=
			g_emulator.registers[instruction.rformat.ry];
		break;
	case CHIP8_XOR_VX_VY:
		g_emulator.registers[instruction.rformat.rx] ^=
			g_emulator.registers[instruction.rformat.ry];
		break;
	case CHIP8_ADD_VX_VY: {
		uint16_t result = g_emulator.registers[instruction.rformat.rx] +
				  g_emulator.registers[instruction.rformat.ry];
		g_emulator.registers[instruction.rformat.rx] = (uint8_t)result;
		g_emulator.registers[0xF] = (0x100 & result) > 0;
		break;
	}
	case CHIP8_SUB_VX_VY: {
		bool flag = g_emulator.registers[instruction.rformat.rx] >=
			    g_emulator.registers[instruction.rformat.ry];
		g_emulator.registers[instruction.rformat.rx] -=
			g_emulator.registers[instruction.rformat.ry];
		g_emulator.registers[0xF] = flag;
		break;
	}
	case CHIP8_SHR_VX: {
		bool flag = g_emulator.registers[instruction.rformat.rx] & 1;
		g_emulator.registers[instruction.rformat.rx] >>= 1;
		g_emulator.registers[0xF] = flag;
		break;
	}
	case CHIP8_SUBN_VX_VY: {
		bool flag = g_emulator.registers[instruction.rformat.ry] >=
			    g_emulator.registers[instruction.rformat.rx];
		g_emulator.registers[instruction.rformat.rx] =
			g_emulator.registers[instruction.rformat.ry] -
			g_emulator.registers[instruction.rformat.rx];
		g_emulator.registers[0xF] = flag;
		break;
	}
	case CHIP8_SHL_VX: {
		bool flag = (g_emulator.registers[instruction.rformat.rx] & 0x80) > 0;
		g_emulator.registers[instruction.rformat.rx] <<= 1;
		g_emulator.registers[0xF] = flag;
		break;
	}
	case CHIP8_SNE_VX_VY:
		if (g_emulator.registers[instruction.rformat.rx] !=
		    g_emulator.registers[instruction.rformat.ry]) {
			g_emulator.pc += 2;
		}
		break;
	case CHIP8_LD_I_ADDR:
		g_emulator.vi = instruction.aformat.addr;
		break;
	case CHIP8_JMP_V0_ADDR:
		g_emulator.pc = instruction.aformat.addr + g_emulator.registers[0];
		break;
	case CHIP8_RND_VX_BYTE:
		g_emulator.registers[instruction.iformat.reg] = (rand() % 256) &
								instruction.iformat.imm;
		break;
	case CHIP8_DRW_VX_VY_NIBBLE: {
		bool flag = false;
		int origin_x = g_emulator.registers[instruction.rformat.rx];
		int origin_y = g_emulator.registers[instruction.rformat.ry];
		for (int row = 0; row < instruction.rformat.imm; ++row) {
			uint8_t byte = g_emulator.memory[g_emulator.vi + row];
			int y = (origin_y + row) % TARGET_HEIGHT;
			for (uint8_t col = 0; col < 8 && byte; ++col) {
				int x = (origin_x + col) % TARGET_WIDTH;
				uint8_t state = byte & 0x80;
				flag = pixel(x, y) > 0 && state;
				draw(x, y, state);
				byte <<= 1;
			}
		}
		g_emulator.registers[0xF] = flag;
		break;
	}
	case CHIP8_SKP_VX:
		if (g_emulator.keyboard[g_emulator.registers[instruction.iformat.reg]]) {
			g_emulator.pc += 2;
		}
		break;
	case CHIP8_SKNP_VX:
		if (g_emulator.keyboard[g_emulator.registers[instruction.iformat.reg]] == 0) {
			g_emulator.pc += 2;
		}
		break;
	case CHIP8_LD_VX_DT:
		g_emulator.registers[instruction.iformat.reg] = g_emulator.dt;
		break;
	case CHIP8_LD_VX_K:
		g_emulator.waiting_for_key = g_emulator.registers[instruction.iformat.reg];
		// Storing early should be fine since processing is halted
		g_emulator.registers[instruction.iformat.reg] = g_emulator.waiting_for_key;
		printf("Waiting for key: %X\n", g_emulator.waiting_for_key);
		break;
	case CHIP8_LD_DT_VX:
		g_emulator.dt = g_emulator.registers[instruction.iformat.reg];
		break;
	case CHIP8_LD_ST_VX:
		g_emulator.st = g_emulator.registers[instruction.iformat.reg];
		break;
	case CHIP8_ADD_I_VX:
		g_emulator.vi += g_emulator.registers[instruction.iformat.reg];
		break;
	case CHIP8_LD_F_VX:
		g_emulator.vi = font(g_emulator.registers[instruction.iformat.reg]);
		break;
	case CHIP8_LD_B_VX: {
		uint8_t digit = g_emulator.registers[instruction.iformat.reg];
		g_emulator.memory[g_emulator.vi + 2] = digit % 10;
		digit /= 10;
		g_emulator.memory[g_emulator.vi + 1] = digit % 10;
		digit /= 10;
		g_emulator.memory[g_emulator.vi] = digit % 10;
		break;
	}
	case CHIP8_LD_I_VX:
		for (int i = 0; i <= instruction.iformat.reg; ++i) {
			g_emulator.memory[g_emulator.vi + i] = g_emulator.registers[i];
		}
		break;
	case CHIP8_LD_VX_I:
		for (int i = 0; i <= instruction.iformat.reg; ++i) {
			g_emulator.registers[i] = g_emulator.memory[g_emulator.vi + i];
		}
		break;
	case CHIP8_UNKNOWN:
		fprintf(stderr, "[!] Unknown instruction received: 0x%04hx @ 0x%03hx\n",
			instruction.raw, g_emulator.pc);
		return false;
	}

	return true;
}

void emulate(uint8_t *rom, size_t rom_size, bool debug) {
	printf("Emulating!\n");

	srand(time(NULL));
	reset_state();
	init_graphics();

	memcpy(g_emulator.memory + PROG_BASE, rom, rom_size);

	if (debug) {
		g_debug = true;
		printf("Debugging enabled!\n");
	}
	while (handle_input()) {
		g_current_time = clock();
		if (g_current_time < g_last_time) {
			// Handle overflow
			g_last_time = g_current_time;
		}

		if (!g_debug) {
			if (g_emulator.waiting_for_key > 0) {
				if (g_emulator.keyboard[g_emulator.waiting_for_key]) {
					g_emulator.has_pressed = true;
				} else if (g_emulator.has_pressed) {
					g_emulator.waiting_for_key = -1;
					g_emulator.has_pressed = false;
				}
			} else {
				if (!next_instruction(false)) {
					dump_state();
					g_debug = true;
					printf("\n[!] Something went wrong @ 0x%03hx: ",
					       g_emulator.pc);
					print_asm(
						bytes2inst(g_emulator.memory + g_emulator.pc - 2));
					printf("\n");
				}
			}
			// TODO: How to handle timers when debugging?
			if (g_current_time - g_last_time >= EMULATOR_INTERVAL) {
				g_last_time = g_current_time;
				if (g_emulator.dt > 0) {
					g_emulator.dt--;
				}
				if (g_emulator.st > 0) {
					g_emulator.st--;
					// TODO: Make a beep
				}
			}
		}
		render();
	}

	cleanup();
}
