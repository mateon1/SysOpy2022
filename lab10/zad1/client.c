// Mateusz Naściszewski, 2022
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#define SOCKTY SOCK_STREAM

#define TRYP(ret, fn, ...) do { ret = (fn)(__VA_ARGS__); if (ret < 0) { perror(#fn " failed"); exit(1); } } while (0)
#define TRYPNZ_(fn, ...) do { int ret = (fn)(__VA_ARGS__); if (ret != 0) { perror(#fn " failed"); exit(1); } } while (0)
#define TRYP_(fn, ...) do { int ret = (fn)(__VA_ARGS__); if (ret < 0) { perror(#fn " failed"); exit(1); } } while (0)
#define TRYPEI(ret, fn, ...) do { retry_ ## __LINE__:; ret = (fn)(__VA_ARGS__); if (ret < 0) { if (errno != EINTR) { goto RETRY ## __LINE__; } perror(#fn " failed"); exit(1); } } while (0)
#define TRYPN(ret, fn, ...) do { ret = (fn)(__VA_ARGS__); if (ret == NULL) { perror(#fn " failed"); exit(1); } } while (0)


//#define SECOND 1000000000L
//static void nap(long min, long max) {
//	long dur = max == min ? min : min + rand() % (max - min);
//	struct timespec sleeplen = { .tv_sec = dur / 1000000000L, .tv_nsec = dur % 1000000000L };
//	struct timespec rem;
//	int ret;
//retry:
//	ret = nanosleep(&sleeplen, &rem);
//	if (ret != 0) {
//		if (errno == EINTR) {
//			sleeplen = rem;
//			goto retry;
//		}
//		perror("nanosleep failed");
//		exit(1);
//	}
//}

/*
// NETWORK MESSAGES: TCP VER

-> REGISTER(<name>)
<> DISCONNECT(<reason code>) // e.g. 0: Unspecified/normal, 1: Invalid name, 2: Name in use, 3: Invalid protocol

<- PING [no data] // on UDP: send some random u64 identifiers with it
-> PONG [no data]

<- STARTGAME(<circle/cross>, <opponent name>)
<> MOVE(<loc>)
<- ENDGAME(<reason string>) // e.g. "X won", "Draw", "Illegal move"

// GAME DISPLAY

[ANSI COLOR]YOUR TURN

x   x |       |  ooo
 x x  |       | o   o
  1   |   2   | o 3 o
 x x  |       | o   o
x   x |       |  ooo
------+-------+------
      |  ooo  |
      | o   o |
  4   | o 5 o |   6
      | o   o |
      |  ooo  |
------+-------+------
x   x |       |
 x x  |       |
  7   |   8   |   9
 x x  |       |
x   x |       |
Player O: [name]
Player X: [name]

*/

static struct epoll_event event_stdin = {
	.events = EPOLLIN,
	.data = { .u64 = 0 }
};
static struct epoll_event event_sock = {
	.events = EPOLLIN | EPOLLOUT,
	.data = { .u64 = 1 }
};

static struct sockaddr_un sa_local = {
	.sun_family = AF_UNIX
};
//static struct sockaddr_in sa_inet = {
//	.sin_family = AF_INET
//};

static struct addrinfo ai_hint = {
	.ai_family = AF_UNSPEC,
	//.ai_socktype = SOCKTY,
};

int main(int argc, char** argv) {
	char type;
	if (argc < 3 || (type = argv[1][0]) == '\0' || argv[1][1] != '\0') {
usage:
		printf("Usage: %s <u|x|s> <socket address>\n", argc > 0 ? argv[0] : "client");
		printf("       %s <i|4|6> <host> <port>\n", argc > 0 ? argv[0] : "client");
		exit(2);
	}
	// TODO:
	// * Handle SIGINT

	int epfd;
	TRYP(epfd, epoll_create1, 0);
	TRYP_(epoll_ctl, epfd, EPOLL_CTL_ADD, STDIN_FILENO, &event_stdin);

	int sockfd;
	switch (type) {
	case 's': case 'u': case 'x':
		if (argc != 3) goto usage;
		strncpy(sa_local.sun_path, argv[2], sizeof(sa_local.sun_path)-1);
		TRYP(sockfd, socket, AF_UNIX, SOCKTY, 0); // XXX: Should it be 6 for TCP?
		TRYP_(connect, sockfd, (struct sockaddr*)&sa_local, sizeof(sa_local));
		break;
	case 'i': case '4': case '6':
		if (argc != 4) goto usage;
		if (type == '4') ai_hint.ai_family = AF_INET;
		if (type == '6') ai_hint.ai_family = AF_INET6;
		struct addrinfo *res;
		TRYPNZ_(getaddrinfo, argv[2], argv[3], &ai_hint, &res); // weirdly handles errors if invalid addresses given
		bool success = false;
		for (struct addrinfo *rp = res; rp != NULL; rp = rp->ai_next) {
			sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
			if (sockfd < 0) continue;
			if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1) { success = true; break; }
			TRYP_(close, sockfd); // connect failed
		}
		freeaddrinfo(res);
		if (!success) {
			printf("Connecting to address failed\n");
			exit(3);
		}
		break;
	default:
		printf("Invalid address type\n");
		goto usage;
	}

	// Now: connected on sockfd, add to epoll instance and start sending
	TRYP_(epoll_ctl, epfd, EPOLL_CTL_ADD, sockfd, &event_sock);

	// until exiting, epoll_wait, do stuff

	return 0;
}
