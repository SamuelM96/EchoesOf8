#include "emulator.h"
#include "common.h"
#include "disassembler.h"
#include "instructions.h"
#include "sds.h"

#define NK_INCLUDE_STANDARD_BOOL
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
#include <SDL_rect.h>
#include <SDL_render.h>
#include <SDL_scancode.h>
#include <SDL_stdinc.h>
#include <SDL_surface.h>
#include <SDL_video.h>

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <time.h>
#include <unistd.h>

#define EMULATOR_MEMORY_SIZE 4096
#define EMULATOR_STACK_SIZE 16

#define CONFIG_CHIP8_VF_RESET 0b1
#define CONFIG_CHIP8_MEMORY 0b10
#define CONFIG_CHIP8_DISP_WAIT 0b100
#define CONFIG_CHIP8_CLIPPING 0b1000
#define CONFIG_CHIP8_SHIFTING 0b10000
#define CONFIG_CHIP8_JUMPING 0b100000

#define CONFIG_CHIP8 \
	CONFIG_CHIP8_VF_RESET | CONFIG_CHIP8_MEMORY | CONFIG_CHIP8_DISP_WAIT | \
		CONFIG_CHIP8_CLIPPING | CONFIG_CHIP8_SHIFTING | CONFIG_CHIP8_JUMPING

#define NANOSECONDS_PER_SECOND 1000000000
#define TARGET_HZ 60

#define PIXEL_COLOUR 0xFF97F1CD
#define pixel(emulator, x, y) (emulator)->display[(y) * TARGET_WIDTH + (x)]
#define FONT_BASE_ADDR 0x050

#define TARGET_WIDTH 64
#define TARGET_HEIGHT 32
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 640

typedef enum CyclesPerFrameType {
	CPF_7 = 0,
	CPF_10,
	CPF_15,
	CPF_20,
	CPF_30,
	CPF_100,
	CPF_200,
	CPF_500,
	CPF_1000,
} CyclesPerFrameType;
#define DEFAULT_CYCLES_PER_FRAME CPF_100

// TODO: Tidy up breakpoint handling - event based?
typedef struct DebugState {
	Disassembly disassembly;
	sds latest_memory_dump;
	bool memory_modifications[EMULATOR_MEMORY_SIZE];
	bool instruction_breakpoints[EMULATOR_MEMORY_SIZE];
	bool memory_breakpoints[EMULATOR_MEMORY_SIZE];

	bool debug_mode;
	bool written_to_memory;
	bool skip_breakpoints;
	bool inst_breakpoint_hit;
	bool memory_breakpoint_hit;
} DebugState;

typedef struct BeeperState {
	SDL_AudioSpec spec;
	SDL_AudioDeviceID id;
	double volume;
	double current_volume;
	double frequency;
	double phase;
	bool in_attack;
	bool in_release;
	bool on;
} Beeper;

typedef struct EmulatorState {
	// ROM to be loaded into RAM and executed
	char *rom_path;
	uint8_t *rom;
	size_t rom_size;

	// 0x000 - 0x1FF = Interpreter memory, not for programs
	// Programs start at 0x200 (512)
	// Some start at 0x600 (1536) (ETI 660 computer)
	uint8_t memory[EMULATOR_MEMORY_SIZE];

	// Stores return addresses
	// Allows for 16 levels of nested subroutines
	uint16_t stack[EMULATOR_STACK_SIZE];

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
	bool display_interrupted;

	// Keyboard state, 1 = Pressed
	uint8_t keyboard[16];

	// CHIP-8 vs SUPER-CHIP/CHIP-48 differences
	uint8_t configuration;

	CyclesPerFrameType cycles_per_frame;

	Beeper beeper;

	DebugState debug_state;
} EmulatorState;

const int SCALE_X = SCREEN_WIDTH / TARGET_WIDTH;
const int SCALE_Y = SCREEN_HEIGHT / TARGET_HEIGHT;
const int CYCLES_PER_FRAME[] = {
	[CPF_7] = 7,	 [CPF_10] = 10,	  [CPF_15] = 15,   [CPF_20] = 20,     [CPF_30] = 30,
	[CPF_100] = 100, [CPF_200] = 200, [CPF_500] = 500, [CPF_1000] = 1000,
};
const char *CYCLES_PER_FRAME_STR[] = {
	[CPF_7] = "7",	   [CPF_10] = "10",   [CPF_15] = "15",
	[CPF_20] = "20",   [CPF_30] = "30",   [CPF_100] = "100",
	[CPF_200] = "200", [CPF_500] = "500", [CPF_1000] = "1000",
};

bool g_show_debug_ui = false;
bool g_inside_text_input = false;

// SDL & Nuklear state
SDL_Window *g_window = NULL;
SDL_Renderer *g_renderer = NULL;
SDL_Texture *g_texture = NULL;
struct nk_context *g_ctx;

static void beeper_callback(void *, uint8_t *, int);
void beeper_toggle(Beeper *, bool);
void free_graphics(EmulatorState *);
static inline void dump_memory(EmulatorState *);
void dump_registers(EmulatorState *);
void dump_stack(EmulatorState *);
void dump_state(EmulatorState *);
bool execute(EmulatorState *, Chip8Instruction);
Chip8Instruction fetch_next(EmulatorState *, bool);
void free_emulator(EmulatorState *);
bool handle_input(EmulatorState *);
void handle_timers(EmulatorState *);
void init_beeper(Beeper *);
void init_graphics(EmulatorState *);
void load_rom(EmulatorState *, uint8_t *, size_t, char *);
void print_instruction_state(EmulatorState *, Chip8Instruction);
void refresh_dump(EmulatorState *);
void render(EmulatorState *);
void reset_state(EmulatorState *);
void update_keyboard_state(EmulatorState *, SDL_Scancode, uint8_t);

void emulate(uint8_t *rom, size_t rom_size, bool debug, char *rom_path) {
	printf("Emulating!\n");

	EmulatorState emulator = { 0 };
	emulator.configuration = CONFIG_CHIP8;
	emulator.cycles_per_frame = DEFAULT_CYCLES_PER_FRAME;

	srand(time(NULL));
	load_rom(&emulator, rom, rom_size, rom_path);
	init_graphics(&emulator);

	DebugState *debug_state = &emulator.debug_state;

	struct timespec start, current;
	long long frame_time = NANOSECONDS_PER_SECOND / TARGET_HZ;
	long long elapsed_time;

	if (debug) {
		debug_state->debug_mode = true;
		g_show_debug_ui = true;
		printf("Debugging enabled!\n");
		printf("  - <SPACE> to pause/unpause\n");
		printf("  - <H> to toggle debug UI\n");
		printf("  - <N> to step (execute the next instruction)\n");
	}

	bool running = true;
	while (running) {
		running = handle_input(&emulator);
		clock_gettime(CLOCK_MONOTONIC, &start);

		if (debug_state->memory_breakpoint_hit) {
			printf("Memory breakpoint hit!\n");
			debug_state->memory_breakpoint_hit = false;
		}

		if (!debug_state->debug_mode) {
			for (int cycle = 0;
			     cycle < CYCLES_PER_FRAME[emulator.cycles_per_frame] && running;
			     ++cycle) {
				running = handle_input(&emulator);
				if (!debug_state->skip_breakpoints &&
				    debug_state->instruction_breakpoints[emulator.pc] &&
				    !debug_state->inst_breakpoint_hit) {
					debug_state->inst_breakpoint_hit = true;
					debug_state->debug_mode = true;
					printf("Hit breakpoint @ 0x%03hx\n", emulator.pc);
					break;
				}

				debug_state->inst_breakpoint_hit = false;

				Chip8Instruction instruction = fetch_next(&emulator, false);
				if (!execute(&emulator, instruction)) {
					dump_state(&emulator);
					debug_state->debug_mode = true;
					printf("\n[!] Something went wrong @ 0x%03hx: ",
					       emulator.pc - 2);
					char *asm_str = inst2str(instruction);
					printf("%s\n", asm_str);
					free(asm_str);
				}
				if (emulator.display_interrupted ||
				    debug_state->memory_breakpoint_hit) {
					break;
				}
			}

			if (!debug_state->inst_breakpoint_hit) {
				handle_timers(&emulator);
			}
		}

		render(&emulator);

		do { // Lock to TARGET_HZ
			clock_gettime(CLOCK_MONOTONIC, &current);
			elapsed_time = (current.tv_sec - start.tv_sec) * NANOSECONDS_PER_SECOND +
				       (current.tv_nsec - start.tv_nsec);
		} while (elapsed_time < frame_time);
	}

	free_graphics(&emulator);
	free_emulator(&emulator);
}

Chip8Instruction fetch_next(EmulatorState *emulator, bool trace) {
	static uint16_t prev_inst_addr = 0;
	uint16_t addr = emulator->pc;

	DebugState *debug_state = &emulator->debug_state;

	Chip8Instruction instruction = bytes2inst(&emulator->memory[addr]);

	// TODO: Process modified instruction before breakpoint handling
	bool modified = debug_state->memory_modifications[addr];
	if (modified || addr != prev_inst_addr && trace) {
		AddressLookup *lookup =
			&debug_state->disassembly.addressbook[addr - debug_state->disassembly.base];
		DisassembledInstruction *disasm =
			&debug_state->disassembly.instruction_blocks[lookup->block_offset]
				 .instructions[lookup->array_offset];

		if (modified) {
			disasm->instruction = instruction;
			char *old_str = disasm->asm_str;
			disasm->asm_str = inst2str(instruction);

			printf("Modified instruction @ 0x%03hx:\n\t%s\n\t%s\n", addr, old_str,
			       disasm->asm_str);

			debug_state->memory_modifications[addr] = false;
			free(old_str);
		}

		if (trace) {
			printf("[0x%03hX] => (%04hX) %s\t", emulator->pc, disasm->instruction.raw,
			       disasm->asm_str);
			print_instruction_state(emulator, disasm->instruction);
			printf("\n");
			dump_state(emulator);
			printf("\n\n");
		}
	}

	prev_inst_addr = addr;
	emulator->pc += 2;

	return instruction;
}

bool execute(EmulatorState *emulator, Chip8Instruction instruction) {
	if (emulator->pc < 0 || emulator->pc > EMULATOR_MEMORY_SIZE) {
		fprintf(stderr, "[!] PC exceeds memory boundaries\n");
		return false;
	}

	DebugState *debug_state = &emulator->debug_state;

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
		debug_state->written_to_memory = true;
		for (int i = 0; i <= instruction.iformat.reg; ++i) {
			uint16_t addr = emulator->vi + i;
			emulator->memory[addr] = emulator->registers[i];
			debug_state->memory_modifications[addr] = true;
			if (debug_state->memory_breakpoints[addr]) {
				debug_state->debug_mode = true;
				debug_state->memory_breakpoint_hit = true;
			}
		}
		if (emulator->configuration & CONFIG_CHIP8_MEMORY) {
			emulator->vi = instruction.iformat.reg + 1;
		}
		break;
	case CHIP8_LD_VX_I:
		for (int i = 0; i <= instruction.iformat.reg; ++i) {
			uint16_t addr = emulator->vi + i;
			emulator->registers[i] = emulator->memory[addr];
			if (debug_state->memory_breakpoints[addr]) {
				debug_state->debug_mode = true;
				debug_state->memory_breakpoint_hit = true;
			}
		}
		if (emulator->configuration & CONFIG_CHIP8_MEMORY) {
			emulator->vi = instruction.iformat.reg + 1;
		}
		break;
	case CHIP8_UNKNOWN:
		// TODO: Handle CHIP-48 instructions
		fprintf(stderr, "[!] Unknown instruction received: 0x%04hx @ 0x%03hx\n",
			instruction.raw, emulator->pc);
		return false;
	}

	return true;
}

void handle_timers(EmulatorState *emulator) {
	// TODO: Handle timers in debug mode
	if (emulator->dt > 0) {
		emulator->dt--;
	}
	if (emulator->st > 0) {
		emulator->st--;
		if (!emulator->debug_state.debug_mode) {
			beeper_toggle(&emulator->beeper, true);
		}
	} else {
		beeper_toggle(&emulator->beeper, false);
	}
}

bool handle_input(EmulatorState *emulator) {
	SDL_Event e;
	nk_input_begin(g_ctx);
	while (SDL_PollEvent(&e)) {
		if (e.type == SDL_QUIT ||
		    e.type == SDL_KEYDOWN && e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
			printf("Quitting...\n");
			return false;
		}

		if (!g_inside_text_input) {
			DebugState *debug_state = &emulator->debug_state;

			switch (e.type) {
			case SDL_KEYUP: {
				update_keyboard_state(emulator, e.key.keysym.scancode, 0);
				break;
			}
			case SDL_KEYDOWN: {
				switch (e.key.keysym.scancode) {
				case SDL_SCANCODE_SPACE:
					debug_state->debug_mode = !debug_state->debug_mode;
					printf("%s emulator\n",
					       debug_state->debug_mode ? "Paused" : "Unpaused");
					break;
				case SDL_SCANCODE_G:
					debug_state->skip_breakpoints =
						!debug_state->skip_breakpoints;
					break;
				case SDL_SCANCODE_H:
					g_show_debug_ui = !g_show_debug_ui;
					break;
				case SDL_SCANCODE_N:
					if (debug_state->debug_mode) {
						execute(emulator, fetch_next(emulator, true));
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
		nk_sdl_handle_event(&e);
	}
	nk_input_end(g_ctx);

	return true;
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

void render(EmulatorState *emulator) {
	DebugState *debug_state = &emulator->debug_state;

	SDL_SetRenderDrawColor(g_renderer, 0x00, 0x05, 0x00, 0xFF);
	SDL_RenderClear(g_renderer);

	const int emu_scale = 10;
	const int emu_width = TARGET_WIDTH * emu_scale;
	const int emu_height = TARGET_HEIGHT * emu_scale;
	int emu_x = 0;
	int emu_y = 0;

	// TODO: Handle window resizing whilst maintaining aspect ratio of emulator
	// display
	// TODO: Scale and resize emulator display dynamically
	// TODO: Change pixel colours
	// TODO: Shows sprites in memory
	// TODO: Save and load emulator state (snapshots)
	// TODO: Call graph
	// TODO: Decompiler output
	// TODO: Audio waveform
	if (g_show_debug_ui) {
		const int window_flags = NK_WINDOW_BORDER | NK_WINDOW_TITLE |
					 NK_WINDOW_NO_SCROLLBAR;
		const int window_width = SCREEN_WIDTH;
		const int window_height = 800;
		SDL_SetWindowSize(g_window, window_width, window_height);

		struct nk_color active_colour = { 230, 150, 150, 255 };
		struct nk_color error_colour = { 255, 80, 80, 255 };
		struct nk_color pc_colour = { 80, 80, 85, 255 };

		// Bounding boxes
		struct nk_rect emu_config_rect = nk_rect(0, 0, 250, 250);
		struct nk_rect stack_rect =
			nk_rect(0, emu_config_rect.y + emu_config_rect.h, emu_config_rect.w, 310);
		struct nk_rect memory_rect = nk_rect(0, stack_rect.y + stack_rect.h, 630,
						     window_height - stack_rect.y - stack_rect.h);
		struct nk_rect registers_rect =
			nk_rect(emu_config_rect.x + emu_config_rect.w, emu_config_rect.y, 650, 239);
		struct nk_rect disasm_rect =
			nk_rect(registers_rect.x + registers_rect.w, registers_rect.y,
				window_width - registers_rect.x - registers_rect.w, window_height);
		struct nk_rect debug_rect = nk_rect(memory_rect.x + memory_rect.w, memory_rect.y,
						    disasm_rect.x - memory_rect.x - memory_rect.w,
						    memory_rect.h);

		emu_x = registers_rect.x + (registers_rect.w - emu_width) / 2;
		emu_y = registers_rect.y + registers_rect.h;

		int default_line_height = 30;

		if (nk_begin(g_ctx, "Emulator Configuration", emu_config_rect, window_flags)) {
			nk_bool vf_reset = emulator->configuration & CONFIG_CHIP8_VF_RESET;
			nk_bool disp_wait = emulator->configuration & CONFIG_CHIP8_DISP_WAIT;
			nk_bool shifting = emulator->configuration & CONFIG_CHIP8_SHIFTING;
			nk_bool clipping = emulator->configuration & CONFIG_CHIP8_CLIPPING;
			nk_bool jumping = emulator->configuration & CONFIG_CHIP8_JUMPING;
			nk_bool memory = emulator->configuration & CONFIG_CHIP8_MEMORY;

			nk_layout_row_dynamic(g_ctx, default_line_height, 2);
			if (nk_checkbox_label(g_ctx, "VF Reset", &vf_reset)) {
				emulator->configuration ^= CONFIG_CHIP8_VF_RESET;
			}
			if (nk_checkbox_label(g_ctx, "Display Wait", &disp_wait)) {
				emulator->configuration ^= CONFIG_CHIP8_DISP_WAIT;
			}
			if (nk_checkbox_label(g_ctx, "Clipping", &clipping)) {
				emulator->configuration ^= CONFIG_CHIP8_CLIPPING;
			}
			if (nk_checkbox_label(g_ctx, "Shifting", &shifting)) {
				emulator->configuration ^= CONFIG_CHIP8_SHIFTING;
			}
			if (nk_checkbox_label(g_ctx, "Jumping", &jumping)) {
				emulator->configuration ^= CONFIG_CHIP8_JUMPING;
			}
			if (nk_checkbox_label(g_ctx, "Memory", &memory)) {
				emulator->configuration ^= CONFIG_CHIP8_MEMORY;
			}

			int selected_cpf = emulator->cycles_per_frame;
			nk_layout_row_dynamic(g_ctx, default_line_height, 2);
			nk_label(g_ctx, "Cycles/frame", NK_TEXT_LEFT);
			nk_combobox(g_ctx, CYCLES_PER_FRAME_STR,
				    sizeof(CYCLES_PER_FRAME) / sizeof(int), &selected_cpf, 20,
				    nk_vec2(100, 225));
			emulator->cycles_per_frame = selected_cpf;

			nk_layout_row_dynamic(g_ctx, 10, 1);
			nk_spacer(g_ctx);

			nk_layout_row_dynamic(g_ctx, 20, 1);
			nk_label(g_ctx, "Volume", NK_TEXT_LEFT);
			static float volume = -1;
			if (volume == -1) {
				volume = (float)emulator->beeper.volume;
			}
			nk_slider_float(g_ctx, 0, &volume, 1, 0.1);
			emulator->beeper.volume = volume;
		}
		nk_end(g_ctx);

		if (nk_begin(g_ctx, "Stack", stack_rect, window_flags)) {
			nk_layout_row_dynamic(g_ctx, default_line_height, 2);
			for (int i = 0; i < EMULATOR_STACK_SIZE; ++i) {
				struct nk_color colour = g_ctx->style.text.color;
				if (emulator->sp == i) {
					g_ctx->style.text.color = active_colour;
				}
				nk_labelf(g_ctx, NK_TEXT_LEFT, "[%X] 0x%02hx", i,
					  emulator->stack[i]);
				g_ctx->style.text.color = colour;
			}
		}
		nk_end(g_ctx);

		if (nk_begin(g_ctx, "Memory", memory_rect, window_flags ^ NK_WINDOW_NO_SCROLLBAR)) {
			// TODO: Scroll memory into view when it hits a breakpoint
			// TODO: Jump to PC button
			// TODO: Jump to address from text field
			char byte_str[3] = { 0 };
			char ascii[17] = { 0 };
			int x = 0;
			int y = 10;
			int char_width = 11;
			int byte_width = 23;
			int addr_width = 50;
			int ascii_width = 130;
			int line_height = 30;
			char header_offsets[] = "0123456789ABCDEF";

			// Header
			nk_layout_space_begin(g_ctx, NK_STATIC, 0, 18);
			nk_layout_space_push(g_ctx, nk_rect(x, y, addr_width, line_height));
			nk_labelf(g_ctx, NK_TEXT_LEFT, "Addr");
			x += addr_width;
			for (int i = 0; i < 16; ++i) {
				nk_layout_space_push(g_ctx, nk_rect(x, y, byte_width, line_height));
				nk_text(g_ctx, header_offsets + i, 1, NK_TEXT_CENTERED);
				x += byte_width;
			}
			x += 10;
			nk_layout_space_push(g_ctx, nk_rect(x, y, 50, line_height));
			nk_labelf(g_ctx, NK_TEXT_LEFT, "ASCII");

			// Hexdump
			nk_layout_space_begin(g_ctx, NK_STATIC, 0, 33);
			for (uint16_t i = 0; i < EMULATOR_MEMORY_SIZE; ++i) {
				char byte = ((char *)emulator->memory)[i];
				if (isprint(byte)) {
					ascii[i % 16] = byte;
				} else {
					ascii[i % 16] = '.';
				}

				if (i % 16 == 0) {
					x = 0;
					nk_layout_space_push(g_ctx, nk_rect(x, y, addr_width,
									    line_height));
					nk_labelf(g_ctx, NK_TEXT_LEFT, "0x%03hx", i);
					x += addr_width;
				}

				snprintf(byte_str, sizeof(byte_str), "%02hx", byte);
				nk_layout_space_push(g_ctx, nk_rect(x, y, byte_width, line_height));
				nk_selectable_label(g_ctx, byte_str, NK_TEXT_CENTERED,
						    debug_state->memory_breakpoints + i);

				x += byte_width;

				if ((i + 1) % 16 == 0) {
					x += 10;
					for (int j = 0; j < sizeof(ascii) - 1; ++j) {
						nk_layout_space_push(g_ctx,
								     nk_rect(x, y, char_width,
									     line_height));
						nk_selectable_text(g_ctx, ascii + j, 1,
								   NK_TEXT_ALIGN_MIDDLE |
									   NK_TEXT_ALIGN_LEFT,
								   debug_state->memory_breakpoints +
									   i - 15 + j);
						x += char_width;
					}
				}
			}
			nk_layout_space_end(g_ctx);
		}
		nk_end(g_ctx);

		if (nk_begin(g_ctx, "Debug", debug_rect, window_flags)) {
			// TODO: Conditional breakpoints, e.g., break on all DRW
			// instructions
			// TODO: Step in and out of functions
			// TODO: Separate out debugging logic and state
			// TODO: Timeless debugging like rr
			nk_layout_row_dynamic(g_ctx, default_line_height, 2);
			if (nk_button_label(g_ctx, debug_state->debug_mode ? "Resume" : "Pause")) {
				debug_state->debug_mode = !debug_state->debug_mode;
			}
			if (nk_button_label(g_ctx, "Step")) {
				execute(emulator, fetch_next(emulator, true));
			}
			nk_layout_row_dynamic(g_ctx, default_line_height, 1);
			nk_checkbox_label(g_ctx, "Ignore BPs", &debug_state->skip_breakpoints);

			nk_layout_row_dynamic(g_ctx, default_line_height, 1);
			if (nk_button_label(g_ctx, "Reset")) {
				reset_state(emulator);
			}

			nk_layout_row_dynamic(g_ctx, 3, 1);
			nk_spacer(g_ctx);

			nk_layout_row_dynamic(g_ctx, default_line_height, 1);
			static char rom_path[256] = { 0 };
			static bool invalid = false;
			static char *error_message = "Invalid file path";
			if (!*rom_path && emulator->rom_path) {
				strncpy(rom_path, emulator->rom_path, sizeof(rom_path));
			}

			nk_flags flags = nk_edit_string_zero_terminated(
				g_ctx, NK_EDIT_SIMPLE, rom_path, sizeof(rom_path), NULL);

			g_inside_text_input = (flags & 0x1) > 0;

			if (nk_button_label(g_ctx, "Load ROM")) {
				if (*rom_path) {
					if (access(rom_path, F_OK) != -1) {
						size_t rom_size;
						uint8_t *rom = read_rom(rom_path, &rom_size);
						load_rom(emulator, rom, rom_size, rom_path);
						invalid = false;
					} else {
						invalid = true;
						if (errno == ENOENT) {
							error_message = "File does not exist";
						} else if (errno == EACCES) {
							error_message = "File is not accessible";
						} else {
							error_message = "Error accessing file";
						}
					}
				} else {
					invalid = true;
					error_message = "Please enter a file path";
				}
			}
			if (invalid) {
				struct nk_color colour = g_ctx->style.text.color;
				g_ctx->style.text.color = error_colour;
				nk_label(g_ctx, error_message, NK_TEXT_LEFT);
				g_ctx->style.text.color = colour;
			}
		}
		nk_end(g_ctx);

		if (nk_begin(g_ctx, "Registers", registers_rect, window_flags)) {
			nk_layout_row_dynamic(g_ctx, default_line_height, 4);
			for (int i = 0; i < sizeof(emulator->registers); ++i) {
				nk_labelf(g_ctx, NK_TEXT_LEFT, "V%X = 0x%02hx", i,
					  emulator->registers[i]);
			}
			nk_layout_row_dynamic(g_ctx, default_line_height, 4);
			nk_labelf(g_ctx, NK_TEXT_LEFT, "PC = 0x%04hx", emulator->pc);
			nk_labelf(g_ctx, NK_TEXT_LEFT, "VI = 0x%04hx", emulator->vi);
			nk_labelf(g_ctx, NK_TEXT_LEFT, "SP = 0x%02hx", emulator->sp);
			nk_layout_row_dynamic(g_ctx, default_line_height, 4);
			nk_labelf(g_ctx, NK_TEXT_LEFT, "ST = 0x%02hx", emulator->st);
			nk_labelf(g_ctx, NK_TEXT_LEFT, "DT = 0x%02hx", emulator->dt);
		}
		nk_end(g_ctx);

		if (nk_begin(g_ctx, "Disassembly", disasm_rect,
			     window_flags ^ NK_WINDOW_NO_SCROLLBAR)) {
			// TODO: Replace with a list view-like setup. Nuklear groups?
			struct nk_window *win = g_ctx->current;
			nk_layout_row_dynamic(g_ctx, 20, 1);
			char text[64] = { 0 };
			g_ctx->style.selectable.text_normal_active = active_colour;
			g_ctx->style.selectable.text_hover_active = active_colour;
			g_ctx->style.selectable.text_hover = active_colour;

			size_t skipped = 0;
			bool blanked = false;
			for (uint16_t ip = 0; ip < debug_state->disassembly.abook_length; ++ip) {
				AddressLookup *lookup = &debug_state->disassembly.addressbook[ip];
				if (lookup->type != ADDR_INSTRUCTION) {
					skipped++;
					continue;
				}

				if (skipped > 1 && !blanked) {
					blanked = true;
					nk_labelf(g_ctx, NK_TEXT_LEFT,
						  "===== 0x%03zx BYTES OF DATA =====",
						  skipped - 1); // Ignore INST_HALF
				}

				blanked = false;
				skipped = 0;

				DisassembledInstruction *instruction =
					&debug_state->disassembly
						 .instruction_blocks[lookup->block_offset]
						 .instructions[lookup->array_offset];

				static uint16_t prev_pc = 0;
				snprintf(text, sizeof(text), "0x%08hx  %s",
					 instruction->address + debug_state->disassembly.base,
					 instruction->asm_str);

				// Highlight instructions with breakpoints
				struct nk_color colour = g_ctx->style.selectable.normal.data.color;
				if (emulator->pc ==
				    instruction->address + debug_state->disassembly.base) {
					g_ctx->style.selectable.normal.data.color = pc_colour;

					if (emulator->pc != prev_pc) {
						long target =
							(int)win->layout->at_y - SCREEN_HEIGHT / 2;
						target = target < 0 ? 0 : target;
						win->scrollbar.y = target;
						prev_pc = emulator->pc;
					}
				}

				uint16_t addr = debug_state->disassembly.base + ip;
				if (nk_selectable_label(g_ctx, text, NK_TEXT_LEFT,
							debug_state->instruction_breakpoints +
								addr)) {
					printf("Breakpoint %s @ 0x%hx!\n",
					       debug_state->instruction_breakpoints[addr] ?
						       "enabled" :
						       "disabled",
					       addr);
					fflush(stdout);
				}
				g_ctx->style.selectable.normal.data.color = colour;
			}
		}
		nk_end(g_ctx);
	} else {
		SDL_SetWindowSize(g_window, SCREEN_WIDTH, SCREEN_HEIGHT);
	}

	SDL_SetRenderTarget(g_renderer, g_texture);
	SDL_UpdateTexture(g_texture, NULL, emulator->display, TARGET_WIDTH * sizeof(uint32_t));

	SDL_SetRenderTarget(g_renderer, NULL);
	SDL_RenderClear(g_renderer);
	if (g_show_debug_ui) {
		int window_w, window_h;
		SDL_GetWindowSize(g_window, &window_w, &window_h);
		SDL_Rect display_rect = { emu_x, emu_y, emu_width, emu_height };
		SDL_RenderCopy(g_renderer, g_texture, NULL, &display_rect);
	} else {
		SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);
	}

	nk_sdl_render(NK_ANTI_ALIASING_ON);

	SDL_RenderPresent(g_renderer);
}

static void beeper_callback(void *userdata, uint8_t *_stream, int _len) {
	Beeper *beeper = (Beeper *)userdata;

	const double attack_release_time = 0.005;

	int16_t *stream = (int16_t *)_stream;
	int len = _len / sizeof(int16_t);

	double attack_release_delta = beeper->volume / (attack_release_time * beeper->spec.freq);
	double phase_increment = 2.0 * beeper->frequency / beeper->spec.freq;

	for (int sample = 0; sample < len; ++sample) {
		beeper->phase += phase_increment;
		if (beeper->phase >= 1.0)
			beeper->phase -= 2.0;

		if (beeper->in_attack) {
			beeper->current_volume += attack_release_delta;
			if (beeper->current_volume >= beeper->volume) {
				beeper->current_volume = beeper->volume;
				beeper->in_attack = false;
			}
		}

		if (beeper->in_release) {
			beeper->current_volume -= attack_release_delta;
			if (beeper->current_volume <= 0.0) {
				beeper->current_volume = 0.0;
				beeper->in_release = false;
				SDL_PauseAudioDevice(beeper->id, 1);
			}
		}

		// Sawtooth beeper
		stream[sample] = beeper->phase * beeper->current_volume * INT16_MAX / 2.0;
	}
}

void beeper_toggle(Beeper *beeper, bool on) {
	if (!beeper->on && on) {
		beeper->on = true;
		beeper->in_attack = true;
		beeper->in_release = false;
		beeper->current_volume = 0.0;
		SDL_PauseAudioDevice(beeper->id, 0);
	} else if (beeper->on && !on) {
		beeper->on = false;
		beeper->in_attack = false;
		beeper->in_release = true;
	}
}

void load_rom(EmulatorState *emulator, uint8_t *rom, size_t rom_size, char *rom_path) {
	printf("[*] Loading ROM @ %s\n", rom_path);
	if (emulator->rom) {
		free(emulator->rom);
	}
	if (emulator->rom_path) {
		free(emulator->rom_path);
	}

	if (rom_path) {
		size_t path_len = strlen(rom_path);
		emulator->rom_path = malloc(path_len);
		memcpy(emulator->rom_path, rom_path, path_len);
	} else {
		emulator->rom_path = NULL;
	}

	emulator->rom = rom;
	emulator->rom_size = rom_size;

	reset_state(emulator);
}

void init_graphics(EmulatorState *emulator) {
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
			float font_scale = 1;

			g_renderer = SDL_CreateRenderer(g_window, -1,
							SDL_RENDERER_ACCELERATED |
								SDL_RENDERER_PRESENTVSYNC |
								SDL_TEXTUREACCESS_TARGET);

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

			g_ctx = nk_sdl_init(g_window, g_renderer);
			{
				struct nk_font_atlas *atlas;
				struct nk_font_config config = nk_font_config(0);
				struct nk_font *font;

				nk_sdl_font_stash_begin(&atlas);
				font = nk_font_atlas_add_default(atlas, 15 * font_scale, &config);
				nk_sdl_font_stash_end();

				font->handle.height /= font_scale;
				nk_style_set_font(g_ctx, &font->handle);
			}

			init_beeper(&emulator->beeper);
		}
	}
}

void init_beeper(Beeper *beeper) {
	beeper->volume = 0.1;
	beeper->current_volume = 0.0;
	beeper->frequency = 261.63;
	beeper->phase = 0.0;
	beeper->in_attack = false;
	beeper->in_release = false;
	beeper->on = false;

	SDL_AudioSpec spec = {
		.freq = 44100,
		.samples = 2048,
		.channels = 1,
		.format = AUDIO_S16,
		.callback = beeper_callback,
		.userdata = beeper,
	};

	beeper->id = SDL_OpenAudioDevice(NULL, 0, &spec, &beeper->spec, 0);

	if (beeper->id == 0) {
		SDL_Log("Failed to open audio device: %s", SDL_GetError());
		// TODO: Handle error
	}
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
	for (int i = 0; i < EMULATOR_STACK_SIZE; ++i) {
		fprintf(stderr, "[%02hhd] = 0x%03hx  ", i, emulator->stack[i]);
		if (i == emulator->sp) {
			printf("<-- SP  ");
		}

		if ((i + 1) % 2 == 0) {
			printf("\n");
		}
	}
}

static inline void dump_memory(EmulatorState *emulator) {
	fprintf(stderr, "\n===== MEMORY DUMP ====\n");
	if (!emulator->debug_state.latest_memory_dump) {
		refresh_dump(emulator);
	}
	printf("%s\n", emulator->debug_state.latest_memory_dump);
}

void refresh_dump(EmulatorState *emulator) {
	DebugState *debug_state = &emulator->debug_state;
	if (debug_state->written_to_memory || !debug_state->latest_memory_dump) {
		if (debug_state->latest_memory_dump) {
			sdsfree(debug_state->latest_memory_dump);
		}
		debug_state->latest_memory_dump =
			hexdump(emulator->memory, EMULATOR_MEMORY_SIZE, 0);
		debug_state->written_to_memory = false;
	}
}

void dump_state(EmulatorState *emulator) {
	dump_registers(emulator);
	dump_stack(emulator);
	// dump_memory(emulator);
}

void reset_state(EmulatorState *emulator) {
	free_disassembly(&emulator->debug_state.disassembly);

	char *rom_path = emulator->rom_path;
	uint8_t *rom = emulator->rom;
	size_t rom_size = emulator->rom_size;
	size_t config = emulator->configuration;
	size_t cycles_per_frame = emulator->cycles_per_frame;

	memset(emulator, 0, sizeof(*emulator));

	emulator->rom_path = rom_path;
	emulator->rom = rom;
	emulator->rom_size = rom_size;
	emulator->configuration = config;
	emulator->cycles_per_frame = cycles_per_frame;
	emulator->pc = PROG_BASE;

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

	memcpy(emulator->memory + PROG_BASE, emulator->rom, emulator->rom_size);

	emulator->debug_state.written_to_memory = false;
	emulator->debug_state.latest_memory_dump =
		hexdump(emulator->memory, EMULATOR_MEMORY_SIZE, 0);
	emulator->debug_state.disassembly = disassemble_rd(
		emulator->memory + PROG_BASE, EMULATOR_MEMORY_SIZE - PROG_BASE, PROG_BASE);
}

void free_emulator(EmulatorState *emulator) {
	if (emulator->rom) {
		free(emulator->rom);
	}
}

void free_graphics(EmulatorState *emulator) {
	nk_sdl_shutdown();
	SDL_DestroyTexture(g_texture);
	SDL_DestroyRenderer(g_renderer);
	SDL_DestroyWindow(g_window);
	SDL_CloseAudioDevice(emulator->beeper.id);
	SDL_Quit();
}
