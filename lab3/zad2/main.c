// Mateusz Naściszewski, 2022
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

static inline double func(double x) {
    return 4.0 / (x * x + 1);
}

static inline unsigned ctz(unsigned value) {
    unsigned i = 0;
    if (value == 0) return 8 * sizeof(value);
    while ((value & 1) == 0) {
        i++; value >>= 1;
    }
    return i;
}

static double accum[8*sizeof(int)];
static __attribute__((noreturn)) void child(int offset, long samples, long workers) {
    unsigned mask = 0;
    for (long s = offset; s < samples; s += workers) {
        unsigned bin = ctz(++mask);
        accum[bin] = func((double)s / ((double)samples - 1.0));
        for (unsigned i = 0; i < bin; i++) {
            accum[bin] += accum[i];
        }
    }
    double res = 0.0;
    for (unsigned i = 0; i < 8*sizeof(int); i++) {
        if ((mask >> i) & 1) res += accum[i];
    }
    //printf("%.18g\n", res);
    char filename[12];
    sprintf(filename, "w%d.bin", offset+1);
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd == -1) {
        perror("Failed to open file");
        exit(1);
    }
    if (write(fd, &res, sizeof(res)) < (ssize_t)sizeof(res)) {
        perror("Write failed");
        if (close(fd) == -1)
            perror("Close failed");
        exit(1);
    }
    if (close(fd) == -1) {
        perror("Close failed");
        exit(1);
    }
    exit(0);
}

static bool parse_int(char *str, long *out) {
    if (*str == '\0') return false;
    char *endptr;
    long res = strtol(str, &endptr, 10);
    *out = res;
    return *endptr == '\0';
}

int main(int argc, char** argv) {
    long samples, workers;
    if (argc != 3 || !parse_int(argv[1], &samples) || samples <= 1 || !parse_int(argv[2], &workers) || workers <= 0) {
        printf("Usage: %s <samples> <workers>\n", argc > 0 ? argv[0] : "main");
        exit(2);
    }

    for (int i = 0; i < workers; i++) {
        pid_t cpid = fork();
        if (cpid == -1) {
            perror("fork failed");
            for (int j = 0; j < i; j++) {
                int status;
                if (wait(&status) == -1) {
                    perror("Failed to wait()");
                    // ignore nested error, still try to wait to prevent zombies
                }
            }
            exit(1);
        }
        if (cpid == 0) {
            // Child
            child(i, samples, workers);
            // noreturn
        }
    }

    int exitval = 0;
    for (int i = 0; i < workers; i++) {
        int status;
        if (wait(&status) == -1) {
            perror("Failed to wait() for child");
            // Don't exit, wait for the other children to prevent zombies
            exitval = 1;
        }
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Child failed\n");
            exitval = 1;
        }
    }
    if (exitval != 0) return exitval;

    unsigned mask = 0;
    for (int r = 0; r < workers; r++) {
        char filename[12];
        sprintf(filename, "w%d.bin", r+1);
        int fd = open(filename, O_RDONLY);
        if (fd == -1) {
            perror("Failed to open file");
            exit(1);
        }
        double res;
        if (read(fd, &res, sizeof(res)) < (ssize_t)sizeof(res)) {
            perror("Read failed");
            if (close(fd) == -1)
                perror("Close failed");
            exit(1);
        }
        if (close(fd) == -1) {
            perror("Close failed");
            exit(1);
        }
        unsigned bin = ctz(++mask);
        accum[bin] = res;
        for (unsigned i = 0; i < bin; i++) {
            accum[bin] += accum[i];
        }
    }

    double final = 0.0;
    for (unsigned i = 0; i < 8*sizeof(int); i++) {
        if ((mask >> i) & 1) final += accum[i];
    }

    printf("Result: %.18f\n", final / (double)samples);

    return exitval;
}
