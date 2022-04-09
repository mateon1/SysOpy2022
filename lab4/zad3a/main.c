// Mateusz Naściszewski, 2022
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <time.h>
#include <sys/wait.h>

#if defined(RT)
#define PROG "main-rt"
#elif defined(KILL)
#define PROG "main-kill"
#elif defined(QUEUE)
#define PROG "main-queue"
#else
#error "Need to provide one of -D RT, -D KILL, -D QUEUE preprocessor flags"
#endif

static int targetpid;
static int counter = 0;
static bool finished = false;

__attribute__((format(printf, 1, 2)))
static void report(const char *fmt, ...) {
    printf("[PID %d] ", getpid());
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
}

#ifdef RT
#define SIGINC (SIGRTMIN + 1)
#define SIGFIN (SIGRTMIN + 2)
#else
#define SIGINC SIGUSR1
#define SIGFIN SIGUSR2
#endif

#define TRY(ret, func, ...) do {\
    ret = func(__VA_ARGS__);\
    if (ret < 0) {\
        perror(#func " failed");\
        exit(1);\
    }\
} while (0)

#ifdef QUEUE
static void send(int sig) {
    int ret;
    TRY(ret, sigqueue, targetpid, sig, (union sigval){0});
}
#else
static void send(int sig) {
    int ret;
    TRY(ret, kill, targetpid, sig);
}
#endif

typedef void (*sighandler_t)(int);

static void handler_inc(int signum) {
    counter++;
    //report("Received signal %d, counter now %d\n", signum, counter);
    (void) signum;
}

static void handler_fin(int signum) {
    report("Received end signal %d, final counter is %d\n", signum, counter);
    finished = true;
}

static void nap(void) {
    struct timespec sleeplen = { .tv_sec = 0, .tv_nsec = 1 };
    int ret = nanosleep(&sleeplen, NULL);
    if (ret != 0 && errno != EINTR) {
        perror("nanosleep failed");
        exit(1);
    }
}

static void child(void) {
    sighandler_t old;
    old = signal(SIGINC, &handler_inc);
    if (old == SIG_ERR) {
        perror("failed to define signal handler");
        exit(1);
    }
    old = signal(SIGFIN, &handler_fin);
    if (old == SIG_ERR) {
        perror("failed to define signal handler");
        exit(1);
    }
    while (!finished) nap();
    for (int i = 0; i < counter; i++) {
        send(SIGINC);
        nap();
        //sched_yield();
    }
    send(SIGFIN);
}

static void start_child(void) {
    int ret;
    // Prevent weird duplicate output, flush before fork
    TRY(ret, fflush, stdout);
    TRY(ret, fflush, stderr);

    int cpid;
    TRY(cpid, fork);
    if (cpid == 0) {
        targetpid = getppid();
        child();
        exit(0);
    } else {
        targetpid = cpid;
    }
}

void wait_child(void) {
    int wstatus, ret;
    TRY(ret, wait, &wstatus);
    if (WIFEXITED(wstatus)) {
        report("Child exited (%d)\n", WEXITSTATUS(wstatus));
    } else if (WIFSIGNALED(wstatus)) {
        report("Child killed by signal %d\n", WTERMSIG(wstatus));
    } else {
        report("Child did something else\n");
    }
}

static bool parse_int(char *str, long *out) {
    if (*str == '\0') return false;
    char *endptr;
    long res = strtol(str, &endptr, 10);
    *out = res;
    return *endptr == '\0';
}

int main(int argc, char **argv) {
    long cnt;
    if (argc != 2 || !parse_int(argv[1], &cnt) || cnt < 0) {
        printf("Usage: %s <increment count>\n", argc > 0 ? argv[0] : PROG);
        exit(2);
    }
    sighandler_t old;
    old = signal(SIGINC, &handler_inc);
    if (old == SIG_ERR) {
        perror("failed to define signal handler");
        exit(1);
    }
    old = signal(SIGFIN, &handler_fin);
    if (old == SIG_ERR) {
        perror("failed to define signal handler");
        exit(1);
    }

    start_child();

    for (long i = 0; i < cnt; i++) {
        send(SIGINC);
        nap();
        //sched_yield();
    }
    nap();
    send(SIGFIN);

    while (!finished) {
        nap();
    }

    printf("Expected signal count: %ld\n", cnt);

    wait_child();

    return 0;
}
