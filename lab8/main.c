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
#include <pthread.h>

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

static bool parse_int(const char *str, long *out, char **end, bool whole) {
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

static bool parse_partype(const char *str, enum partype *out) {
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

static int sprint_dur(char *buf, struct timespec from, struct timespec to) {
	long secs = to.tv_sec - from.tv_sec, nanos = to.tv_nsec - from.tv_nsec;
	if (nanos < 0) {
		secs--;
		nanos += 1000000000;
	}
	if (secs == 0 && nanos < 10000) {
		return sprintf(buf, "%ld nanos", nanos);
	} else if (secs == 0 && nanos < 10000000) {
		return sprintf(buf, "%ld.%ld micros", nanos / 1000, nanos / 100 % 10);
	} else {
		long millis = 1000 * secs + nanos / 1000000;
		return sprintf(buf, "%ld.%02ld millis", millis, nanos/10000%100);
	}
}

struct thread_ctx {
	const char *in;
	char *out;
	ssize_t len;
	int maxval;
	union {
		struct { int min, max; } val;
		struct { int rowlen, stride; } blk;
	} un;
};

static void* val_thread(void *_ctx) {
	struct thread_ctx *ctx = (struct thread_ctx*)_ctx;
	struct timespec tstart;
	struct timespec tstart_cpu;
	if (clock_gettime(CLOCK_MONOTONIC_RAW, &tstart) < 0) { perror("Failed to get time"); exit(1); }
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tstart_cpu) < 0) { perror("Failed to get time"); exit(1); }
	if (ctx->maxval <= 255) {
		for (ssize_t i = 0; i < ctx->len; i++) {
			int val = (unsigned char)ctx->in[i];
			if (ctx->un.val.min <= val && val < ctx->un.val.max) {
				ctx->out[i] = ctx->maxval - val;
			}
		}
	} else {
		for (ssize_t i = 0; i+1 < ctx->len; i += 2) {
			int val = ((unsigned char)ctx->in[i] << 8) | (unsigned char)ctx->in[i+1];
			if (ctx->un.val.min <= val && val < ctx->un.val.max) {
				val = ctx->maxval - val;
				ctx->out[i] = val >> 8;
				ctx->out[i+1] = val & 255;
			}
		}
	}
	struct timespec tend;
	struct timespec tend_cpu;
	if (clock_gettime(CLOCK_MONOTONIC_RAW, &tend) < 0) { perror("Failed to get time"); exit(1); }
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tend_cpu) < 0) { perror("Failed to get time"); exit(1); }
	char buf[80];
	char *p = buf;
	p += sprintf(p, "Thread %d took ", gettid());
	p += sprint_dur(p, tstart, tend);
	p += sprintf(p, " wall time, ");
	p += sprint_dur(p, tstart_cpu, tend_cpu);
	p += sprintf(p, " CPU time\n");
	writeall(STDOUT_FILENO, buf, strlen(buf));
	return _ctx;
}

static void* blk_thread(void *_ctx) {
	struct thread_ctx *ctx = (struct thread_ctx*)_ctx;
	struct timespec tstart;
	struct timespec tstart_cpu;
	if (clock_gettime(CLOCK_MONOTONIC_RAW, &tstart) < 0) { perror("Failed to get time"); exit(1); }
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tstart_cpu) < 0) { perror("Failed to get time"); exit(1); }
	for (ssize_t base = 0; base + ctx->un.blk.rowlen <= ctx->len; base += ctx->un.blk.stride) {
		if (ctx->maxval <= 255) {
			for (ssize_t i = base; i < base + ctx->un.blk.rowlen; i++) {
				int val = ctx->in[i];
				ctx->out[i] = ctx->maxval - val;
			}
		} else {
			for (ssize_t i = base; i < base + ctx->un.blk.rowlen; i += 2) {
				int val = (ctx->in[i] << 8) | ctx->in[i+1];
				val = ctx->maxval - val;
				ctx->out[i] = val >> 8;
				ctx->out[i+1] = val & 255;
			}
		}
	}
	struct timespec tend;
	struct timespec tend_cpu;
	if (clock_gettime(CLOCK_MONOTONIC_RAW, &tend) < 0) { perror("Failed to get time"); exit(1); }
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tend_cpu) < 0) { perror("Failed to get time"); exit(1); }
	char buf[80];
	char *p = buf;
	p += sprintf(p, "Thread %d took ", gettid());
	p += sprint_dur(p, tstart, tend);
	p += sprintf(p, " wall time, ");
	p += sprint_dur(p, tstart_cpu, tend_cpu);
	p += sprintf(p, " CPU time\n");
	writeall(STDOUT_FILENO, buf, strlen(buf));
	return _ctx;
}

int main(int argc, char** argv) {
	long threads;
	enum partype method;
	if (argc != 5 || !parse_int(argv[1], &threads, NULL, true) || threads < 1 || !parse_partype(argv[2], &method)) {
		printf("Usage: %s <threads> <method: blk | num> <input file> <output file>\n", argc > 0 ? argv[0] : "main");
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

	bool incomment = false;

	while ((nread = read(infd, buf+buffered, sizeof(buf)-buffered-1)) > 0) {
#define CHK(n) do { if (p + (n) > end) goto moredata; } while (0);
		buffered += nread;
		int len = buffered;
		char *p = buf;
		char *end = p + len;
		p[len] = '\0';
		while (1) {
			// always between header tokens here, consume as much whitespace as possible
			if (state > 0 && state < 4) {
				CHK(1);
				while (isspace(*p)) { p++; CHK(1); }
				if (*p == '#') incomment = true; // detect comments after spaces
			}
			while (incomment) {
				CHK(1);
				if (*p++ == '\n') {
					incomment = false;
					if (state == 4 && !isspace(*p++)) goto badformat; // consume one extra whitespace if comment is before image data
				}
			}
			//char xxx[40];
			//printf("state %d written %ld\n", state, imagewritten);
			//snprintf(xxx,40,"%s",p);
			//xxx[39]='\0';
			//printf("%s\n", xxx);
			switch (state) {
			case 0:
				CHK(3);
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
				if (*p == '#') incomment = true;
				else if (!isspace(*p)) goto badformat;
				p++;
				state = 1;
				break;
			case 1: case 2: case 3:
				char *rest;
				long val;
				if (!parse_int(p, &val, &rest, false)) goto badformat;
				if (rest >= end) goto moredata;
				if (val <= 0) goto badformat;
				if (*rest == '#') incomment = true;
				else if (!isspace(*rest)) goto badformat;
				rest++;
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
	if (close(infd) < 0) {
		perror("Close failed");
		exit(1);
	}
	struct timespec read;
	struct timespec read_cpu;
	if (clock_gettime(CLOCK_MONOTONIC_RAW, &read) < 0) { perror("Failed to get time"); exit(1); }
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &read_cpu) < 0) { perror("Failed to get time"); exit(1); }

	char *out = malloc(imagewritten);

struct thread_ctx {
	const char *in;
	char *out;
	ssize_t len;
	int maxval;
	union {
		struct { int min, max; } val;
		struct { int rowlen, stride; } blk;
	} un;
};

	pthread_t *threadbuf = malloc(threads * sizeof(pthread_t));
	switch (method) {
		case NUMBERS:
			for (ssize_t i = 0; i < threads; i++) {
				struct thread_ctx *ctx = malloc(sizeof(struct thread_ctx));
				ctx->in = image;
				ctx->out = out;
				ctx->len = imagewritten;
				ctx->maxval = maxval;
				ctx->un.val.min = (maxval + 1) * i / threads;
				ctx->un.val.max = (maxval + 1) * (i + 1) / threads;
				if (pthread_create(&threadbuf[i], NULL, val_thread, ctx) != 0) {
					perror("Failed to create thread");
					exit(1);
				}
			}
			break;
		case BLOCK:
			for (ssize_t i = 0; i < threads; i++) {
				struct thread_ctx *ctx = malloc(sizeof(struct thread_ctx));
				int offs = width * i / threads;
				int rowlen = width * (i + 1) / threads - offs;
				ctx->in = image + offs;
				ctx->out = out + offs;
				ctx->len = imagewritten - offs;
				ctx->maxval = maxval;
				ctx->un.blk.rowlen = rowlen;
				ctx->un.blk.stride = width;
				if (pthread_create(&threadbuf[i], NULL, blk_thread, ctx) != 0) {
					perror("Failed to create thread");
					exit(1);
				}
			}
			break;
		default:
			assert(false && "Impossible case");
			_exit(1);
	}

	for (ssize_t i = 0; i < threads; i++) {
		struct thread_ctx *ctx;
		if (pthread_join(threadbuf[i], (void**)&ctx) != 0) {
			perror("Failed to join thread");
			exit(1);
		}
		free(ctx);
	}
	free(threadbuf);
	free(image);

	struct timespec invert;
	struct timespec invert_cpu;
	if (clock_gettime(CLOCK_MONOTONIC_RAW, &invert) < 0) { perror("Failed to get time"); exit(1); }
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &invert_cpu) < 0) { perror("Failed to get time"); exit(1); }

	char header[32];
	if (bit) { binary = true; bit = false; hasmaxval = true; } // P4 -> P5
	int headerbytes = sprintf(header, "P%c\n%d %d", (bit || binary ? 3 : 0) + (!hasmaxval ? '1' : channels == 1 ? '2' : '3'), width, height);
	if (hasmaxval) headerbytes += sprintf(header + headerbytes, " %d", maxval);
	sprintf(header + headerbytes, "\n");
	writeall(outfd, header, strlen(header));
	if (binary) {
		writeall(outfd, out, imagewritten);
	} else {
		char buffer[4096];
		int buffered = 0;
		int column = 0;
		for (int i = 0; i < imagewritten; ) {
			int val;
			if (maxval <= 255) {
				val = (unsigned char)out[i];
				i++;
			} else {
				val = ((unsigned char)out[i] << 8) | (unsigned char)out[i+1];
				i++; i++;
			}
			bool nl = column > 160 || i == imagewritten;
			int written = sprintf(buffer + buffered, "%d%c", val, nl ? '\n': ' ');
			buffered += written;
			column += written;
			if (nl) column = 0;
			if (buffered > 4000) {
				writeall(outfd, buffer, buffered);
				buffered = 0;
			}
		}
		if (buffered > 0) {
			writeall(outfd, buffer, buffered);
			buffered = 0;
		}
	}

	if (close(outfd) < 0) {
		perror("Close failed");
		exit(1);
	}

	struct timespec write;
	struct timespec write_cpu;
	if (clock_gettime(CLOCK_MONOTONIC_RAW, &write) < 0) { perror("Failed to get time"); exit(1); }
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &write_cpu) < 0) { perror("Failed to get time"); exit(1); }

	free(out);

	char durbuf[32];
	sprint_dur(durbuf, start, write);
	printf("Main thread took %13s wall time, ", durbuf);
	sprint_dur(durbuf, start_cpu, write_cpu);
	printf("%13s CPU time total\n", durbuf);
	sprint_dur(durbuf, start, read);
	printf("- Read input file:     %13s wall time, ", durbuf);
	sprint_dur(durbuf, start_cpu, read_cpu);
	printf("%13s CPU time\n", durbuf);
	sprint_dur(durbuf, read, invert);
	printf("- Spawn, invert, join: %13s wall time, ", durbuf);
	sprint_dur(durbuf, read_cpu, invert_cpu);
	printf("%13s CPU time\n", durbuf);
	sprint_dur(durbuf, invert, write);
	printf("- Write output file:   %13s wall time, ", durbuf);
	sprint_dur(durbuf, invert_cpu, write_cpu);
	printf("%13s CPU time\n", durbuf);

	return 0;
}
