// Mateusz Naściszewski, 2022
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

#define SECONDS 1000000000L
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
	sem_t elfslots_sem;
	int reindeer;
	int elves;
	int elfids[MAXELFWAIT];
} ctx_t;

static pthread_mutex_t report_lock = PTHREAD_MUTEX_INITIALIZER;
__attribute__((format(printf, 2, 3)))
static void report(const char *who, const char *fmt, ...) {
	pthread_mutex_lock(&report_lock);
	printf("[%8s, TID %d] ", who, gettid());
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	fflush(stdout);
	pthread_mutex_unlock(&report_lock);
}

static void init_ctx(ctx_t *ctx) {
	if (sem_init(&ctx->reindeer_sem, 0, 0) < 0) { perror("sem_init"); exit(1); };
	if (sem_init(&ctx->elf_sem, 0, 0) < 0) { perror("sem_init"); exit(1); };
	if (sem_init(&ctx->elfslots_sem, 0, MAXELFWAIT) < 0) { perror("sem_init"); exit(1); };
	if (pthread_mutex_init(&ctx->lock, NULL) < 0) { perror("pthread_mutex_init"); exit(1); }
	if (pthread_cond_init(&ctx->cond, NULL) < 0) { perror("pthread_mutex_init"); exit(1); }
	ctx->reindeer = 0;
	ctx->elves = 0;
}

static void* santa_thread(void *_ctx) {
	ctx_t *ctx = (ctx_t*)_ctx;
	assert(pthread_mutex_lock(&ctx->lock) == 0);
	int deliveries = 0;
	while (deliveries < 3) {
		while (ctx->reindeer < MAXREINWAIT && ctx->elves < MAXELFWAIT) {
			report("santa", "going to sleep\n");
			pthread_cond_wait(&ctx->cond, &ctx->lock);
			report("santa", "waking up\n");
		}

		if (ctx->reindeer >= MAXREINWAIT) {
			report("santa", "delivering presents\n");
			nap(2*SECONDS, 4*SECONDS);
			for (int j = 0; j < MAXREINWAIT; j++) sem_post(&ctx->reindeer_sem);
			ctx->reindeer = 0;
			deliveries++;
		} else if (ctx->elves >= MAXELFWAIT) {
			char buf[40 + 10 * MAXELFWAIT];
			char *p = buf + sprintf(buf, "solving problems for elves");
			for (int j = 0; j < MAXELFWAIT; j++) {
				p += sprintf(p, "%c %d", j == 0 ? ':' : ',', ctx->elfids[j]);
			}
			report("santa", "%s\n", buf);
			nap(1*SECONDS, 2*SECONDS);
			for (int j = 0; j < MAXELFWAIT; j++) sem_post(&ctx->elf_sem);
			ctx->elves = 0;
		} else {
			assert(false && "Unreachable case");
			exit(3);
		}
	}
	report("santa", "exiting\n");
	assert(pthread_mutex_unlock(&ctx->lock) == 0);
	return _ctx;
}

static void* elf_thread(void *_ctx) {
	ctx_t *ctx = (ctx_t*)_ctx;
	while (1) {
		nap(2*SECONDS, 5*SECONDS);
		sem_wait(&ctx->elfslots_sem);
		assert(pthread_mutex_lock(&ctx->lock) == 0);
		ctx->elfids[ctx->elves] = gettid();
		ctx->elves++;
		report("elf", "problem - %d elves waiting\n", ctx->elves);
		if (ctx->elves >= MAXELFWAIT) {
			report("elf", "waking santa\n");
			pthread_cond_signal(&ctx->cond);
		}
		assert(pthread_mutex_unlock(&ctx->lock) == 0);
		sem_wait(&ctx->elf_sem);
		report("elf", "problem solved\n");
		sem_post(&ctx->elfslots_sem);
	}
	return _ctx;
}

static void* reindeer_thread(void *_ctx) {
	ctx_t *ctx = (ctx_t*)_ctx;
	while (1) {
		report("reindeer", "going on vacation\n");
		nap(5*SECONDS, 10*SECONDS);
		assert(pthread_mutex_lock(&ctx->lock) == 0);
		ctx->reindeer++;
		report("reindeer", "back from vacation - %d reindeer at home\n", ctx->reindeer);
		if (ctx->reindeer >= MAXREINWAIT) {
			report("reindeer", "waking santa\n");
			pthread_cond_signal(&ctx->cond);
		}
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
		printf("Out of memory\n");
		exit(1);
	}
	init_ctx(ctx);

	pthread_t reindeer[MAXREIN];
	pthread_t elves[MAXELF];
	pthread_t santa;

	pthread_create(&santa, NULL, santa_thread, (void*)ctx);
	for (int i = 0; i < MAXELF; i++)
		pthread_create(&elves[i], NULL, elf_thread, (void*)ctx);
	for (int i = 0; i < MAXREIN; i++)
		pthread_create(&reindeer[i], NULL, reindeer_thread, (void*)ctx);

	pthread_join(santa, NULL);

	return 0; // Will clean up other threads, can't pthread_cancel because some threads may wait on mutexes or semaphores
}
