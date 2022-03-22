// Mateusz Naściszewski, 2022
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char** argv) {
    if (argc != 2 || *argv[1] == '\0') {
        printf("Usage: %s <num forks>\n", argc > 0 ? argv[0] : "main");
        exit(2);
    }

    char *endptr = NULL;
    long forks = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0' || forks <= 0) {
        fprintf(stderr, "Failed to parse argument\n");
        exit(2);
    }

    for (int i = 0; i < forks; i++) {
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
            char buf[48];
            int written = sprintf(buf, "Hello from child %d (pid %d)\n", i + 1, getpid());
            if (write(STDOUT_FILENO, buf, written) == -1) {
                perror("Child write failed");
                exit(1);
            }
            exit(0);
        } else {
            // Parent
            // Do nothing, for now
        }
    }

    int exitval = 0;
    for (int i = 0; i < forks; i++) {
        int status;
        if (wait(&status) == -1) {
            perror("Failed to wait() for child");
            // Don't exit, wait for the other children to prevent zombies
            exitval = 1;
        }
    }

    return exitval;
}
