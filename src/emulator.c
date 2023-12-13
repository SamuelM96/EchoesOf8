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
#define INSTRUCTIONS_PER_SEC 700
const clock_t EMULATOR_INTERVAL = CLOCKS_PER_SEC / TARGET_HZ;
const clock_t INSTRUCTION_INTERVAL = CLOCKS_PER_SEC / INSTRUCTIONS_PER_SEC;
clock_t g_last_inst_time;
clock_t g_last_timer_time;
clock_t g_current_time;

#define TARGET_WIDTH 64
#define TARGET_HEIGHT 32
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 640
const int SCALE_X = SCREEN_WIDTH / TARGET_WIDTH;
const int SCALE_Y = SCREEN_HEIGHT / TARGET_HEIGHT;

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
} EmulatorState;

bool g_debug = false;

// SDL state
SDL_Window *g_window = NULL;
SDL_Renderer *g_renderer = NULL;
SDL_Texture *g_texture = NULL;

#define pixel(emulator, x, y) (emulator)->display[(y) * TARGET_WIDTH + (x)]

bool process_instruction(EmulatorState *, Chip8Instruction);
void print_instruction_state(EmulatorState *, Chip8Instruction);

Chip8Instruction fetch_next(EmulatorState *emulator, bool trace) {
	static uint16_t prev_inst_addr = 0;
	Chip8Instruction instruction = bytes2inst(&emulator->memory[emulator->pc]);
	if (emulator->pc != prev_inst_addr && trace) {
		print_asm(instruction);
		printf("\t");
		print_instruction_state(emulator, instruction);
		printf("\n");
	}
	prev_inst_addr = emulator->pc;
	emulator->pc += 2;

	return instruction;
}

void dump_registers(EmulatorState *emulator) {
	fprintf(stderr, "===== REGISTERS DUMP ====\n");
	for (int i = 0; i < sizeof(emulator->registers); ++i) {
		fprintf(stderr, "V%X = 0x%02hx  ", i, emulator->registers[i]);
		if ((i + 1) % 4 == 0) {
			fprintf(stderr, "\n");
		}
	}
	fprintf(stderr, "SP = 0x%02hx  ", emulator->sp);
	fprintf(stderr, "DT = 0x%02hx  ", emulator->dt);
	fprintf(stderr, "ST = 0x%02hx\n", emulator->st);
	fprintf(stderr, "VI = 0x%04hx\n", emulator->vi);
	fprintf(stderr, "PC = 0x%04hx\n", emulator->pc);
}

void dump_stack(EmulatorState *emulator) {
	fprintf(stderr, "\n===== STACK DUMP ====\n");
	for (int i = 0; i < ARRAY_SIZE(emulator->stack); ++i) {
		fprintf(stderr, "[%02hhd] = 0x%02hx", i, emulator->stack[i]);
		if (i == emulator->sp) {
			printf("  <-- SP");
		}
		printf("\n");
	}
}

static inline void dump_memory(EmulatorState *emulator) {
	fprintf(stderr, "\n===== MEMORY DUMP ====\n");
	hexdump(emulator->memory, sizeof(emulator->memory), 0);
}

void dump_state(EmulatorState *emulator) {
	dump_registers(emulator);
	dump_stack(emulator);
	// dump_memory(emulator);
}

void reset_state(EmulatorState *emulator) {
	memset(emulator->memory, 0, sizeof(emulator->memory));
	memset(emulator->stack, 0, sizeof(emulator->stack));
	memset(emulator->registers, 0, sizeof(emulator->registers));
	memset(emulator->display, 0, sizeof(emulator->display));
	memset(emulator->keyboard, 0, sizeof(emulator->keyboard));

	const uint8_t emulator_fonts[80] = {
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
		0xF0, 0x80, 0xF0, 0x80, 0x80, // F
	};
	memcpy(emulator->memory + FONT_BASE_ADDR, emulator_fonts, sizeof(emulator_fonts));

	emulator->sp = 0;
	emulator->vi = 0;
	emulator->pc = PROG_BASE;
	emulator->dt = 0;
	emulator->st = 0;

	g_debug = false;

	g_last_timer_time = clock();
	g_current_time = g_last_timer_time;
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

void update_keyboard_state(EmulatorState *emulator, SDL_Scancode scancode, uint8_t state) {
	bool keypad_pressed = true;
	switch (scancode) {
	case SDL_SCANCODE_1:
		emulator->keyboard[0x1] = state;
		break;
	case SDL_SCANCODE_2:
		emulator->keyboard[0x2] = state;
		break;
	case SDL_SCANCODE_3:
		emulator->keyboard[0x3] = state;
		break;
	case SDL_SCANCODE_4:
		emulator->keyboard[0xC] = state;
		break;
	case SDL_SCANCODE_Q:
		emulator->keyboard[0x4] = state;
		break;
	case SDL_SCANCODE_W:
		emulator->keyboard[0x5] = state;
		break;
	case SDL_SCANCODE_E:
		emulator->keyboard[0x6] = state;
		break;
	case SDL_SCANCODE_R:
		emulator->keyboard[0xD] = state;
		break;
	case SDL_SCANCODE_A:
		emulator->keyboard[0x7] = state;
		break;
	case SDL_SCANCODE_S:
		emulator->keyboard[0x8] = state;
		break;
	case SDL_SCANCODE_D:
		emulator->keyboard[0x9] = state;
		break;
	case SDL_SCANCODE_F:
		emulator->keyboard[0xE] = state;
		break;
	case SDL_SCANCODE_Z:
		emulator->keyboard[0xA] = state;
		break;
	case SDL_SCANCODE_X:
		emulator->keyboard[0x0] = state;
		break;
	case SDL_SCANCODE_C:
		emulator->keyboard[0xB] = state;
		break;
	case SDL_SCANCODE_V:
		emulator->keyboard[0xF] = state;
		break;
	default:
		keypad_pressed = false;
		break;
	}

	if (keypad_pressed) {
		printf("Key %s: %s\n", state ? "down" : "up", SDL_GetScancodeName(scancode));
	}
}

bool handle_input(EmulatorState *emulator) {
	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		switch (e.type) {
		case SDL_QUIT:
			printf("Quitting...\n");
			return false;
		case SDL_KEYUP: {
			update_keyboard_state(emulator, e.key.keysym.scancode, 0);
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
					process_instruction(emulator, fetch_next(emulator, true));
					dump_registers(emulator);
					dump_stack(emulator);
					printf("\n");
				}
				break;
			default:
				update_keyboard_state(emulator, e.key.keysym.scancode, 1);
				break;
			}
			break;
		}
		}
	}

	return true;
}

void render(uint32_t *pixels, int width) {
	SDL_SetRenderTarget(g_renderer, g_texture);
	SDL_UpdateTexture(g_texture, NULL, pixels, width * sizeof(uint32_t));

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

void print_instruction_state(EmulatorState *emulator, Chip8Instruction instruction) {
	switch (instruction_format(instruction_type(instruction))) {
	case R_FORMAT:
		printf("# V%hX = %02hhx, V%hX = %02hhx", instruction.rformat.rx,
		       emulator->registers[instruction.rformat.rx], instruction.rformat.ry,
		       emulator->registers[instruction.rformat.ry]);
		break;
	case I_FORMAT:
		printf("# V%hX = %02hhx", instruction.iformat.reg,
		       emulator->registers[instruction.iformat.reg]);
		break;
	case A_FORMAT:
	case UNKNOWN_FORMAT:
		break;
	}
}

bool process_instruction(EmulatorState *emulator, Chip8Instruction instruction) {
	if (emulator->pc < 0 || emulator->pc > sizeof(emulator->memory)) {
		fprintf(stderr, "[!] PC exceeds memory boundaries\n");
		return false;
	}

	switch (instruction_type(instruction)) {
	case CHIP8_CLS:
		memset(emulator->display, 0, sizeof(emulator->display));
		render(emulator->display, TARGET_WIDTH);
		break;
	case CHIP8_RET:
		emulator->pc = emulator->stack[--emulator->sp];
		break;
	case CHIP8_SYS_ADDR:
		// Ignore
		break;
	case CHIP8_JMP_ADDR:
		emulator->pc = instruction.aformat.addr;
		break;
	case CHIP8_CALL_ADDR:
		emulator->stack[emulator->sp++] = emulator->pc;
		emulator->pc = instruction.aformat.addr;
		break;
	case CHIP8_SE_VX_BYTE:
		if (emulator->registers[instruction.iformat.reg] == instruction.iformat.imm) {
			emulator->pc += 2;
		}
		break;
	case CHIP8_SNE_VX_BYTE:
		if (emulator->registers[instruction.iformat.reg] != instruction.iformat.imm) {
			emulator->pc += 2;
		}
		break;
	case CHIP8_SE_VX_VY:
		if (emulator->registers[instruction.rformat.rx] ==
		    emulator->registers[instruction.rformat.ry]) {
			emulator->pc += 2;
		}
		break;
	case CHIP8_LD_VX_BYTE:
		emulator->registers[instruction.iformat.reg] = instruction.iformat.imm;
		break;
	case CHIP8_ADD_VX_BYTE:
		emulator->registers[instruction.iformat.reg] += instruction.iformat.imm;
		break;
	case CHIP8_LD_VX_VY:
		emulator->registers[instruction.rformat.rx] =
			emulator->registers[instruction.rformat.ry];
		break;
	case CHIP8_OR_VX_VY:
		emulator->registers[instruction.rformat.rx] |=
			emulator->registers[instruction.rformat.ry];
		break;
	case CHIP8_AND_VX_VY:
		emulator->registers[instruction.rformat.rx] &=
			emulator->registers[instruction.rformat.ry];
		break;
	case CHIP8_XOR_VX_VY:
		emulator->registers[instruction.rformat.rx] ^=
			emulator->registers[instruction.rformat.ry];
		break;
	case CHIP8_ADD_VX_VY: {
		uint16_t result = emulator->registers[instruction.rformat.rx] +
				  emulator->registers[instruction.rformat.ry];
		emulator->registers[instruction.rformat.rx] = (uint8_t)result;
		emulator->registers[0xF] = (0x100 & result) > 0;
		break;
	}
	case CHIP8_SUB_VX_VY: {
		bool flag = emulator->registers[instruction.rformat.rx] >=
			    emulator->registers[instruction.rformat.ry];
		emulator->registers[instruction.rformat.rx] -=
			emulator->registers[instruction.rformat.ry];
		emulator->registers[0xF] = flag;
		break;
	}
	case CHIP8_SHR_VX: {
		bool flag = emulator->registers[instruction.rformat.rx] & 1;
		emulator->registers[instruction.rformat.rx] >>= 1;
		emulator->registers[0xF] = flag;
		break;
	}
	case CHIP8_SUBN_VX_VY: {
		bool flag = emulator->registers[instruction.rformat.ry] >=
			    emulator->registers[instruction.rformat.rx];
		emulator->registers[instruction.rformat.rx] =
			emulator->registers[instruction.rformat.ry] -
			emulator->registers[instruction.rformat.rx];
		emulator->registers[0xF] = flag;
		break;
	}
	case CHIP8_SHL_VX: {
		bool flag = (emulator->registers[instruction.rformat.rx] & 0x80) > 0;
		emulator->registers[instruction.rformat.rx] <<= 1;
		emulator->registers[0xF] = flag;
		break;
	}
	case CHIP8_SNE_VX_VY:
		if (emulator->registers[instruction.rformat.rx] !=
		    emulator->registers[instruction.rformat.ry]) {
			emulator->pc += 2;
		}
		break;
	case CHIP8_LD_I_ADDR:
		emulator->vi = instruction.aformat.addr;
		break;
	case CHIP8_JMP_V0_ADDR:
		emulator->pc = instruction.aformat.addr + emulator->registers[0];
		break;
	case CHIP8_RND_VX_BYTE:
		emulator->registers[instruction.iformat.reg] = (rand() % 256) &
							       instruction.iformat.imm;
		break;
	case CHIP8_DRW_VX_VY_NIBBLE: {
		bool flag = false;
		int origin_x = emulator->registers[instruction.rformat.rx];
		int origin_y = emulator->registers[instruction.rformat.ry];
		for (int row = 0; row < instruction.rformat.imm; ++row) {
			uint8_t byte = emulator->memory[emulator->vi + row];
			int y = (origin_y + row) % TARGET_HEIGHT;
			for (uint8_t col = 0; col < 8 && byte; ++col) {
				int x = (origin_x + col) % TARGET_WIDTH;
				uint8_t state = byte & 0x80;
				flag = pixel(emulator, x, y) > 0 && state;
				pixel(emulator, x, y) ^= state ? 0xFF97F1CD : 0;
				byte <<= 1;
			}
		}
		emulator->registers[0xF] = flag;
		render(emulator->display, TARGET_WIDTH);
		break;
	}
	case CHIP8_SKP_VX:
		if (emulator->keyboard[emulator->registers[instruction.iformat.reg]]) {
			emulator->pc += 2;
		}
		break;
	case CHIP8_SKNP_VX:
		if (emulator->keyboard[emulator->registers[instruction.iformat.reg]] == 0) {
			emulator->pc += 2;
		}
		break;
	case CHIP8_LD_VX_DT:
		emulator->registers[instruction.iformat.reg] = emulator->dt;
		break;
	case CHIP8_LD_VX_K: {
		static int8_t key = -1;
		static bool is_held = false;

		bool is_pressed = false;
		for (int i = 0; i < sizeof(emulator->keyboard); ++i) {
			if (emulator->keyboard[i]) {
				is_pressed = true;
				key = i;
				break;
			}
		}

		if (is_pressed) {
			is_held = true;
		} else if (is_held) {
			emulator->registers[instruction.iformat.reg] = key;
			is_held = false;
			break;
		}

		emulator->pc -= 2;
		break;
	}
	case CHIP8_LD_DT_VX:
		emulator->dt = emulator->registers[instruction.iformat.reg];
		break;
	case CHIP8_LD_ST_VX:
		emulator->st = emulator->registers[instruction.iformat.reg];
		break;
	case CHIP8_ADD_I_VX:
		emulator->vi += emulator->registers[instruction.iformat.reg];
		break;
	case CHIP8_LD_F_VX:
		emulator->vi = FONT_BASE_ADDR + emulator->registers[instruction.iformat.reg] * 5;
		break;
	case CHIP8_LD_B_VX: {
		uint8_t digit = emulator->registers[instruction.iformat.reg];
		emulator->memory[emulator->vi + 2] = digit % 10;
		digit /= 10;
		emulator->memory[emulator->vi + 1] = digit % 10;
		digit /= 10;
		emulator->memory[emulator->vi] = digit % 10;
		break;
	}
	case CHIP8_LD_I_VX:
		for (int i = 0; i <= instruction.iformat.reg; ++i) {
			emulator->memory[emulator->vi + i] = emulator->registers[i];
		}
		break;
	case CHIP8_LD_VX_I:
		for (int i = 0; i <= instruction.iformat.reg; ++i) {
			emulator->registers[i] = emulator->memory[emulator->vi + i];
		}
		break;
	case CHIP8_UNKNOWN:
		fprintf(stderr, "[!] Unknown instruction received: 0x%04hx @ 0x%03hx\n",
			instruction.raw, emulator->pc);
		return false;
	}

	return true;
}

void emulate(uint8_t *rom, size_t rom_size, bool debug) {
	printf("Emulating!\n");

	EmulatorState emulator = { 0 };

	srand(time(NULL));
	reset_state(&emulator);
	init_graphics();

	memcpy(emulator.memory + PROG_BASE, rom, rom_size);

	if (debug) {
		g_debug = true;
		printf("Debugging enabled!\n");
	}
	while (handle_input(&emulator)) {
		g_current_time = clock();
		if (g_current_time < g_last_timer_time) {
			// Handle overflow
			g_last_timer_time = g_current_time;
		}

		if (!g_debug) {
			if (g_current_time - g_last_inst_time >= INSTRUCTION_INTERVAL) {
				g_last_inst_time = g_current_time;
				Chip8Instruction instruction = fetch_next(&emulator, false);
				if (!process_instruction(&emulator, instruction)) {
					dump_state(&emulator);
					g_debug = true;
					printf("\n[!] Something went wrong @ 0x%03hx: ",
					       emulator.pc - 2);
					print_asm(instruction);
					printf("\n");
				}
			}

			// TODO: How to handle timers when debugging?
			if (g_current_time - g_last_timer_time >= EMULATOR_INTERVAL) {
				g_last_timer_time = g_current_time;
				if (emulator.dt > 0) {
					emulator.dt--;
				}
				if (emulator.st > 0) {
					emulator.st--;
					// TODO: Make a beep
				}
			}
		}
	}

	cleanup();
}
