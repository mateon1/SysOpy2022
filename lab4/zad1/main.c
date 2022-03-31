// Mateusz Naściszewski, 2022
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

#ifdef FORK
#define PROG "main-fork"
#else
// Don't handle CHILD case, doesn't have usage.
#define PROG "main-exec"
#endif

typedef void (*sighandler_t)(int);

static bool ischild = false;
#ifndef CHILD
static void handle_sig1(int signum) {
    printf("[%s, PID %d] Received signal %d\n", ischild ? "child" : "parent", getpid(), signum);
}
#endif

static void dopending() {
    sigset_t set;
    if (sigpending(&set) < 0) { perror("sigpending failed"); exit(1); }
    int ret = sigismember(&set, SIGUSR1);
    if (ret < 0) { perror("sigismember failed"); exit(1); }
    printf("[%s, PID %d] Saw pending? %d\n", ischild ? "child" : "parent", getpid(), ret);
}

#ifndef CHILD
static bool parse_int(char *str, long *out) {
    if (*str == '\0') return false;
    char *endptr;
    long res = strtol(str, &endptr, 10);
    *out = res;
    return *endptr == '\0';
}

int main(int argc, char** argv) {
    // Only handle arguments in parent process
    long mode;
    if (argc != 2 || !parse_int(argv[1], &mode) || mode < 0 || mode > 3) {
        printf("Usage: %s <ignore mode: 0 | 1 | 2 | 3>\n", argc > 0 ? argv[0] : PROG);
        exit(2);
    }
    switch (mode) {
        case 0: {// ignore
            sighandler_t ret1 = signal(SIGUSR1, SIG_IGN);
            if (ret1 == SIG_ERR) {
                perror("Failed to ignore signal");
                exit(1);
            }
            printf("Ignored signal\n");
            break;
        }
        case 1: {// handle
            sighandler_t ret2 = signal(SIGUSR1, &handle_sig1);
            if (ret2 == SIG_ERR) {
                perror("Failed to install signal handler");
                exit(1);
            }
            printf("Set signal handler\n");
            break;
        }
        case 2: {// mask
            sigset_t signalset;
            if (sigemptyset(&signalset) < 0) { perror("sigemptyset failed"); exit(1); }
            if (sigaddset(&signalset, SIGUSR1) < 0) { perror("sigaddset failed"); exit(1); }
            if (sigprocmask(SIG_BLOCK, &signalset, NULL)) {
                perror("Failed to set signal mask");
                exit(1);
            }
            printf("Set signal mask\n");
            break;
        }
        case 3: // pending
            break;
        default:
            assert(0 && "Can't reach this");
    }
    raise(SIGUSR1);
    if (mode == 2) {
        dopending();
    }
    int cpid = fork();
    if (cpid == 0) {
#ifdef FORK
        ischild = true;
        if (mode == 3) {
            dopending();
        } else {
            raise(SIGUSR1);
        }
#else
        if (mode == 3) {
            execv("./main-child", (char*[]){"./main-child", "pending", NULL});
        } else {
            execv("./main-child", (char*[]){"./main-child", NULL});
        }
#endif
    } else {
        int wstatus;
        if (wait(&wstatus) < 0) {
            perror("wait failed");
            exit(1);
        }
    }
    return 0;
}
#else // CHILD
int main(int argc, char** argv) {
    ischild = true;
    if (argc == 2) {
        // pending
        dopending();
    } else {
        raise(SIGUSR1);
    }
    return 0;
}
#endif
