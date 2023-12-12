#include "emulator.h"
#include "common.h"
#include "disassembler.h"
#include "instructions.h"

#include <SDL.h>
#include <SDL_events.h>
#include <SDL_pixels.h>
#include <SDL_render.h>
#include <SDL_surface.h>
#include <SDL_video.h>

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define TARGET_WIDTH 64
#define TARGET_HEIGHT 32
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 640
const int SCALE_X = SCREEN_WIDTH / TARGET_WIDTH;
const int SCALE_Y = SCREEN_HEIGHT / TARGET_HEIGHT;

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
uint16_t EMULATOR_PC = PROG_BASE;

// Delay timer
uint8_t EMULATOR_DT;

// Sound timer
uint8_t EMULATOR_ST;

// Display pixels
uint32_t EMULATOR_DISPLAY[TARGET_WIDTH * TARGET_HEIGHT];

// SDL state
SDL_Window *g_window = NULL;
SDL_Renderer *g_renderer = NULL;
SDL_Texture *g_texture = NULL;

#define pixel(x, y) EMULATOR_DISPLAY[(y) * TARGET_WIDTH + (x)]
#define draw(x, y, state)                                  \
	do {                                               \
		pixel((x), (y)) ^= state ? 0xFF97F1CD : 0; \
	} while (false)

void reset_state() {
	memset(EMULATOR_MEMORY, 0, sizeof(EMULATOR_MEMORY));
	memset(EMULATOR_STACK, 0, sizeof(EMULATOR_STACK));
	memset(EMULATOR_REGISTERS, 0, sizeof(EMULATOR_REGISTERS));
	memset(EMULATOR_DISPLAY, 0, sizeof(EMULATOR_DISPLAY));

	EMULATOR_SP = 0;
	EMULATOR_VI = 0;
	EMULATOR_PC = PROG_BASE;
	EMULATOR_DT = 0;
	EMULATOR_ST = 0;
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

bool handle_input() {
	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		switch (e.type) {
		case SDL_QUIT:
			return false;
			break;
		case SDL_MOUSEMOTION:
		case SDL_MOUSEBUTTONDOWN:
			if (e.button.button == SDL_BUTTON_LEFT) {
				draw(e.motion.x / SCALE_X, e.motion.y / SCALE_Y, true);
			}
			break;
		}
	}

	return true;
}

void render() {
	SDL_SetRenderTarget(g_renderer, g_texture);
	SDL_UpdateTexture(g_texture, NULL, EMULATOR_DISPLAY, TARGET_WIDTH * sizeof(uint32_t));

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

void dump_state() {
	fprintf(stderr, "===== REGISTERS DUMP ====\n");
	for (int i = 0; i < sizeof(EMULATOR_REGISTERS); ++i) {
		fprintf(stderr, "V%X = 0x%02hx  ", i, EMULATOR_REGISTERS[i]);
		if ((i + 1) % 4 == 0) {
			fprintf(stderr, "\n");
		}
	}
	fprintf(stderr, "SP = 0x%02hx  ", EMULATOR_SP);
	fprintf(stderr, "DT = 0x%02hx  ", EMULATOR_DT);
	fprintf(stderr, "ST = 0x%02hx\n", EMULATOR_ST);
	fprintf(stderr, "VI = 0x%04hx\n", EMULATOR_VI);
	fprintf(stderr, "PC = 0x%04hx\n", EMULATOR_PC);

	fprintf(stderr, "\n===== STACK DUMP ====\n");
	for (int i = 0; i < ARRAY_SIZE(EMULATOR_STACK); ++i) {
		fprintf(stderr, "[%02hhd] = 0x%02hx\n", i, EMULATOR_STACK[i]);
	}

	fprintf(stderr, "\n===== MEMORY DUMP ====\n");
	hexdump(EMULATOR_MEMORY, sizeof(EMULATOR_MEMORY), 0);
}

bool next_instruction() {
	if (EMULATOR_PC < 0 || EMULATOR_PC > sizeof(EMULATOR_MEMORY)) {
		fprintf(stderr, "[!] PC exceeds memory limit");
		return false;
	}

	Chip8Instruction instruction = bytes2inst(&EMULATOR_MEMORY[EMULATOR_PC]);
	print_asm(instruction);
	EMULATOR_PC += 2;

	switch (instruction_type(instruction)) {
	case CHIP8_CLS:
		memset(EMULATOR_DISPLAY, 0, sizeof(EMULATOR_DISPLAY));
		break;
	case CHIP8_RET:
	case CHIP8_SYS_ADDR:
		fprintf(stderr, "[!] SYS instructions not supported: 0x%04hx @ 0x%03hx\n",
			instruction.raw, EMULATOR_PC);
		return false;
	case CHIP8_JMP_ADDR:
		EMULATOR_PC = instruction.aformat.addr;
		break;
	case CHIP8_CALL_ADDR:
		return false;
	case CHIP8_SE_VX_BYTE:
		return false;
	case CHIP8_SNE_VX_BYTE:
		return false;
	case CHIP8_SE_VX_VY:
		return false;
	case CHIP8_LD_VX_BYTE:
		EMULATOR_REGISTERS[instruction.iformat.reg] = instruction.iformat.imm;
		break;
	case CHIP8_ADD_VX_BYTE:
		return false;
	case CHIP8_LD_VX_VY:
		EMULATOR_REGISTERS[instruction.rformat.rx] =
			EMULATOR_REGISTERS[instruction.rformat.ry];
		break;
	case CHIP8_OR_VX_VY:
		return false;
	case CHIP8_AND_VX_VY:
		return false;
	case CHIP8_XOR_VX_VY:
		return false;
	case CHIP8_ADD_VX_VY:
		return false;
	case CHIP8_SUB_VX_VY:
		return false;
	case CHIP8_SHR_VX:
		return false;
	case CHIP8_SUBN_VX_VY:
		return false;
	case CHIP8_SHL_VX:
		return false;
	case CHIP8_SNE_VX_VY:
		return false;
	case CHIP8_LD_I_ADDR:
		EMULATOR_VI = instruction.aformat.addr;
		break;
	case CHIP8_JMP_V0_ADDR:
		return false;
	case CHIP8_RND_VX_BYTE:
		return false;
	case CHIP8_DRW_VX_VY_NIBBLE: {
		int origin_x = EMULATOR_REGISTERS[instruction.rformat.rx];
		int origin_y = EMULATOR_REGISTERS[instruction.rformat.ry];
		EMULATOR_REGISTERS[0xF] = pixel(origin_x, origin_y) > 0;
		for (int row = 0; row < instruction.rformat.imm; ++row) {
			uint8_t byte = EMULATOR_MEMORY[EMULATOR_VI + row];
			int y = (origin_y + row) % TARGET_HEIGHT;
			for (uint8_t col = 0; col < 8 && byte; ++col) {
				int x = (origin_x + col) % TARGET_WIDTH;
				draw(x, y, byte & 0x80);
				byte <<= 1;
			}
		}
		break;
	}
	case CHIP8_SKP_VX:
		return false;
	case CHIP8_SKNP_VX:
		return false;
	case CHIP8_LD_VX_DT:
		return false;
	case CHIP8_LD_VX_K:
		return false;
	case CHIP8_LD_DT_VX:
		return false;
	case CHIP8_LD_ST_VX:
		return false;
	case CHIP8_ADD_I_VX:
		return false;
	case CHIP8_LD_F_VX:
		return false;
	case CHIP8_LD_B_VX:
		return false;
	case CHIP8_LD_I_VX:
		return false;
	case CHIP8_LD_VX_I:
		return false;
	case CHIP8_UNKNOWN:
		fprintf(stderr, "[!] Unknown instruction received: 0x%04hx @ 0x%03hx\n",
			instruction.raw, EMULATOR_PC);
		return false;
	}

	return true;
}

void emulate(uint8_t *rom, size_t rom_size) {
	printf("Emulating!\n");

	reset_state();
	init_graphics();

	memcpy(EMULATOR_MEMORY + PROG_BASE, rom, rom_size);

	bool pause = false;
	while (handle_input()) {
		if (!pause) {
			if (!next_instruction()) {
				dump_state();
				pause = true;
			}
		}
		render();
	}

	cleanup();
}
