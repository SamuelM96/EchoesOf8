#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define PROG_BASE 0x200

void hexdump(void *buffer, size_t length, size_t base) {
	printf("%08zx: ", base);
	for (size_t i = 0; i < length; ++i) {
		printf("%02hhx", ((char *)buffer)[i]);
		if ((i + 1) % 16 == 0) {
			printf("\n%08zx: ", base + i + 1);
		} else if ((i + 1) % 2 == 0) {
			printf(" ");
		}
	}
	printf("\n");
}

// Linear sweep
void disassemble_linear(uint8_t *code, size_t length, size_t base) {
	// References:
	// - https://github.com/mattmikolay/chip-8/wiki/CHIP%E2%80%908-Instruction-Set
	// - http://devernay.free.fr/hacks/chip8/C8TECH10.HTM

	if (length % 2 != 0) {
		fprintf(stderr, "Cannot disassemble code: not aligned to 2 bytes");
		return;
	}

	for (size_t i = 0; i < length; i += 2) {
		// Instruction size = word = 2 bytes
		uint16_t instruction = (code[i] << 8) | code[i + 1];
		printf("0x%08zx  %04hx    ", base + i, instruction);

		// Parse instruction
		switch (instruction >> 12) {
		case 0:
			switch (0xfff & instruction) {
			case 0x0E0:
				printf("CLS");
				break;
			case 0x0EE:
				printf("RET");
				break;
			default:
				printf("SYS %#03x", 0xfff & instruction);
				break;
			}
			break;
		case 1:
			printf("JMP %#03x", 0xfff & instruction);
			break;
		case 2:
			printf("CALL %#03x", 0xfff & instruction);
			break;
		case 3:
			printf("SE V%d, %#02x", (0xf00 & instruction) >> 8, 0xff & instruction);
			break;
		case 4:
			printf("SNE V%d, %#02x", (0xf00 & instruction) >> 8, 0xff & instruction);
			break;
		case 5:
			printf("SE V%d, V%d", (0xf00 & instruction) >> 8,
			       (0xf0 & instruction) >> 4);
			break;
		case 6:
			printf("LD V%d, %#02x", (0xf00 & instruction) >> 8, 0xff & instruction);
			break;
		case 7:
			printf("ADD V%d, %#02x", (0xf00 & instruction) >> 8, 0xff & instruction);
			break;
		case 8:
			switch (0xf & instruction) {
			case 0:
				printf("LD V%d, V%d", (0xf00 & instruction) >> 8,
				       (0xf0 & instruction) >> 4);
				break;
			case 1:
				printf("OR V%d, V%d", (0xf00 & instruction) >> 8,
				       (0xf0 & instruction) >> 4);
				break;
			case 2:
				printf("AND V%d, V%d", (0xf00 & instruction) >> 8,
				       (0xf0 & instruction) >> 4);
				break;
			case 3:
				printf("XOR V%d, V%d", (0xf00 & instruction) >> 8,
				       (0xf0 & instruction) >> 4);
				break;
			case 4:
				printf("ADD V%d, V%d", (0xf00 & instruction) >> 8,
				       (0xf0 & instruction) >> 4);
				break;
			case 5:
				printf("SUB V%d, V%d", (0xf00 & instruction) >> 8,
				       (0xf0 & instruction) >> 4);
				break;
			case 6:
				printf("SHR V%d, V%d", (0xf00 & instruction) >> 8,
				       (0xf0 & instruction) >> 4);
				break;
			case 7:
				printf("SUBN V%d, V%d", (0xf00 & instruction) >> 8,
				       (0xf0 & instruction) >> 4);
				break;
			case 0xE:
				printf("SHL V%d, V%d", (0xf00 & instruction) >> 8,
				       (0xf0 & instruction) >> 4);
				break;
			}
			break;
		case 9:
			printf("SNE V%d, V%d", (0xf00 & instruction) >> 8,
			       (0xf0 & instruction) >> 4);
			break;
		case 0xA:
			printf("LD I, %#03x", 0xfff & instruction);
			break;
		case 0xB:
			printf("JMP V0, %#03x", 0xfff & instruction);
			break;
		case 0xC:
			printf("RND V%d, %#02x", (0xf00 & instruction) >> 8, 0xff & instruction);
			break;
		case 0xD:
			printf("DRW V%d, V%d, %d", (0xf00 & instruction) >> 8,
			       (0xf0 & instruction) >> 4, 0xf & instruction);
			break;
		case 0xE:
			switch (0xff & instruction) {
			case 0x9E:
				printf("SKP V%d", (0xf00 & instruction) >> 8);
				break;
			case 0xA1:
				printf("SKNP V%d", (0xf00 & instruction) >> 8);
				break;
			default:
				printf("unknown");
				break;
			}
			break;
		case 0xF:
			switch (0xff & instruction) {
			case 0x07:
				printf("LD V%d", (0xf00 & instruction) >> 8);
				break;
			case 0x0A:
				printf("LD V%d", (0xf00 & instruction) >> 8);
				break;
			case 0x15:
				printf("LD DT, V%d", (0xf00 & instruction) >> 8);
				break;
			case 0x18:
				printf("LD ST, V%d", (0xf00 & instruction) >> 8);
				break;
			case 0x1E:
				printf("ADD I, V%d", (0xf00 & instruction) >> 8);
				break;
			case 0x29:
				printf("LD F, V%d", (0xf00 & instruction) >> 8);
				break;
			case 0x33:
				printf("LD B, V%d", (0xf00 & instruction) >> 8);
				break;
			case 0x55:
				printf("LD [I], V%d", (0xf00 & instruction) >> 8);
				break;
			case 0x65:
				printf("LD V%d, [I]", (0xf00 & instruction) >> 8);
				break;
			default:
				printf("unknown");
				break;
			}
			break;
		default:
			printf("unknown");
			break;
		}
		printf("\n");
	}
}

void parse_rom(char *rom_path) {
	FILE *rom = fopen(rom_path, "rb");

	if (rom == NULL) {
		fprintf(stderr, "Error opening ROM file");
		exit(EXIT_FAILURE);
	}

	uint8_t buffer[4096];

	printf("ROM dump\n");
	size_t ret, base = PROG_BASE;
	while ((ret = fread(buffer, sizeof(*buffer), ARRAY_SIZE(buffer), rom))) {
		// hexdump(buffer, ret, base);
		disassemble_linear(buffer, ret, base);
		base += ret;
	}
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Usage: eo8 <rom>\n");
		return EXIT_FAILURE;
	}

	parse_rom(argv[1]);

	return EXIT_SUCCESS;
}
