// Mateusz Naściszewski, 2022
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

static size_t next_size(size_t x) {
	x++; // add 1, so size always increases, even for 2^k-1
	assert(x > 0); // Doesn't handle max value well
	if (x < 7) return 7; // optimization, don't do tiny allocations
	while ((x & (x - 1)) != 0) { // while isn't a power of two
		x &= (x - 1); // clear lowest bit
	}
	x <<= 1;
	x--; // power of two minus one
	return x;
}

// Sized string type. ->cap and ->len do not count terminating NUL byte.
struct sizedstr {
	size_t cap;
	size_t len;
	char *data;
};

static void ss_alloc(struct sizedstr *ss, size_t len) {
	ss->cap = len;
	ss->len = 0;
	ss->data = calloc(len + 1, 1);
}

static void ss_realloc(struct sizedstr *ss, size_t newlen) {
	assert(newlen > ss->cap);
	char *olddata = ss->data;
	ss->data = calloc(newlen + 1, 1);
	if (ss->data != NULL) {
		strcpy(ss->data, olddata);
		ss->cap = newlen;
	}
	free(olddata);
}

static void ss_free(struct sizedstr *ss) {
	assert(ss->data != NULL);
	free(ss->data);
	ss->data = NULL;
	ss->len = 0;
	ss->cap = 0;
}

static void ss_ensure(struct sizedstr *ss, size_t len, bool realloc) {
	if (len > ss->cap) {
		size_t wanted = next_size(len - 1);
		assert(wanted >= len);
		if (realloc) {
			ss_realloc(ss, wanted);
		} else {
			if (ss->data != NULL) ss_free(ss);
			ss_alloc(ss, wanted);
		}
		if (ss->data == NULL) {
			fprintf(stderr, "String allocation failed\n");
			exit(65);
		}
	}
}

static void ss_assign(struct sizedstr *ss, const char* s) {
	size_t newlen = strlen(s);
	ss_ensure(ss, newlen, false);
	strcpy(ss->data, s);
	ss->len = newlen;
}

static void ss_push(struct sizedstr *ss, const char* name) {
	size_t extra = strlen(name);
	ss_ensure(ss, ss->len + extra + 1, true);
	ss->data[ss->len++] = '/';
	strcpy(ss->data + ss->len, name);
	ss->len += extra;
}

// Replaces last slash with NUL and returns pointer to the leftover data - valid while no other functions called
// Returns NULL without modification if no slash is present
static char* ss_pop(struct sizedstr *ss) {
	char* ptr = strrchr(ss->data, '/');
	if (ptr != NULL) {
		ss->len = ptr - ss->data;
		*ptr++ = '\0';
	}
	return ptr;
}

static char* needle;
static uid_t uid;
static gid_t gid;
static long depth;
static struct sizedstr curpath;

#define HAVEPERMS(sb, kind) \
	(\
		sb.st_uid == uid ? (sb.st_mode & S_I##kind##USR) > 0 :\
		sb.st_gid == gid ? (sb.st_mode & S_I##kind##GRP) > 0 :\
						   (sb.st_mode & S_I##kind##OTH) > 0)

// Return whether to recurse into the path
static bool process(const char* path) {
	struct stat sb;
	if (lstat(path, &sb) < 0) { // Usually fails due to file moving / getting deleted between readdir() and stat()
		fprintf(stderr, "failed stat: %s\n", path);
		return false;
	}
	if (S_ISREG(sb.st_mode) && HAVEPERMS(sb, R)) { // readable file
		bool dosearch = true;
		bool shebangchk = false;
		if ((sb.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) == 0) {
			// Not executable
			// Filter common binary files by name
			char *ptr;
			if ((ptr = strstr(path, ".o")) != NULL && *(ptr + 2) == '\0') {
				// object file
				dosearch = false;
			} else if ((ptr = strstr(path, ".png")) != NULL && *(ptr + 4) == '\0') {
				// image file
				dosearch = false;
			} else if ((ptr = strstr(path, ".gz")) != NULL && *(ptr + 3) == '\0') {
				// compressed file
				dosearch = false;
			} else if ((ptr = strstr(path, ".zip")) != NULL && *(ptr + 4) == '\0') {
				// compressed file
				dosearch = false;
			} else if ((ptr = strstr(path, ".core")) != NULL && *(ptr + 5) == '\0') {
				// core dump
				dosearch = false;
			}
			// Non-exhaustive list, obviously
		} else {
			// It's executable, but maybe a script file. Check if it starts with a shebang or not
			shebangchk = true;
		}

		if (dosearch) {
			int fd = open(path, O_RDONLY);
			if (fd == -1) {
				perror("Failed to open file");
				exit(1);
			}
			bool first = true;
			bool found = false;
			size_t needlesize = strlen(needle);
			ssize_t nbuf = 0;
			char buf[4096];
			while (true) {
				nbuf = read(fd, first ? buf : buf + needlesize, first ? sizeof(buf) : sizeof(buf) - needlesize);
				if (nbuf == -1) {
					perror("Failed to read from file");
					exit(1);
				}
				if (nbuf == 0) break;
				if (!first) nbuf += needlesize;
				if (first) {
					if (shebangchk && nbuf >= 2 && strncmp(buf, "#!", 2) != 0) {
						// binary executable
						break;
					}
				}
				char *ptr = memmem(buf, nbuf, needle, needlesize);
				if (ptr != NULL) {
					found = true;
					break;
				}
				assert(nbuf >= (ssize_t)needlesize);
				memmove(buf, buf + nbuf - needlesize, needlesize);
				first = false;
			}
			if (close(fd) == -1) {
				perror("Close failed");
				exit(1);
			}
			if (found) {
				printf("%s\tPID %d\n", curpath.data, getpid());
			}
		}
	}
	return S_ISDIR(sb.st_mode) && HAVEPERMS(sb, R) && HAVEPERMS(sb, X);
}

static __attribute__((noreturn)) void walk(void) {
	uid = geteuid();
	gid = getegid();

	DIR* dir = opendir(curpath.data);
	if (dir == NULL) {
		perror("Could not open search directory");
		exit(1);
	}

	int children = 0;

	while (true) {
		errno = 0;
		struct dirent *ent = readdir(dir);
		if (ent == NULL && errno == 0) {
			//printf("End of dir: %s\n", curpath.data);
			if (closedir(dir) != 0) {
				perror("Failed to close directory");
				exit(1);
			}
			break;
		} else if (ent == NULL) {
			perror("Failed to read directory"); // Happens for /net directory for zombie processes in /proc
			fprintf(stderr, "%s\n", curpath.data);
			exit(1);
		} else {
			if (ent->d_name[0] == '.' && (ent->d_name[1] == '\0' || (ent->d_name[1] == '.' && ent->d_name[2] == '\0'))) {
				continue; // Ignore . and .. entries
			}
			//printf("Dir entry: %s - entry %s\n", curpath.data, ent->d_name);
			ss_push(&curpath, ent->d_name);
			//printf("%s\n", curpath.data);
			if (process(curpath.data) && depth > 1) {
				pid_t cpid = fork();
				if (cpid == -1) {
					perror("fork failed");
					for (int i = 0; i < children; i++) {
						int status;
						if (wait(&status) == -1) {
							perror("Failed to wait()");
							// ignore nested error, still try to wait to prevent zombies
						}
					}
					exit(1);
				}
				if (cpid == 0) {
					--depth;
					walk(); // noreturn
				}
				children++;
			}
			char *popped = ss_pop(&curpath);
			assert(popped != NULL);
			//printf("Processed entry, now in: %s\n", curpath.data);
		}
	}

	int exitval = 0;
	for (int i = 0; i < children; i++) {
		int status;
		if (wait(&status) == -1) {
			perror("Failed to wait() for child");
			// Don't exit, wait for the other children to prevent zombies
			exitval = 1;
		}
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
			fprintf(stderr, "Child failed\n");
			exitval = 1;
		}
	}

	exit(exitval);
}

static bool parse_int(char *str, long *out) {
	if (*str == '\0') return false;
	char *endptr;
	long res = strtol(str, &endptr, 10);
	*out = res;
	return *endptr == '\0';
}

int main(int argc, char** argv) {
	if (argc != 4 || !parse_int(argv[3], &depth) || depth <= 0) {
		printf("Usage: %s <path> <search string> <max depth>\n", argc > 0 ? argv[0] : "main");
		exit(2);
	}

	needle = argv[2];
	if (strlen(needle) < 1) {
		printf("Search string must be at least one character long\n");
		exit(2);
	}

	pid_t cpid = fork();
	if (cpid == -1) {
		perror("fork failed");
		exit(1);
	}
	if (cpid == 0) {
		if (chdir(argv[1]) == -1) {
			perror("chdir() to search directory failed");
			exit(1);
		}
		ss_assign(&curpath, ".");
		walk(); // noreturn
	}

	int status;
	if (wait(&status) == -1) {
		perror("Failed to wait() for child");
		exit(1);
	}
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		fprintf(stderr, "Child failed\n");
		exit(1);
	}

	return 0;
}
