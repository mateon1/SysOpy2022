// Mateusz Naściszewski, 2022
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <sys/file.h>

#ifndef MAXLINE
#define MAXLINE 100000
#endif

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

static size_t readall(int fd, char *buf, size_t size) {
    ssize_t ret;
    size_t nread = 0;
retry:
    while ((ret = read(fd, buf, size)) > 0 && size > 0) {
        buf += ret; size -= ret; nread += ret;
    }
    if (ret < 0) {
        if (errno == EINTR && size > 0) goto retry;
        perror("Failed to write data");
        exit(1);
    }
    assert(size == 0);
    return nread;
}

static bool parse_int(char *str, long *out) {
    if (*str == '\0') return false;
    char *endptr;
    long res = strtol(str, &endptr, 10);
    *out = res;
    return *endptr == '\0';
}

int main(int argc, char** argv) {
    long blocksize;
    if (argc != 4 || !parse_int(argv[3], &blocksize) || blocksize < 0) {
        printf("Usage: %s <fifo> <out file> <block size>\n", argc > 0 ? argv[0] : "consumer");
        exit(2);
    }

    int fifofd = open(argv[1], O_RDONLY);
    if (fifofd < 0) {
        perror("Failed to open fifo");
        exit(1);
    }
    int outfd = open(argv[2], O_RDWR | O_CREAT, 0644);
    if (fifofd < 0) {
        perror("Failed to open output file");
        exit(1);
    }

#define CLEANUP do {\
    if (close(fifofd) < 0) perror("Failed to close pipe");\
    exit(1);\
} while (0)

    char *buf = malloc(blocksize + 12);
    while (true) {
        char *p = buf;
        int nread;
        lock(outfd, LOCK_EX);
        while ((nread = read(fifofd, p, blocksize+12 - (p - buf))) > 0) {
            p += nread;
            if (p[-1] == '\n') break;
        }
        if (nread < 0) {
            perror("Read from pipe failed");
            CLEANUP;
        }
        if (p == buf) break;
        assert(p > buf);
        if (p[-1] != '\n') {
baddata:
            fprintf(stderr, "Received invalid data as input\n");
            write(STDOUT_FILENO, buf, p - buf);
            CLEANUP;
        }
read_coalescing_redo_hack: {
        char *origp = p;
        p = rawmemchr(buf, '\n') + 1;
        char *dstart = memchr(buf, ':', p - buf);
        if (dstart == NULL)
            goto baddata;
        assert(dstart - buf == 10);
        dstart++;
        int lineno = atoi(buf);
        if (lineno <= 0)
            goto baddata;

        // lock(outfd, LOCK_EX); // Cannot lock here, even if I would want to, since multiple consumer case breaks
        off_t outsize = lseek(outfd, 0, SEEK_END);
        if (outsize < 0) { perror("seek failed"); CLEANUP; }
        if (lseek(outfd, 0, SEEK_SET) < 0) { perror("seek failed"); CLEANUP; }
        char *outbuf = malloc(outsize);
        readall(outfd, outbuf, outsize);
        int curline = 0;
        off_t i = 0;
        while (i < outsize && curline < lineno) {
            if (outbuf[i++] == '\n') curline++;
        }
        if (curline == lineno) {
            i--;
            assert(outbuf[i] == '\n');
            // insert case
            lseek(outfd, i, SEEK_SET); // before newline
            writeall(outfd, dstart, p - dstart); // includes newline
            writeall(outfd, outbuf + i + 1, outsize - i - 1); // after newline
        } else {
            // append case
            assert(i == outsize);
            for (; curline < lineno - 1; curline++) {
                writeall(outfd, "\n", 1);
            }
            writeall(outfd, dstart, p - dstart);
        }
        free(outbuf);
        if (origp != p) {
            memmove(buf, p, origp - p);
            p = origp - (p - buf);
            goto read_coalescing_redo_hack;
        }
        }
        lock(outfd, LOCK_UN);
        //system("nl -ba test/out");
    }

    if (close(fifofd) < 0) {
        perror("Failed to close pipe");
        exit(1);
    }
    if (close(outfd) < 0) {
        perror("Failed to close output file");
        exit(1);
    }

    return 0;
}
