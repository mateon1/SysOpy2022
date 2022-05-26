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

#define MAXELF 10
#define MAXELFWAIT 3
#define MAXREIN 9
#define MAXREINWAIT 9

typedef struct threadctx {
	pthread_mutex_t lock;
	pthread_cond_t cond;
	sem_t reindeer_sem;
	sem_t elf_sem;
	int reindeer;
	int elves;
	int elfids[MAXELFWAIT];
} ctx_t;

static void* santa_thread(void *_ctx) {
	ctx_t ctx = (ctx_t)_ctx;
	assert(pthread_mutex_lock(&ctx->lock) == 0);
	for (int i = 0; i < 3; i++) {
		while (ctx->reindeer < MAXREINWAIT && ctx->elves < MAXELFWAIT) {
			pthread_cond_wait(&ctx->cond);
		}

		if (ctx->reindeer >= MAXREINWAIT) {
			nap(2*SECONDS, 4*SECONDS);
			for (int j = 0; j < MAXREINWAIT; j++) sem_post(&ctx->reindeer_sem);
		} else if (ctx->elves >= MAXELFWAIT) {
			nap(1*SECONDS, 2*SECONDS);
			for (int j = 0; j < MAXELFWAIT; j++) sem_post(&ctx->elf_sem);
		} else {
			assert(false && "Unreachable case");
			exit(3);
		}
	}
	assert(pthread_mutex_unlock(&ctx->lock) == 0);
	return _ctx;
}

static void* elf_thread(void *_ctx) {
	ctx_t ctx = (ctx_t)_ctx;
	while (1) {
		nap(2*SECONDS, 5*SECONDS);
		assert(pthread_mutex_lock(&ctx->lock) == 0);
		ctx->elfids[ctx->elves] = gettid();
		ctx->elves++;
		if (ctx->elves >= MAXELFWAIT) pthread_cond_signal(&ctx->cond);
		assert(pthread_mutex_unlock(&ctx->lock) == 0);
		sem_wait(&ctx->elf_sem);
	}
	return _ctx;
}

static void* reindeer_thread(void *_ctx) {
	ctx_t ctx = (ctx_t)_ctx;
	while (1) {
		nap(5*SECONDS, 10*SECONDS);
		assert(pthread_mutex_lock(&ctx->lock) == 0);
		ctx->reindeer++;
		if (ctx->reindeer >= MAXREINWAIT) pthread_cond_signal(&ctx->cond);
		assert(pthread_mutex_unlock(&ctx->lock) == 0);
		sem_wait(&ctx->reindeer_sem);
	}
	return _ctx;
}

int main(int argc, char** argv) {
	if (argc != 1) {
		printf("Usage: %s\n # program expects no parameters", argc > 0 ? argv[0] : "main");
		exit(2);
	}

	ctx_t *ctx = calloc(sizeof(ctx_t), 1);
	if (ctx == NULL) {
		printf("Out of memory");
		exit(1);
	}

	pthread_t reindeer[MAXREIN];
	pthread_t elves[MAXELF];
	pthread_t santa;

	pthread_create(&santa, NULL, santa_thread, (void*)ctx);
	for (int i = 0; i < MAXELF; i++)
		pthread_create(&elves[i], NULL, elf_thread, (void*)ctx);
	for (int i = 0; i < MAXREIN; i++)
		pthread_create(&reindeer[i], NULL, reindeer_thread, (void*)ctx);

}
