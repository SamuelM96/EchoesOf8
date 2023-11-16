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
	// https://github.com/mattmikolay/chip-8/wiki/CHIP%E2%80%908-Instruction-Set
	if (length % 2 != 0) {
		fprintf(stderr, "Cannot disassemble code: not aligned to 2 bytes");
		return;
	}

	for (size_t i = 0; i < length; i += 2) {
		// Instruction size = word = 2 bytes
		uint16_t instruction = (code[i] << 8) | code[i + 1];
		printf("0x%08zx  %04hx    ", base + i, instruction);

		// Parse instruction
		// TODO: Cleanup mnemonics
		switch (instruction >> 12) {
		case 0:
			switch (0xfff & instruction) {
			case 0x0E0:
				printf("clear");
				break;
			case 0x0EE:
				printf("ret");
				break;
			default:
				printf("callm %#03x", 0xfff & instruction);
				break;
			}
			break;
		case 1:
			printf("jmp %#03x", 0xfff & instruction);
			break;
		case 2:
			printf("call %#03x", 0xfff & instruction);
			break;
		case 3:
			printf("skipe v%d, %#02x", (0xf00 & instruction) >> 8, 0xff & instruction);
			break;
		case 4:
			printf("skipne v%d, %#02x", (0xf00 & instruction) >> 8, 0xff & instruction);
			break;
		case 5:
			printf("skipre v%d, v%d", (0xf00 & instruction) >> 8,
			       (0xf0 & instruction) >> 4);
			break;
		case 6:
			printf("store v%d, %#02x", (0xf00 & instruction) >> 8, 0xff & instruction);
			break;
		case 7:
			printf("add v%d, %#02x", (0xf00 & instruction) >> 8, 0xff & instruction);
			break;
		case 8:
			switch (0xf & instruction) {
			case 0:
				printf("store v%d, v%d", (0xf00 & instruction) >> 8,
				       (0xf0 & instruction) >> 4);
				break;
			case 1:
				printf("or v%d, v%d", (0xf00 & instruction) >> 8,
				       (0xf0 & instruction) >> 4);
				break;
			case 2:
				printf("and v%d, v%d", (0xf00 & instruction) >> 8,
				       (0xf0 & instruction) >> 4);
				break;
			case 3:
				printf("xor v%d, v%d", (0xf00 & instruction) >> 8,
				       (0xf0 & instruction) >> 4);
				break;
			case 4:
				// FIX: Does this store the value?
				printf("add v%d, v%d", (0xf00 & instruction) >> 8,
				       (0xf0 & instruction) >> 4);
				break;
			case 5:
				// FIX: Does this store the value?
				printf("sub v%d, v%d", (0xf00 & instruction) >> 8,
				       (0xf0 & instruction) >> 4);
				break;
			case 6:
				printf("shr v%d, v%d", (0xf00 & instruction) >> 8,
				       (0xf0 & instruction) >> 4);
				break;
			case 7:
				printf("sub v%d, v%d", (0xf00 & instruction) >> 8,
				       (0xf0 & instruction) >> 4);
				break;
			case 0xE:
				printf("shl v%d, v%d", (0xf00 & instruction) >> 8,
				       (0xf0 & instruction) >> 4);
				break;
			}
			break;
		case 9:
			printf("skip v%d, v%d", (0xf00 & instruction) >> 8,
			       (0xf0 & instruction) >> 4);
			break;
		case 0xA:
			printf("store I, %#03x", 0xfff & instruction);
			break;
		case 0xB:
			printf("jmp0 %#03x", 0xfff & instruction);
			break;
		case 0xC:
			printf("rand v%d, %#02x", (0xf00 & instruction) >> 8, 0xff & instruction);
			break;
		case 0xD:
			printf("draw v%d, v%d, %d", (0xf00 & instruction) >> 8,
			       (0xf0 & instruction) >> 4, 0xf & instruction);
			break;
		case 0xE:
			switch (0xff & instruction) {
			case 0x9E:
				printf("skipke v%d", (0xf00 & instruction) >> 8);
				break;
			case 0xA1:
				printf("skipkne v%d", (0xf00 & instruction) >> 8);
				break;
			default:
				printf("unknown");
				break;
			}
			break;
		case 0xF:
			switch (0xff & instruction) {
			case 0x07:
				printf("stored v%d", (0xf00 & instruction) >> 8);
				break;
			case 0x0A:
				printf("waitk v%d", (0xf00 & instruction) >> 8);
				break;
			case 0x15:
				printf("setd v%d", (0xf00 & instruction) >> 8);
				break;
			case 0x18:
				printf("sets v%d", (0xf00 & instruction) >> 8);
				break;
			case 0x1E:
				printf("addi v%d", (0xf00 & instruction) >> 8);
				break;
			case 0x29:
				printf("seti v%d", (0xf00 & instruction) >> 8);
				break;
			case 0x33:
				printf("storb v%d", (0xf00 & instruction) >> 8);
				break;
			case 0x55:
				printf("stori v%d", (0xf00 & instruction) >> 8);
				break;
			case 0x65:
				printf("fill v%d", (0xf00 & instruction) >> 8);
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
