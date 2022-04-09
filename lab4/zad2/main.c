// Mateusz Naściszewski, 2022
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <sched.h>
#include <sys/wait.h>

__attribute__((format(printf, 1, 2)))
static void report(const char *fmt, ...) {
    printf("[PID %d] ", getpid());
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
}

#define TRY(ret, func, ...) do {\
    ret = func(__VA_ARGS__);\
    if (ret < 0) {\
        perror(#func " failed");\
        exit(1);\
    }\
} while (0)

#define RUN(func) do {\
    int ret;\
    /* Prevent weird duplicate output, flush before fork */ \
    TRY(ret, fflush, stdout);\
    TRY(ret, fflush, stderr);\
    int cpid;\
    TRY(cpid, fork);\
    if (cpid == 0) {\
        func();\
        exit(0);\
    } else {\
        int wstatus;\
        TRY(ret, waitpid, cpid, &wstatus, 0);\
        if (WIFEXITED(wstatus)) {\
            report("Child " #func " exited (%d)\n", WEXITSTATUS(wstatus));\
        } else if (WIFSIGNALED(wstatus)) {\
            report("Child " #func " killed by signal %d\n", WTERMSIG(wstatus));\
        } else {\
            report("Child " #func " did something else\n");\
        }\
    }\
} while (0)

typedef void (*sighandler_t)(int);

static void siginfo_handler(int signum, siginfo_t *info, void *uctx) {
    (void)uctx;
    report("Received signal %d with siginfo!\n", signum);
    report("  sig: %d, errno: %d, code: %d\n",
            info->si_signo,
            info->si_errno,
            info->si_code);
    report("  value: %d, status: %d, sender pid: %d, uid: %d\n",
            info->si_value.sival_int,
            info->si_status,
            info->si_pid,
            info->si_uid);
}

static void sig_handler(int signum) {
    report("Received signal %d\n", signum);
}

static void parent_killer(void) {
    int ret;
    // Delay a bit for parent to catch up
    for (int i = 0; i < 10; i++) TRY(ret, sched_yield);

    report("kill(parent, SIGUSR1);\n");
    TRY(ret, kill, getppid(), SIGUSR1);
}

static void do_siginfo(void) {
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = siginfo_handler;
    int ret;
    TRY(ret, sigemptyset, &sa.sa_mask);

    TRY(ret, sigaction, SIGUSR1, &sa, NULL);
    report("Registered SIGUSR1 handler\n");

    report("raise(SIGUSR1);\n");
    TRY(ret, raise, SIGUSR1);

    report("sigqueue(self, SIGUSR1, {1000000007});\n");
    TRY(ret, sigqueue, getpid(), SIGUSR1, (union sigval){1000000007});

    report("kill(self, SIGUSR1);\n");
    TRY(ret, kill, getpid(), SIGUSR1);
}

static void do_norestart(void) {
    struct sigaction sa;
    sa.sa_flags = 0;
    sa.sa_handler = sig_handler;
    int ret;
    TRY(ret, sigaction, SIGUSR1, &sa, NULL);
    report("Registered SIGUSR1 handler without SA_RESTART\n");

    RUN(parent_killer);
}

static void do_restart(void) {
    struct sigaction sa;
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = sig_handler;
    int ret;
    TRY(ret, sigaction, SIGUSR1, &sa, NULL);
    report("Registered SIGUSR1 handler with SA_RESTART\n");

    RUN(parent_killer);
}

static void do_resethand(void) {
    struct sigaction sa;
    sa.sa_flags = SA_RESETHAND;
    sa.sa_handler = sig_handler;
    int ret;
    TRY(ret, sigaction, SIGUSR1, &sa, NULL);
    report("Registered SIGUSR1 handler\n");

    report("raise(SIGUSR1);\n");
    TRY(ret, raise, SIGUSR1);

    report("raise(SIGUSR1);\n");
    TRY(ret, raise, SIGUSR1);
}

int main(void) {
    printf("== SA_SIGINFO ==\n");
    RUN(do_siginfo);
    printf("\n");

    printf("== SA_RESTART ==\n");
    RUN(do_norestart);
    printf("\n");
    RUN(do_restart);
    printf("\n");

    printf("== SA_RESETHAND (AKA: SA_ONESHOT) ==\n");
    RUN(do_resethand);

    return 0;
}
