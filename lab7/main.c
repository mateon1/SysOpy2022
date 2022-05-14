// Mateusz Naściszewski, 2022
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <wait.h>
#include <sys/mman.h>
#include <sys/time.h>

#ifdef POSIX
#include <sys/stat.h>
#include <semaphore.h>
#define PROGNAME "posix"
#define SHMNAME "/yxgksv_SO2022L07"
#else
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#define PROGNAME "sysv"
#endif

#ifndef SLOTS
#define SLOTS 5
#endif

struct shared {
	int ovenslots[SLOTS];
	int tableslots[SLOTS];
	int tablehead;
	int tabletail;
	int inoven;
	int ontable;
};

enum semaphores {
	OVEN_LK = 0,
	OVEN_CAP,
	TABLE_LK,
	TABLE_CAP,
	TABLE_AVAIL
};

#ifdef POSIX
static int shmfd;
static sem_t* sem[5];
#else
static char *cwd;
static key_t sysvkey;
static int shmid;
static int semid;
#endif
static struct shared *shmdata;

static void setup_shm(void) {
#ifdef POSIX
	if ((shmfd = shm_open(SHMNAME, O_RDWR | O_CREAT, 0660)) < 0) { perror("shm_open"); exit(1); }
	if (ftruncate(shmfd, sizeof(struct shared)) < 0) { perror("ftruncate"); exit(1); }
	if ((shmdata = mmap(NULL, sizeof(struct shared), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0)) == MAP_FAILED) { perror("mmap"); exit(1); }
	errno = 0; if (sem_unlink(SHMNAME ".oven.lk") < 0 && errno != ENOENT) { perror("sem_unlink"); exit(1); }
	errno = 0; if (sem_unlink(SHMNAME ".oven.cap") < 0 && errno != ENOENT) { perror("sem_unlink"); exit(1); }
	errno = 0; if (sem_unlink(SHMNAME ".table.lk") < 0 && errno != ENOENT) { perror("sem_unlink"); exit(1); }
	errno = 0; if (sem_unlink(SHMNAME ".table.cap") < 0 && errno != ENOENT) { perror("sem_unlink"); exit(1); }
	errno = 0; if (sem_unlink(SHMNAME ".table.avail") < 0 && errno != ENOENT) { perror("sem_unlink"); exit(1); }
	if ((sem[OVEN_LK]     = sem_open(SHMNAME ".oven.lk", O_RDWR | O_CREAT, 0660, 1)) == SEM_FAILED) { perror("sem_open"); exit(1); }
	if ((sem[OVEN_CAP]    = sem_open(SHMNAME ".oven.cap", O_RDWR | O_CREAT, 0660, SLOTS)) == SEM_FAILED) { perror("sem_open"); exit(1); }
	if ((sem[TABLE_LK]    = sem_open(SHMNAME ".table.lk", O_RDWR | O_CREAT, 0660, 1)) == SEM_FAILED) { perror("sem_open"); exit(1); }
	if ((sem[TABLE_CAP]   = sem_open(SHMNAME ".table.cap", O_RDWR | O_CREAT, 0660, SLOTS)) == SEM_FAILED) { perror("sem_open"); exit(1); }
	if ((sem[TABLE_AVAIL] = sem_open(SHMNAME ".table.avail", O_RDWR | O_CREAT, 0660, 0)) == SEM_FAILED) { perror("sem_open"); exit(1); }
#else
	if ((cwd = getcwd(NULL, 0)) == NULL) { perror("getcwd"); exit(1); }
	if ((sysvkey = ftok(cwd, 'K')) < 0) { perror("ftok"); exit(1); }
	if ((shmid = shmget(sysvkey, sizeof(struct shared), IPC_CREAT | 0660)) < 0) { perror("shmget"); exit(1); }
	if ((shmdata = shmat(shmid, NULL, 0)) == MAP_FAILED) { perror("shmat"); exit(1); }
	if ((semid = semget(sysvkey, 5, IPC_CREAT | 0660)) < 0) { perror("shmget"); exit(1); }

	unsigned short values[] = {1, SLOTS, 1, SLOTS, 0};

	//struct sembuf ops[] = {
	//	(struct sembuf){ .sem_num = OVEN_LK, .sem_op = 1, .sem_flg = 0 },
	//	(struct sembuf){ .sem_num = OVEN_CAP, .sem_op = SLOTS, .sem_flg = 0 },
	//	(struct sembuf){ .sem_num = TABLE_LK, .sem_op = 1, .sem_flg = 0 },
	//	(struct sembuf){ .sem_num = TABLE_CAP, .sem_op = SLOTS, .sem_flg = 0 },
	//};
	//if (semop(semid, ops, sizeof(ops)/sizeof(ops[0])) < 0) { perror("semop"); exit(1); }
	if (semctl(semid, 0, SETALL, values) < 0) { perror("semctl"); exit(1); }
#endif

	for (int i = 0; i < SLOTS; i++) {
		shmdata->ovenslots[i] = -1;
		shmdata->tableslots[i] = -1;
	}
	shmdata->tablehead = 0;
	shmdata->tabletail = 0;
	shmdata->inoven = 0;
	shmdata->ontable = 0;
}

static void load_shm(void) {
#ifdef POSIX
	if ((shmfd = shm_open(SHMNAME, O_RDWR, 0660)) < 0) { perror("shm_open"); exit(1); }
	if ((shmdata = mmap(NULL, sizeof(struct shared), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0)) == MAP_FAILED) { perror("mmap"); exit(1); }
	if ((sem[OVEN_LK]     = sem_open(SHMNAME ".oven.lk", O_RDWR)) == SEM_FAILED) { perror("sem_open"); exit(1); }
	if ((sem[OVEN_CAP]    = sem_open(SHMNAME ".oven.cap", O_RDWR)) == SEM_FAILED) { perror("sem_open"); exit(1); }
	if ((sem[TABLE_LK]    = sem_open(SHMNAME ".table.lk", O_RDWR)) == SEM_FAILED) { perror("sem_open"); exit(1); }
	if ((sem[TABLE_CAP]   = sem_open(SHMNAME ".table.cap", O_RDWR)) == SEM_FAILED) { perror("sem_open"); exit(1); }
	if ((sem[TABLE_AVAIL] = sem_open(SHMNAME ".table.avail", O_RDWR)) == SEM_FAILED) { perror("sem_open"); exit(1); }
#else
	if ((cwd = getcwd(NULL, 0)) == NULL) { perror("getcwd"); exit(1); }
	if ((sysvkey = ftok(cwd, 'K')) < 0) { perror("ftok"); exit(1); }
	if ((shmid = shmget(sysvkey, sizeof(struct shared), 0660)) < 0) { perror("shmget"); exit(1); }
	if ((shmdata = shmat(shmid, NULL, 0)) == MAP_FAILED) { perror("shmat"); exit(1); }
	if ((semid = semget(sysvkey, 5, 0660)) < 0) { perror("shmget"); exit(1); }
#endif
}

__attribute__((format(printf, 1, 2)))
static void report(const char *fmt, ...) {
	struct timeval tv;
	if (gettimeofday(&tv, NULL) < 0) { perror("gettimeofday"); exit(1); }
	long hr, min, sec;
	sec = tv.tv_sec;
	min = sec / 60;
	hr = min / 60;
	sec %= 60;
	min %= 60;
	hr %= 24;
	printf("[%2ld:%02ld:%02ld.%ld] [PID %d] ", hr, min, sec, tv.tv_usec / 100000, getpid());
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	printf("\n");
	fflush(stdout);
}

#define SEC 1000000000L
static void nap(long min, long max) {
	long dur = max == min ? min : min + rand() % (max - min);
	struct timespec sleeplen = { .tv_sec = dur / SEC, .tv_nsec = dur % SEC };
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

static void oven_before_insert(void) {
#ifdef POSIX
	if (sem_wait(sem[OVEN_CAP]) < 0) { perror("sem_wait"); exit(1); }
	if (sem_wait(sem[OVEN_LK]) < 0) { perror("sem_wait"); exit(1); }
#else
	struct sembuf ops[] = {
		(struct sembuf){ .sem_num = OVEN_CAP, .sem_op = -1, .sem_flg = 0 },
		(struct sembuf){ .sem_num = OVEN_LK, .sem_op = -1, .sem_flg = 0 }
	};
	if (semop(semid, ops, sizeof(ops)/sizeof(ops[0])) < 0) { perror("semop"); exit(1); }
#endif
}

static void oven_after_insert(void) {
#ifdef POSIX
	if (sem_post(sem[OVEN_LK]) < 0) { perror("sem_post"); exit(1); }
#else
	struct sembuf ops[] = {
		(struct sembuf){ .sem_num = OVEN_LK, .sem_op = 1, .sem_flg = 0 }
	};
	if (semop(semid, ops, sizeof(ops)/sizeof(ops[0])) < 0) { perror("semop"); exit(1); }
#endif
}

static void oven_before_remove(void) {
#ifdef POSIX
	if (sem_wait(sem[OVEN_LK]) < 0) { perror("sem_wait"); exit(1); }
#else
	struct sembuf ops[] = {
		(struct sembuf){ .sem_num = OVEN_LK, .sem_op = -1, .sem_flg = 0 }
	};
	if (semop(semid, ops, sizeof(ops)/sizeof(ops[0])) < 0) { perror("semop"); exit(1); }
#endif
}

static void oven_after_remove(void) {
#ifdef POSIX
	if (sem_post(sem[OVEN_CAP]) < 0) { perror("sem_post"); exit(1); }
	if (sem_post(sem[OVEN_LK]) < 0) { perror("sem_post"); exit(1); }
#else
	struct sembuf ops[] = {
		(struct sembuf){ .sem_num = OVEN_CAP, .sem_op = 1, .sem_flg = 0 },
		(struct sembuf){ .sem_num = OVEN_LK, .sem_op = 1, .sem_flg = 0 }
	};
	if (semop(semid, ops, sizeof(ops)/sizeof(ops[0])) < 0) { perror("semop"); exit(1); }
#endif
}

static void table_before_put(void) {
#ifdef POSIX
	if (sem_wait(sem[TABLE_CAP]) < 0) { perror("sem_wait"); exit(1); }
	if (sem_wait(sem[TABLE_LK]) < 0) { perror("sem_wait"); exit(1); }
#else
	struct sembuf ops[] = {
		(struct sembuf){ .sem_num = TABLE_CAP, .sem_op = -1, .sem_flg = 0 },
		(struct sembuf){ .sem_num = TABLE_LK, .sem_op = -1, .sem_flg = 0 }
	};
	if (semop(semid, ops, sizeof(ops)/sizeof(ops[0])) < 0) { perror("semop"); exit(1); }
#endif
}

static void table_after_put(void) {
#ifdef POSIX
	if (sem_post(sem[TABLE_LK]) < 0) { perror("sem_post"); exit(1); }
	if (sem_post(sem[TABLE_AVAIL]) < 0) { perror("sem_post"); exit(1); }
#else
	struct sembuf ops[] = {
		(struct sembuf){ .sem_num = TABLE_LK, .sem_op = 1, .sem_flg = 0 },
		(struct sembuf){ .sem_num = TABLE_AVAIL, .sem_op = 1, .sem_flg = 0 }
	};
	if (semop(semid, ops, sizeof(ops)/sizeof(ops[0])) < 0) { perror("semop"); exit(1); }
#endif
}

static void table_before_get(void) {
#ifdef POSIX
	if (sem_wait(sem[TABLE_AVAIL]) < 0) { perror("sem_wait"); exit(1); }
	if (sem_wait(sem[TABLE_LK]) < 0) { perror("sem_wait"); exit(1); }
#else
	struct sembuf ops[] = {
		(struct sembuf){ .sem_num = TABLE_AVAIL, .sem_op = -1, .sem_flg = 0 },
		(struct sembuf){ .sem_num = TABLE_LK, .sem_op = -1, .sem_flg = 0 }
	};
	if (semop(semid, ops, sizeof(ops)/sizeof(ops[0])) < 0) { perror("semop"); exit(1); }
#endif
}

static void table_after_get(void) {
#ifdef POSIX
	if (sem_post(sem[TABLE_LK]) < 0) { perror("sem_post"); exit(1); }
	if (sem_post(sem[TABLE_CAP]) < 0) { perror("sem_post"); exit(1); }
#else
	struct sembuf ops[] = {
		(struct sembuf){ .sem_num = TABLE_LK, .sem_op = 1, .sem_flg = 0 },
		(struct sembuf){ .sem_num = TABLE_CAP, .sem_op = 1, .sem_flg = 0 }
	};
	if (semop(semid, ops, sizeof(ops)/sizeof(ops[0])) < 0) { perror("semop"); exit(1); }
#endif
}

static int cook_main(void) {
	while (true) {
		int type = rand() % 10;
		report("Preparing pizza %d", type);

		nap(1 * SEC, 2 * SEC);

		oven_before_insert();
		int slot;
		for (slot = 0; slot < SLOTS; slot++) {
			if (shmdata->ovenslots[slot] < 0) {
				break;
			}
		}
		assert(shmdata->ovenslots[slot] < 0);
		shmdata->ovenslots[slot] = type;
		shmdata->inoven++;
		report("Cooking pizza %d, %d in oven", type, shmdata->inoven);
		oven_after_insert();

		nap(4 * SEC, 5 * SEC);

		oven_before_remove();
		int cooked = shmdata->ovenslots[slot];
		shmdata->ovenslots[slot] = -1;
		shmdata->inoven--;
		oven_after_remove();

		table_before_put();
		shmdata->tableslots[shmdata->tablehead++] = cooked;
		shmdata->tablehead %= SLOTS;
		shmdata->ontable++;
		report("Cooked pizza %d, %d in oven, %d on table", cooked, shmdata->inoven, shmdata->ontable);
		table_after_put();
	}
	return 1;
}

static int deliv_main(void) {
	while (true) {
		table_before_get();
		int pizza = shmdata->tableslots[shmdata->tabletail];
		shmdata->tableslots[shmdata->tabletail++] = -1;
		assert(pizza >= 0);
		shmdata->tabletail %= SLOTS;
		shmdata->ontable--;
		report("Took pizza %d, %d on table", pizza, shmdata->ontable);
		table_after_get();

		nap(4 * SEC, 5 * SEC);

		report("Delivered pizza %d", pizza);

		nap(4 * SEC, 5 * SEC);
	}
	return 1;
}

static bool parse_int(const char *str, long *out) {
	if (*str == '\0') return false;
	char *endptr;
	errno = 0;
	long res = strtol(str, &endptr, 10);
	*out = res;
	return errno == 0 && *endptr == '\0';
}

int main(int argc, char** argv) {
	srand(time(NULL) + getpid());
	bool cook, deliv;
	if (argc == 2 && ((cook = strcmp(argv[1], "cook") == 0) || (deliv = strcmp(argv[1], "deliv")))) {
		load_shm();
		if (cook) {
			return cook_main();
		} else {
			return deliv_main();
		}
	}

	long N, M;
	if (argc != 3 || !parse_int(argv[1], &N) || N < 1 || !parse_int(argv[2], &M) || M < 1) {
		printf("Usage: %s <cook threads> <delivery threads>\n", argc > 0 ? argv[0] : PROGNAME);
		exit(2);
	}

	setup_shm();

	for (long i = 0; i < N; i++) {
		if (fork() == 0) {
			execv("./" PROGNAME, (char*[]){"./" PROGNAME, "cook", NULL});
		}
	}
	for (long i = 0; i < M; i++) {
		if (fork() == 0) {
			execv("./" PROGNAME, (char*[]){"./" PROGNAME, "dlv", NULL});
		}
	}

	bool errored = false;
	for (long i = 0; i < M + N; i++) {
		int wstatus;
		if (wait(&wstatus) < 0) {
			errored = true;
			perror("wait() failed");
			continue;
		}
	}

	if (errored) return 1;

	return 0;
}
