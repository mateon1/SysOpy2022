// Mateusz Naściszewski, 2022
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>

static bool parse_int(char *str, long *out, char **end, bool whole) {
	if (*str == '\0') return false;
	char *endptr;
	errno = 0;
	long res = strtol(str, &endptr, 10);
	*out = res;
	if (end) *end = endptr;
	return errno == 0 && (!whole || *endptr == '\0');
}

enum partype {
	NUMBERS,
	BLOCK,
};

static bool parse_partype(char *str, enum partype *out) {
	if (strcmp(str, "num") == 0) {
		*out = NUMBERS;
		return true;
	}
	if (strcmp(str, "blk") == 0) {
		*out = BLOCK;
		return true;
	}
	return false;
}

bool readnum

int main(int argc, char** argv) {
	long threads;
	enum partype method;
	if (argc != 5 || !parse_int(argv[1], &threads, NULL, true) || threads < 1 || !parse_partype(argv[2], &method)) {
		printf("Usage: %s <input file>\n", argc > 0 ? argv[0] : "main");
		exit(2);
	}

	int infd = open(argv[3], O_RDONLY);
	if (infd < 0) {
		perror("Failed to open input file");
		exit(1);
	}
	int outfd = open(argv[4], O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (outfd < 0) {
		perror("Failed to open output file");
		exit(1);
	}

	char buf[4096];
	ssize_t buffered = 0;
	ssize_t nread;

	int state = 0; // 0: before header, 1: before width, 2: before height: 3: before maxval, 4: reading image data
	int width, height, maxval;
	bool binary;
	bool hasmaxval;
	bool channels;
	ssize_t imagewritten = 0;
	char *image;

	while ((nread = read(fd, buf+buffered, sizeof(buf)-buffered)) > 0) {
		buffered += nread;
		char *p = buf;
		int len = buffered;
		while (1) {
			switch (state) {
			case 0:
				if (len < 3)
					goto moredata;
				if (*p++ != 'P')
					goto badformat;
				switch (*p++) {
				case '1': binary = false; hasmaxval = false; maxval = 1; channels = 1; break;
				case '2': binary = false; hasmaxval = true; channels = 1; break;
				case '3': binary = false; hasmaxval = true; channels = 3; break;
				case '4': binary = true; hasmaxval = false; maxval = 1; channels = 1; break;
				case '5': binary = true; hasmaxval = true; channels = 1; break;
				case '6': binary = true; hasmaxval = true; channels = 3; break;
				default: goto badformat;
				}
				if (!iswhitespace(*p++)) goto badformat;
				break;
			case 1:
				char *rest;
				int val;
				if (!parse_int(p, &val, &rest, false)) goto badformat;
			}
		}
moredata:
		if (p > buf)
			memmove(buf, p, buffered - (p - buf));
		buffered -= discard;
	}

	if (nread < 0) {
		perror("Failed to read file");
		exit(1);
	}

	hash_clear(&ctx.definitions);

	return 0;

badformat:
	fprintf(stderr, "Input image is not a valid PNM file\n");
	exit(1);
}
