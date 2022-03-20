#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <sys/stat.h>

#ifdef NFTW
#define PROGNAME "main-nftw"
#include <ftw.h>

static int nftw_func(const char *fpath, const struct stat *sb, int type, struct FTW *ftwbuf);
#else
#define PROGNAME "main-dir"
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#endif

static struct stats {
	unsigned n_file;
	unsigned n_dir;
	unsigned n_char;
	unsigned n_blk;
	unsigned n_fifo;
	unsigned n_sym;
	unsigned n_sock;
	unsigned n_fail;

	uint64_t total_bytes;
} stats;

static void report(const char *fpath, const struct stat *sb) {
	static char timebuf[32];
	struct tm *time;
	char *desc = "????";
	switch (sb->st_mode & S_IFMT) {
		case S_IFSOCK:desc = "sock"; stats.n_sock++; break;
		case S_IFLNK: desc = "syml"; stats.n_sym++; break;
		case S_IFREG: desc = "file"; stats.n_file++; break;
		case S_IFBLK: desc = "bdev"; stats.n_blk++; break;
		case S_IFDIR: desc = "dir "; stats.n_dir++; break;
		case S_IFCHR: desc = "cdev"; stats.n_char++; break;
		case S_IFIFO: desc = "fifo"; stats.n_fifo++; break;
		default:
			fprintf(stderr, "Error: %s\nunknown mode: %07o\n", fpath, sb->st_mode);
			stats.n_fail++;
			return;
	}
	printf("%s\t%lu\t%s\t%ld\t", fpath, sb->st_nlink, desc, sb->st_size);
	stats.total_bytes += sb->st_size;
	time = gmtime(&sb->st_atime);
	if (time != NULL) {
		strftime(timebuf, sizeof(timebuf), "%F %T", time);
		printf("%s\t", timebuf);
	} else {
		printf("???\t");
	}
	time = gmtime(&sb->st_mtime);
	if (time != NULL) {
		strftime(timebuf, sizeof(timebuf), "%F %T", time);
		printf("%s\n", timebuf);
	} else {
		printf("???\n");
	}
}

#ifdef NFTW
static int nftw_func(const char *fpath, const struct stat *sb, int type, struct FTW *ftwbuf) {
	if (type == FTW_NS) { // failed stat() call
		stats.n_fail++;
		printf("%s\t?\t????\t?\t???\t???\n", fpath);
		return 0; // early return, ignore stat struct contents as they are unspecified
	}
	report(fpath, sb);
	return 0;
}

static void walk(const char* path) {
	int err = nftw(path, nftw_func, 8, FTW_PHYS);
	if (err != 0) {
		fprintf(stderr, "nftw() failed, returned %d!\n", err);
		exit(2);
	}
}
#else
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

static uid_t uid;
static gid_t gid;

// Return whether to recurse into the path
static bool process(const char* path) {
	struct stat sb;
	if (lstat(path, &sb) < 0) {
		printf("%s\t?\t????\t?\t???\t???\n", path);
		stats.n_fail++;
		return false;
	}
	report(path, &sb);
	return S_ISDIR(sb.st_mode) && (
		sb.st_uid == uid ? (sb.st_mode & S_IRUSR) > 0 && (sb.st_mode & S_IXUSR) > 0 :
		sb.st_gid == gid ? (sb.st_mode & S_IRGRP) > 0 && (sb.st_mode & S_IXGRP) > 0 :
		                   (sb.st_mode & S_IROTH) > 0 && (sb.st_mode & S_IXOTH) > 0
	);
}

static DIR* dirs[256];
static struct sizedstr fullpath;
static void walk(const char* path) {
	uid = geteuid();
	gid = getegid();

	unsigned depth = 0;
	ss_assign(&fullpath, path);

	if (!process(fullpath.data)) return;
	dirs[0] = opendir(fullpath.data);
	if (dirs[0] == NULL) {
		perror("Could not open search directory");
		exit(1);
	}

	while (true) {
		errno = 0;
		struct dirent *ent = readdir(dirs[depth]);
		if (ent == NULL && errno == 0) {
			//printf("End of dir: %s\n", fullpath.data);
			if (closedir(dirs[depth]) != 0) {
				perror("Failed to close directory");
				exit(1);
			}
			if (depth == 0) break;
			depth--;
			char *popped = ss_pop(&fullpath);
			assert(popped != NULL);
		} else if (ent == NULL) {
			perror("Failed to read directory");
			exit(1);
		} else {
			if (ent->d_name[0] == '.' && (ent->d_name[1] == '\0' || (ent->d_name[1] == '.' && ent->d_name[2] == '\0'))) {
				continue; // Ignore . and .. entries
			}
			//printf("Dir entry: %s - entry %s\n", fullpath.data, ent->d_name);
			ss_push(&fullpath, ent->d_name);
			//printf("%s\n", fullpath.data);
			if (process(fullpath.data)) {
				//printf("Recursing into dir %s\n", fullpath.data);
				dirs[++depth] = opendir(fullpath.data);
				if (dirs[depth] == NULL) {
					perror("Could not open directory");
					exit(1);
				}
			} else {
				char *popped = ss_pop(&fullpath);
				assert(popped != NULL);
				//printf("Processed entry, now in: %s\n", fullpath.data);
			}
		}
	}

	ss_free(&fullpath);
}
#endif

int main(int argc, char** argv) {
	if (argc != 2) {
		printf("Usage: %s <dir path>\n", argc > 0 ? argv[0] : PROGNAME);
		exit(2);
	}
	char* path = realpath(argv[1], NULL);
	if (path == NULL) {
		perror("Could not obtain realpath");
		exit(1);
	}
	walk(path);

	printf("Files:      %u\n", stats.n_file);
	printf("Dirs:       %u\n", stats.n_dir);
	printf("Char devs:  %u\n", stats.n_char);
	printf("Block devs: %u\n", stats.n_blk);
	printf("FIFOs:      %u\n", stats.n_fifo);
	printf("Symlinks:   %u\n", stats.n_sym);
	printf("Sockets:    %u\n", stats.n_sock);
	printf("\n");
	printf("Bytes total: %lu\n", stats.total_bytes);

	if (stats.n_fail > 0) {
		fprintf(stderr, "Failed to read %u files total\n", stats.n_fail);
		exit(1);
	}

	return 0;
}
