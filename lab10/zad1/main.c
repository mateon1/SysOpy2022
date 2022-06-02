// Mateusz Naściszewski, 2022
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#define SECOND 1000000000L
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

int main(int argc, char** argv) {
	// SERVER
	// TODO:
	// * Parse listen port, open sock
	// * Pinging thread, delete client on timeout, inform pair (if any)
	// ** On UDP, retry up to 3 times with bigger timeouts
	// * Sock listen thread (epoll/select)

	// CLIENT
	// TODO:
	// * Parse sock or addr, connect
	// * Handle SIGINT

	// UDP considerations:
	// * Retries for all message sending, add some ACK/NAK functionality
	return 0;
}
