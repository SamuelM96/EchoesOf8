#include "emulator.h"
#include "common.h"
#include "disassembler.h"
#include "instructions.h"

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_SDL_RENDERER_IMPLEMENTATION
#include "nuklear.h"
#include "nuklear_sdl_renderer.h"

#include <SDL.h>
#include <SDL_audio.h>
#include <SDL_error.h>
#include <SDL_events.h>
#include <SDL_keyboard.h>
#include <SDL_log.h>
#include <SDL_pixels.h>
#include <SDL_render.h>
#include <SDL_scancode.h>
#include <SDL_stdinc.h>
#include <SDL_surface.h>
#include <SDL_video.h>

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

bool g_debug = false;
const int SCALE_X = SCREEN_WIDTH / TARGET_WIDTH;
const int SCALE_Y = SCREEN_HEIGHT / TARGET_HEIGHT;

// SDL & Nuklear state
SDL_Window *g_window = NULL;
SDL_Renderer *g_renderer = NULL;
SDL_Texture *g_texture = NULL;
struct nk_context *g_nk_ctx;

// Beeper state
SDL_AudioSpec g_beeper_spec;
SDL_AudioDeviceID g_beeper_id;
double g_beeper_volume = 0.1;
double g_beeper_current_volume = 0.0;
double g_beeper_frequency = 261.63;
double g_beeper_phase = 0.0;
bool g_beeper_in_attack = false;
bool g_beeper_in_release = false;
bool g_beeper_on = false;

// Sawtooth beeper
static void beeper_callback(void *userdata, uint8_t *_stream, int _len) {
	const double attack_release_time = 0.005;

	int16_t *stream = (int16_t *)_stream;
	int len = _len / sizeof(int16_t);

	double attack_release_delta = g_beeper_volume / (attack_release_time * g_beeper_spec.freq);
	double phase_increment = 2.0 * g_beeper_frequency / g_beeper_spec.freq;

	for (int sample = 0; sample < len; ++sample) {
		g_beeper_phase += phase_increment;
		if (g_beeper_phase >= 1.0)
			g_beeper_phase -= 2.0;

		if (g_beeper_in_attack) {
			g_beeper_current_volume += attack_release_delta;
			if (g_beeper_current_volume >= g_beeper_volume) {
				g_beeper_current_volume = g_beeper_volume;
				g_beeper_in_attack = false;
			}
		}

		if (g_beeper_in_release) {
			g_beeper_current_volume -= attack_release_delta;
			if (g_beeper_current_volume <= 0.0) {
				g_beeper_current_volume = 0.0;
				g_beeper_in_release = false;
				SDL_PauseAudioDevice(g_beeper_id, 1);
			}
		}

		stream[sample] = g_beeper_phase * g_beeper_current_volume * INT16_MAX / 2.0;
	}
}

void beeper_state(bool on) {
	if (!g_beeper_on && on) {
		g_beeper_on = true;
		g_beeper_in_attack = true;
		g_beeper_in_release = false;
		g_beeper_current_volume = 0.0;
		SDL_PauseAudioDevice(g_beeper_id, 0);
	} else if (g_beeper_on && !on) {
		g_beeper_on = false;
		g_beeper_in_attack = false;
		g_beeper_in_release = true;
	}
}

void handle_timers(EmulatorState *emulator) {
	if (emulator->dt > 0) {
		emulator->dt--;
	}
	if (emulator->st > 0) {
		emulator->st--;
		if (!g_debug) {
			beeper_state(true);
		}
	} else {
		beeper_state(false);
	}
}

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
	size_t config = emulator->configuration;

	memset(emulator, 0, sizeof(*emulator));

	emulator->configuration = config;

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

	emulator->pc = PROG_BASE;

	g_debug = false;
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
}

bool handle_input(EmulatorState *emulator) {
	SDL_Event e;
	nk_input_begin(g_nk_ctx);
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
		nk_sdl_handle_event(&e);
	}
	nk_input_end(g_nk_ctx);

	return true;
}

void render(EmulatorState *emulator) {
	if (g_debug) {
		if (nk_begin(g_nk_ctx, "Emulator Configuration", nk_rect(50, 50, 250, 150),
			     NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
				     NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE)) {
			nk_bool vf_reset = emulator->configuration & CONFIG_CHIP8_VF_RESET;
			nk_bool disp_wait = emulator->configuration & CONFIG_CHIP8_DISP_WAIT;
			nk_bool shifting = emulator->configuration & CONFIG_CHIP8_SHIFTING;
			nk_bool clipping = emulator->configuration & CONFIG_CHIP8_CLIPPING;
			nk_bool jumping = emulator->configuration & CONFIG_CHIP8_JUMPING;
			nk_bool memory = emulator->configuration & CONFIG_CHIP8_MEMORY;

			nk_layout_row_dynamic(g_nk_ctx, 30, 2);
			if (nk_checkbox_label(g_nk_ctx, "VF Reset", &vf_reset)) {
				emulator->configuration ^= CONFIG_CHIP8_VF_RESET;
			}
			if (nk_checkbox_label(g_nk_ctx, "Display Wait", &disp_wait)) {
				emulator->configuration ^= CONFIG_CHIP8_DISP_WAIT;
			}
			if (nk_checkbox_label(g_nk_ctx, "Clipping", &clipping)) {
				emulator->configuration ^= CONFIG_CHIP8_CLIPPING;
			}
			if (nk_checkbox_label(g_nk_ctx, "Shifting", &shifting)) {
				emulator->configuration ^= CONFIG_CHIP8_SHIFTING;
			}
			if (nk_checkbox_label(g_nk_ctx, "Jumping", &jumping)) {
				emulator->configuration ^= CONFIG_CHIP8_JUMPING;
			}
			if (nk_checkbox_label(g_nk_ctx, "Memory", &memory)) {
				emulator->configuration ^= CONFIG_CHIP8_MEMORY;
			}
		}
		nk_end(g_nk_ctx);
	}

	SDL_SetRenderTarget(g_renderer, g_texture);
	SDL_UpdateTexture(g_texture, NULL, emulator->display, TARGET_WIDTH * sizeof(uint32_t));

	SDL_SetRenderTarget(g_renderer, NULL);
	SDL_RenderClear(g_renderer);
	SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);

	nk_sdl_render(NK_ANTI_ALIASING_ON);

	SDL_RenderPresent(g_renderer);
}

void cleanup() {
	nk_sdl_shutdown();
	SDL_DestroyTexture(g_texture);
	SDL_DestroyRenderer(g_renderer);
	SDL_DestroyWindow(g_window);
	SDL_CloseAudioDevice(g_beeper_id);
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
		break;
	case CHIP8_RET:
		emulator->pc = emulator->stack[--emulator->sp];
		break;
	case CHIP8_SYS_ADDR:
		// Ignore
		fprintf(stderr, "[*] SYS attempt: 0x%04hx\n", instruction.raw);
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
		if (emulator->configuration & CONFIG_CHIP8_VF_RESET) {
			emulator->registers[0xF] = 0;
		}
		break;
	case CHIP8_AND_VX_VY:
		emulator->registers[instruction.rformat.rx] &=
			emulator->registers[instruction.rformat.ry];
		if (emulator->configuration & CONFIG_CHIP8_VF_RESET) {
			emulator->registers[0xF] = 0;
		}
		break;
	case CHIP8_XOR_VX_VY:
		emulator->registers[instruction.rformat.rx] ^=
			emulator->registers[instruction.rformat.ry];
		if (emulator->configuration & CONFIG_CHIP8_VF_RESET) {
			emulator->registers[0xF] = 0;
		}
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
		if (emulator->configuration & CONFIG_CHIP8_SHIFTING) {
			emulator->registers[instruction.rformat.rx] =
				emulator->registers[instruction.rformat.ry];
		}
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
		if (emulator->configuration & CONFIG_CHIP8_SHIFTING) {
			emulator->registers[instruction.rformat.rx] =
				emulator->registers[instruction.rformat.ry];
		}
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
		if (emulator->configuration & CONFIG_CHIP8_JUMPING) {
			emulator->pc = instruction.aformat.addr + emulator->registers[0];
		} else {
			emulator->pc = instruction.aformat.addr +
				       emulator->registers[instruction.iformat.reg];
		}
		break;
	case CHIP8_RND_VX_BYTE:
		emulator->registers[instruction.iformat.reg] = (rand() % 256) &
							       instruction.iformat.imm;
		break;
	case CHIP8_DRW_VX_VY_NIBBLE: {
		if (emulator->configuration & CONFIG_CHIP8_DISP_WAIT &&
		    !emulator->display_interrupted) {
			emulator->display_interrupted = true;
			emulator->pc -= 2;
			break;
		}
		bool flag = false;
		int origin_x = emulator->registers[instruction.rformat.rx] % TARGET_WIDTH;
		int origin_y = emulator->registers[instruction.rformat.ry] % TARGET_HEIGHT;
		int max_row = instruction.rformat.imm;
		int max_col = 8;

		if (emulator->configuration & CONFIG_CHIP8_CLIPPING) {
			max_row = origin_y + max_row > TARGET_HEIGHT ? TARGET_HEIGHT - origin_y :
								       max_row;
			max_col = origin_x + max_col > TARGET_WIDTH ? TARGET_WIDTH - origin_x :
								      max_col;
		}

		for (int row = 0; row < max_row; ++row) {
			uint8_t byte = emulator->memory[emulator->vi + row];
			int y = origin_y + row;
			for (uint8_t col = 0; col < max_col && byte; ++col) {
				int x = origin_x + col;
				uint8_t state = byte & 0x80;
				flag |= pixel(emulator, x, y) > 0 && state;
				pixel(emulator, x, y) ^= state ? PIXEL_COLOUR : 0;
				byte <<= 1;
			}
		}

		emulator->registers[0xF] = flag;
		emulator->display_interrupted = false;
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
		if (emulator->configuration & CONFIG_CHIP8_MEMORY) {
			emulator->vi = instruction.iformat.reg + 1;
		}
		break;
	case CHIP8_LD_VX_I:
		for (int i = 0; i <= instruction.iformat.reg; ++i) {
			emulator->registers[i] = emulator->memory[emulator->vi + i];
		}
		if (emulator->configuration & CONFIG_CHIP8_MEMORY) {
			emulator->vi = instruction.iformat.reg + 1;
		}
		break;
	case CHIP8_UNKNOWN:
		fprintf(stderr, "[!] Unknown instruction received: 0x%04hx @ 0x%03hx\n",
			instruction.raw, emulator->pc);
		return false;
	}

	return true;
}

void init_graphics() {
	SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
		fprintf(stderr, "[!] SDL could not initialise! SDL error: %s\n", SDL_GetError());
	} else {
		g_window = SDL_CreateWindow("EchoesOf8 - CHIP-8 Emulator", SDL_WINDOWPOS_UNDEFINED,
					    SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT,
					    SDL_WINDOW_SHOWN);
		if (g_window == NULL) {
			fprintf(stderr, "[!] Window could not be created! SDL error: %s\n",
				SDL_GetError());
		} else {
			int flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
			float font_scale = 1;

			g_renderer =
				SDL_CreateRenderer(g_window, -1, SDL_TEXTUREACCESS_TARGET | flags);

			// Scale for High-DPI displays
			{
				int render_w, render_h;
				int window_w, window_h;
				float scale_x, scale_y;
				SDL_GetRendererOutputSize(g_renderer, &render_w, &render_h);
				SDL_GetWindowSize(g_window, &window_w, &window_h);
				scale_x = (float)(render_w) / (float)(window_w);
				scale_y = (float)(render_h) / (float)(window_h);
				SDL_RenderSetScale(g_renderer, scale_x, scale_y);
				font_scale = scale_y;
			}

			g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888,
						      SDL_TEXTUREACCESS_TARGET, TARGET_WIDTH,
						      TARGET_HEIGHT);

			g_nk_ctx = nk_sdl_init(g_window, g_renderer);
			{
				struct nk_font_atlas *atlas;
				struct nk_font_config config = nk_font_config(0);
				struct nk_font *font;

				nk_sdl_font_stash_begin(&atlas);
				font = nk_font_atlas_add_default(atlas, 13 * font_scale, &config);
				nk_sdl_font_stash_end();

				font->handle.height /= font_scale;
				nk_style_set_font(g_nk_ctx, &font->handle);
			}
		}

		SDL_AudioSpec spec = { 0 };

		spec.freq = 44100;
		spec.samples = 2048;
		spec.channels = 1;
		spec.format = AUDIO_S16;
		spec.callback = beeper_callback;
		g_beeper_id = SDL_OpenAudioDevice(NULL, 0, &spec, &g_beeper_spec, 0);

		if (g_beeper_id == 0) {
			SDL_Log("Failed to open audio device: %s", SDL_GetError());
			// TODO: Handle error
		}
	}
}

void emulate(uint8_t *rom, size_t rom_size, bool debug) {
	printf("Emulating!\n");

	EmulatorState emulator = { 0 };
	emulator.configuration = CONFIG_CHIP8;

	srand(time(NULL));
	reset_state(&emulator);
	init_graphics();

	memcpy(emulator.memory + PROG_BASE, rom, rom_size);

	struct timespec start, current;
	long long frame_time = NANOSECONDS_PER_SECOND / TARGET_HZ;
	long long elapsed_time;

	if (debug) {
		g_debug = true;
		printf("Debugging enabled! Press <SPACE> pause/unpause.\n");
	}

	while (handle_input(&emulator)) {
		clock_gettime(CLOCK_MONOTONIC, &start);
		if (!g_debug) {
			for (int cycle = 0; cycle < CYCLES_PER_FRAME; ++cycle) {
				Chip8Instruction instruction = fetch_next(&emulator, false);
				if (!process_instruction(&emulator, instruction)) {
					dump_state(&emulator);
					g_debug = true;
					printf("\n[!] Something went wrong @ 0x%03hx: ",
					       emulator.pc - 2);
					print_asm(instruction);
					printf("\n");
				}
				if (emulator.display_interrupted) {
					break;
				}
			}

			handle_timers(&emulator);
		}

		render(&emulator);

		do { // Lock to TARGET_HZ
			clock_gettime(CLOCK_MONOTONIC, &current);
			elapsed_time = (current.tv_sec - start.tv_sec) * NANOSECONDS_PER_SECOND +
				       (current.tv_nsec - start.tv_nsec);
		} while (elapsed_time < frame_time);
	}

	cleanup();
}
