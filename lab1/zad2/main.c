#include <stdio.h> // printf, fprintf
#include <stdlib.h> // atoi
#include <string.h> // strcmp
#include <assert.h> // assert
#include <sys/time.h> // clock_t
#include <sys/times.h> // struct tms, times

#include "libwc.h"

static char* timer_name;
static struct tms start_tm, end_tm;
static clock_t start_clk = 0, end_clk = 0;

typedef struct command_t {
    char* name;
    int (*func)(int, char**);
} command_t;

static libwc_context wc_ctx;

int com_timer(int left, char **args) {
    assert(left >= 1);
    timer_name = args[0];
    start_clk = times(&start_tm);
    return 1;
}

int com_endtimer(int left, char **args) {
    assert(timer_name != NULL);

    end_clk = times(&end_tm);

    printf("%s: %ld real, %ld user (+%ld children), %ld system (+%ld children)\n",
        timer_name,
        end_clk - start_clk,
        end_tm.tms_utime - start_tm.tms_utime,
        end_tm.tms_cutime - start_tm.tms_cutime,
        end_tm.tms_stime - start_tm.tms_stime,
        end_tm.tms_cstime - start_tm.tms_cstime);

    return 0;
}

int com_count(int left, char **args) {
    assert(left >= 1);
    libwc_stats_to_tmpfile(wc_ctx, args[0]);
    int res = libwc_load_result(wc_ctx);
    if (res < 0) {
        fprintf(stderr, "Failed to load wc result\n");
        exit(1);
    }
    return 1;
}

int com_del(int left, char **args) {
    assert(left >= 1);
    int idx = atoi(args[0]);
    bool res = libwc_del_result(wc_ctx, idx);
    assert(res);
    return 1;
}

#define COMMAND(FUNC) (command_t) {.name = #FUNC, .func = & com_##FUNC }

static command_t commands[] = {
    COMMAND(timer),
    COMMAND(endtimer),
    COMMAND(count),
    COMMAND(del),
};


int main(int argc, char** argv) {

    // Skip argv[0] - program name
    char **args = argv + 1;
    int left = argc - 1;

    wc_ctx = libwc_create();

    int i = 0;
    while (left > 0 && i < sizeof(commands) / sizeof(commands[0])) {
        if (strcmp(*args, commands[i].name) == 0) {
            printf("> Run command %s\n", commands[i].name);
            // Consume command name
            args += 1; left -= 1;
            int consumed = commands[i].func(left, args);
            // Consume processed args
            args += consumed; left -= consumed;
            // Restart loop over commands
            i = 0;
        } else {
            i++;
        }
    }

    if (left > 0) {
        fprintf(stderr, "Unrecognized command: %s\n", *args);
        return 1;
    }

    return 0;
}
