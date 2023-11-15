#include <stdio.h>
#include <stdlib.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

void hexdump(void *buffer, size_t length) {
	printf("00000000: ");
	for (int i = 0; i < length; ++i) {
		printf("%02hhx", ((char *)buffer)[i]);
		if ((i + 1) % 16 == 0) {
			printf("\n%08d: ", i + 1);
		} else if ((i + 1) % 2 == 0) {
			printf(" ");
		}
	}
	printf("\n");
}

void parse_rom(char *rom_path) {
	FILE *rom = fopen(rom_path, "rb");

	if (rom == NULL) {
		fprintf(stderr, "Error opening ROM file");
		exit(EXIT_FAILURE);
	}

	char buffer[1024];

	size_t ret = fread(buffer, sizeof(*buffer), ARRAY_SIZE(buffer), rom);
	if (ret == 0) {
		fprintf(stderr, "Reading ROM failed");
		exit(EXIT_FAILURE);
	}

	printf("ROM dump (%zu bytes):\n", ret);
	hexdump(buffer, ret);
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Usage: eo8 <rom>\n");
		return EXIT_FAILURE;
	}

	parse_rom(argv[1]);

	return EXIT_SUCCESS;
}
