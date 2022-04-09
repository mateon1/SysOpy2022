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

static int targetpid = 0;
static int counter = 0;
static bool finished = false;
static bool sending = false;
static bool confirmed = false;

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

static void nap(void) {
    struct timespec sleeplen = { .tv_sec = 0, .tv_nsec = 1 };
    int ret = nanosleep(&sleeplen, NULL);
    if (ret != 0 && errno != EINTR) {
        perror("nanosleep failed");
        exit(1);
    }
}

#ifdef QUEUE
static void send(int sig) {
    int ret;
    TRY(ret, sigqueue, targetpid, sig, (union sigval){counter});
}
#else
static void send(int sig) {
    int ret;
    TRY(ret, kill, targetpid, sig);
}
#endif

static void confirm_inc(void) {
    assert(sending == true);
    confirmed = false;
    send(SIGINC);
    while (!confirmed) nap();
}

typedef void (*sighandler_t)(int);

static void handler_inc(int signum, siginfo_t *info, void *uctx) {
    (void) signum;
    (void) uctx;
    if (targetpid == 0) {
        targetpid = info->si_pid;
    }
    if (sending) {
        // received confirmation
        confirmed = true;
    } else if (!finished) {
        counter++;
        send(SIGINC); // send confirmation
    }
}

static void handler_fin(int signum, siginfo_t *info, void *uctx) {
    (void) signum;
    (void) uctx;
    if (targetpid == 0) {
        targetpid = info->si_pid;
    }
#ifdef QUEUE
    report("Received end signal %d, final counter is %d, expected counter %d\n", signum, counter, info->si_value.sival_int);
#else
    report("Received end signal %d, final counter is %d\n", signum, counter);
#endif
    finished = true;
}

static void child(void) {
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    int ret;
    TRY(ret, sigemptyset, &sa.sa_mask);

    sa.sa_sigaction = handler_inc;
    TRY(ret, sigaction, SIGINC, &sa, NULL);
    sa.sa_sigaction = handler_fin;
    TRY(ret, sigaction, SIGFIN, &sa, NULL);

    while (!finished) nap();
    sending = true;
    for (int i = 0; i < counter; i++) {
        confirm_inc();
    }
    send(SIGFIN);
    sending = false;
}

static void start_child(void) {
    int ret;
    // Prevent weird duplicate output, flush before fork
    TRY(ret, fflush, stdout);
    TRY(ret, fflush, stderr);

    int cpid;
    TRY(cpid, fork);
    if (cpid == 0) {
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

    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    int ret;
    TRY(ret, sigemptyset, &sa.sa_mask);

    sa.sa_sigaction = handler_inc;
    TRY(ret, sigaction, SIGINC, &sa, NULL);

    sa.sa_sigaction = handler_fin;
    TRY(ret, sigaction, SIGFIN, &sa, NULL);

    start_child();

    nap();

    sending = true;
    counter = cnt;
    for (long i = 0; i < counter; i++) {
        confirm_inc();
    }
    send(SIGFIN);
    sending = false;
    counter = 0;

    while (!finished) {
        nap();
    }

    printf("Expected signal count: %ld, received %d\n", cnt, counter);

    wait_child();

    return 0;
}
