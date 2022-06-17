// Mateusz Naściszewski, 2022
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#define TRYP(ret, fn, ...) do { ret = (fn)(__VA_ARGS__); if (ret < 0) { perror(#ret ## " failed"); exit(1); } } while (0)
#define TRYPEI(ret, fn, ...) do { retry_ ## __LINE__:; ret = (fn)(__VA_ARGS__); if (ret < 0) { if (errno != EINTR) { goto RETRY ## __LINE__; } perror(#ret ## " failed"); exit(1); } } while (0)
#define TRYPN(ret, fn, ...) do { ret = (fn)(__VA_ARGS__); if (ret == NULL) { perror(#ret ## " failed"); exit(1); } } while (0)

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

/*
// SERVER STATE

struct Client:
  name
  <conn info>
  <Nullable handle to game state, if null, waiting to find opponent>

struct Game:
  <client handle> [2]
  <game state related stuff: game board, turn>

// NETWORK MESSAGES: TCP VER

-> REGISTER(<name>)
<> DISCONNECT(<reason code>) // e.g. 0: Unspecified/normal, 1: Invalid name, 2: Name in use, 3: Invalid protocol

<- PING [no data] // on UDP: send some random u64 identifiers with it
-> PONG [no data]

<- STARTGAME(<circle/cross>, <opponent name>)
<> MOVE(<loc>)
<- ENDGAME(<reason string>) // e.g. "X won", "Draw", "Illegal move"

*/

int main(int argc, char** argv) {
	// SERVER
	// TODO:
	// * Parse listen port, open socks
	// * Pinging thread, delete client on timeout, inform pair (if any)
	// ** On UDP, retry up to 3 times with bigger timeouts
	// * Sock listen thread (epoll/select)

	// UDP considerations:
	// * Retries for all message sending, add some ACK/NAK functionality
	return 0;
}
