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
#include <time.h>
#include <ctype.h>

static void writeall(int fd, char *buf, size_t size) {
	ssize_t ret;
retry:
	while ((ret = write(fd, buf, size)) > 0 && size > 0) {
		buf += ret; size -= ret;
	}
	if (ret < 0) {
		if (errno == EINTR && size > 0) goto retry;
		perror("Failed to write data");
		exit(1);
	}
	assert(size == 0);
}

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

	struct timespec start;
	struct timespec start_cpu;
	if (clock_gettime(CLOCK_MONOTONIC_RAW, &start) < 0) { perror("Failed to get time"); exit(1); }
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_cpu) < 0) { perror("Failed to get time"); exit(1); }

	char buf[4097];
	ssize_t buffered = 0;
	ssize_t nread;

	int state = 0; // 0: before header, 1: before width, 2: before height: 3: before maxval, 4: reading image data
	int width = 1, height = 1, maxval = 1;
	bool binary = true;
	bool bit = false;
	bool hasmaxval = true;
	int channels = 1;
	ssize_t imagewritten = 0;
	char *image = NULL;

	while ((nread = read(infd, buf+buffered, sizeof(buf)-buffered-1)) > 0) {
		buffered += nread;
		int len = buffered;
		char *p = buf;
		char *end = p + len;
		p[len] = '\0';
		while (1) {
			//printf("state %d written %ld\n", state, imagewritten);
			switch (state) {
			case 0:
				if (len < 3)
					goto moredata;
				if (*p++ != 'P')
					goto badformat;
				switch (*p++) {
				case '1': binary = false; hasmaxval = false; break;
				case '2': binary = false; break;
				case '3': binary = false; channels = 3; break;
				case '4': bit = true; hasmaxval = false; break;
				case '5': binary = true; break;
				case '6': binary = true; channels = 3; break;
				default: goto badformat;
				}
				if (!isspace(*p++)) goto badformat;
				state = 1;
				break;
			case 1: case 2: case 3:
				char *rest;
				long val;
				if (!parse_int(p, &val, &rest, false)) goto badformat;
				if (rest >= end) goto moredata;
				if (val <= 0 || !isspace(*rest++)) goto badformat;
				p = rest;
				if (state == 1)
					width = val;
				else if (state == 2)
					height = val;
				else if (state == 3)
					maxval = val;
				state++;
				if (state == 3 && !hasmaxval) state++;
				if (state == 4) {
					image = malloc(width * height * channels * (maxval <= 255 ? 1 : 2));
				}
				break;
			case 4:
#define CHK(n) do { if (p + (n) > end) goto moredata; } while (0);
				int lim = width * height * channels * (maxval <= 255 ? 1 : 2);
				while (imagewritten < lim) {
					//if (end - rest < 10) printf("state %d written %ld, %s\n", state, imagewritten, p);
					long val;
					if (bit) {
						CHK(1);
						val = (*p) & 1;
						*p >>= 1;
						if ((imagewritten&7)==7) p++;
					} else if (binary) {
						CHK(maxval > 255 ? 2 : 1);
						val = (unsigned char)*p++;
						if (maxval > 255) { val <<= 8; val |= (unsigned char)*p++; }
					} else {
						while (isspace(*p)) { p++; CHK(2); }
						if (!parse_int(p, &val, &rest, false)) goto badformat;
						if (rest >= end) goto moredata;
						if (val < 0 || rest == p) goto badformat;
						p = rest;
					}
					assert(val >= 0);
					if (val > maxval) goto badformat;
					if (maxval > 255) {
						image[imagewritten++] = val >> 8;
						image[imagewritten++] = val & 255;
					} else {
						image[imagewritten++] = val;
					}
				}
				goto readfinish;
			default:
				assert(false && "Impossible case");
				_exit(1);
			}
		}
moredata:
		if (p > buf)
			memmove(buf, p, buffered - (p - buf));
		buffered -= p - buf;
	}

	if (nread < 0) {
		perror("Failed to read file");
		exit(1);
	}

	fprintf(stderr, "File ended prematurely\n");
	exit(1);

badformat:
	fprintf(stderr, "Input image is not a valid PNM file\n");
	exit(1);

readfinish:
	struct timespec read;
	struct timespec read_cpu;
	if (clock_gettime(CLOCK_MONOTONIC_RAW, &read) < 0) { perror("Failed to get time"); exit(1); }
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &read_cpu) < 0) { perror("Failed to get time"); exit(1); }

	printf("START    : %10jd.%09ld\n", start.tv_sec, start.tv_nsec);
	printf("START_CPU: %10jd.%09ld\n", start_cpu.tv_sec, start_cpu.tv_nsec);
	printf("READ     : %10jd.%09ld\n", read.tv_sec, read.tv_nsec);
	printf("READ_CPU : %10jd.%09ld\n", read_cpu.tv_sec, read_cpu.tv_nsec);

	// TODO: Threads: invert
	for (int i = 0; i < imagewritten;) {
		if (maxval <= 255) {
			image[i] = maxval - image[i];
			i++;
		} else {
			int val = maxval - ((image[i] << 8) | image[i+1]);
			image[i++] = val >> 8;
			image[i++] = val & 255;
		}
	}

	struct timespec invert;
	struct timespec invert_cpu;
	if (clock_gettime(CLOCK_MONOTONIC_RAW, &invert) < 0) { perror("Failed to get time"); exit(1); }
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &invert_cpu) < 0) { perror("Failed to get time"); exit(1); }
	printf("INV      : %10jd.%09ld\n", invert.tv_sec, invert.tv_nsec);
	printf("INV_CPU  : %10jd.%09ld\n", invert_cpu.tv_sec, invert_cpu.tv_nsec);

	char header[32];
	if (bit) { binary = true; bit = false; hasmaxval = true; } // P4 -> P5
	int headerbytes = sprintf(header, "P%c\n%d %d", (bit || binary ? 3 : 0) + (!hasmaxval ? '1' : channels == 1 ? '2' : '3'), width, height);
	if (hasmaxval) headerbytes += sprintf(header + headerbytes, " %d", maxval);
	sprintf(header + headerbytes, "\n");
	writeall(outfd, header, strlen(header));
	if (binary) {
		writeall(outfd, image, imagewritten);
	} else {
		char buf[12];
		int column = 0;
		for (int i = 0; i < imagewritten; ) {
			int val;
			if (maxval <= 255) {
				val = image[i];
				i++;
			} else {
				val = (image[i] << 8) | image[i+1];
				i++; i++;
			}
			bool nl = column > 160 || i == imagewritten;
			column += sprintf(buf, "%d%c", val, nl ? '\n': ' ');
			if (nl) column = 0;
		}
	}

	struct timespec write;
	struct timespec write_cpu;
	if (clock_gettime(CLOCK_MONOTONIC_RAW, &write) < 0) { perror("Failed to get time"); exit(1); }
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &write_cpu) < 0) { perror("Failed to get time"); exit(1); }
	printf("WRITE    : %10jd.%09ld\n", write.tv_sec, write.tv_nsec);
	printf("WRITE_CPU: %10jd.%09ld\n", write_cpu.tv_sec, write_cpu.tv_nsec);

	return 0;
}
