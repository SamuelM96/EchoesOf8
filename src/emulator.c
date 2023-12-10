#include "emulator.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_surface.h>
#include <SDL2/SDL_video.h>
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
uint16_t EMULATOR_PC;

// Delay timer
uint8_t EMULATOR_DT;

// Sound timer
uint8_t EMULATOR_ST;

// Display pixels
uint32_t EMULATOR_DISPLAY[TARGET_WIDTH * TARGET_HEIGHT];

void reset_state() {
	memset(EMULATOR_MEMORY, 0, sizeof(EMULATOR_MEMORY));
	memset(EMULATOR_STACK, 0, sizeof(EMULATOR_STACK));
	memset(EMULATOR_REGISTERS, 0, sizeof(EMULATOR_REGISTERS));
	memset(EMULATOR_DISPLAY, 0, sizeof(EMULATOR_DISPLAY));

	EMULATOR_SP = 0;
	EMULATOR_VI = 0;
	EMULATOR_PC = 0;
	EMULATOR_DT = 0;
	EMULATOR_ST = 0;
}

static inline void draw(int x, int y) {
	EMULATOR_DISPLAY[y * TARGET_WIDTH + x] = 0xFF97F1CD;
}

void emulate(uint8_t *rom, size_t rom_size) {
	printf("Emulating!\n");
	reset_state();

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "[!] SDL could not initialise! SDL error: %s\n", SDL_GetError());
	} else {
		SDL_Window *window = SDL_CreateWindow("EchoesOf8 - CHIP-8 Emulator",
						      SDL_WINDOWPOS_UNDEFINED,
						      SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH,
						      SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
		if (window == NULL) {
			fprintf(stderr, "[!] Window could not be created! SDL error: %s\n",
				SDL_GetError());
		} else {
			SDL_Renderer *renderer =
				SDL_CreateRenderer(window, -1, SDL_TEXTUREACCESS_TARGET);

			SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
								 SDL_TEXTUREACCESS_TARGET,
								 TARGET_WIDTH, TARGET_HEIGHT);

			SDL_Event e;
			bool quit = false;
			bool left_mouse_btn_down = false;
			while (!quit) {
				SDL_SetRenderTarget(renderer, texture);
				SDL_UpdateTexture(texture, NULL, EMULATOR_DISPLAY,
						  TARGET_WIDTH * sizeof(uint32_t));

				while (SDL_PollEvent(&e)) {
					switch (e.type) {
					case SDL_QUIT:
						quit = true;
						break;
					case SDL_MOUSEBUTTONUP:
						if (e.button.button == SDL_BUTTON_LEFT) {
							left_mouse_btn_down = false;
						}
						break;
					case SDL_MOUSEBUTTONDOWN:
						if (e.button.button == SDL_BUTTON_LEFT) {
							left_mouse_btn_down = true;
							draw(e.motion.x / SCALE_X,
							     e.motion.y / SCALE_Y);
						}
						break;
					case SDL_MOUSEMOTION:
						if (left_mouse_btn_down) {
							draw(e.motion.x / SCALE_X,
							     e.motion.y / SCALE_Y);
						}
						break;
					}
				}

				SDL_SetRenderTarget(renderer, NULL);
				SDL_RenderClear(renderer);
				SDL_RenderCopy(renderer, texture, NULL, NULL);
				SDL_RenderPresent(renderer);
			}

			SDL_DestroyTexture(texture);
			SDL_DestroyRenderer(renderer);
			SDL_DestroyWindow(window);
			SDL_Quit();
		}
	}
}
