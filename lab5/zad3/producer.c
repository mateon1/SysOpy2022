// Mateusz Naściszewski, 2022
#include <stdio.h>
#include <time.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/ioctl.h>

#ifndef MAXLINE
#define MAXLINE 100000
#endif

static void nap(long min, long max) {
    long dur = max == min ? min : min + rand() % (max - min);
    struct timespec sleeplen = { .tv_sec = dur / 1000000000L, .tv_nsec = dur % 1000000000L };
    struct timespec rem;
    int ret;
retry:
    ret = nanosleep(&sleeplen, &rem);
    if (ret != 0) {
        if (errno == EINTR) {
            sleeplen = rem;
            goto retry;
        }
        perror("nanosleep failed");
        exit(1);
    }
}

static void lock(int fd, int operation) {
retry:
    if (flock(fd, operation) < 0) {
        if (errno == EINTR) goto retry;
        perror("Failed to acquire file lock");
        exit(1);
    }
}

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

static bool parse_int(char *str, long *out) {
    if (*str == '\0') return false;
    char *endptr;
    long res = strtol(str, &endptr, 10);
    *out = res;
    return *endptr == '\0';
}

int main(int argc, char** argv) {
    long blocksize, lineno;
    if (argc != 5 || !parse_int(argv[4], &blocksize) || blocksize < 0 || !parse_int(argv[2], &lineno) || lineno < 1 || lineno > MAXLINE) {
        printf("Usage: %s <fifo> <line num> <in file> <block size>\n", argc > 0 ? argv[0] : "producer");
        exit(2);
    }
    srand(lineno);

    int fifofd = open(argv[1], O_WRONLY);
    if (fifofd < 0) {
        perror("Failed to open fifo");
        exit(1);
    }
    int infd = open(argv[3], O_RDONLY);
    if (infd < 0) {
        perror("Failed to open input file");
        exit(1);
    }

    char *buf = malloc(blocksize + 12);
    char *wrstart = buf + sprintf(buf, "% 10ld:", lineno);
    assert(wrstart - buf == 11);

    int nread;
    while ((nread = read(infd, wrstart, blocksize)) > 0) {
        if (wrstart[nread-1] != '\n')
            wrstart[nread++] = '\n';
        lock(fifofd, LOCK_EX);
        // wait for pipe to empty to avoid coalescing writes
        int bytes;
        while (1) {
            if (ioctl(fifofd, FIONREAD, &bytes) < 0) {
                perror("ioctl on pipe failed");
                if (close(fifofd) < 0) perror("Failed to close pipe");
                exit(1);
            }
            if (bytes > 0)
                nap(1000000L, 1000000L); // 1ms
            else
                break;
        }
        writeall(fifofd, buf, wrstart + nread - buf);
        lock(fifofd, LOCK_UN);
        nap(20000000L, 150000000L); // 20ms - 150ms
    }
    if (nread < 0) {
        perror("Failed to read input file");
        exit(1);
    }

    if (close(infd) < 0) {
        perror("Failed to close input file");
        exit(1);
    }
    if (close(fifofd) < 0) {
        perror("Failed to close pipe");
        exit(1);
    }

    return 0;
}
