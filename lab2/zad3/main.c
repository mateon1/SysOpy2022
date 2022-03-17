#define _XOPEN_SOURCE 500
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef NFTW
#define PROGNAME "main-nftw"
#include <ftw.h>

static int nftw_func(const char *fpath, const struct stat *sb, int type, struct FTW *ftwbuf);
#else
#define PROGNAME "main-dir"
#endif

static struct stats {
	unsigned n_file;
	unsigned n_dir;
	unsigned n_char;
	unsigned n_blk;
	unsigned n_fifo;
	unsigned n_sym;
	unsigned n_sock;

	uint64_t total_bytes;
} stats;


#ifdef NFTW
static int nftw_func(const char *fpath, const struct stat *sb, int type, struct FTW *ftwbuf) {
	stats.total_bytes++;
	return 0;
}

void walk(char* path) {
	int err = nftw(path, nftw_func, 8, FTW_PHYS);
	if (err != 0) {
		fprintf(stderr, "nftw() failed, returned %d!\n", err);
		exit(2);
	}
}
#else
void walk(char* path) {}
#endif

int main(int argc, char** argv) {
	if (argc != 2) {
		printf("Usage: %s <dir path>\n", argc > 0 ? argv[0] : PROGNAME);
		exit(2);
	}
	walk(argv[1]);

	printf("Files:      %u\n", stats.n_file);
	printf("Dirs:       %u\n", stats.n_dir);
	printf("Char devs:  %u\n", stats.n_char);
	printf("Block devs: %u\n", stats.n_blk);
	printf("FIFOs:      %u\n", stats.n_fifo);
	printf("Symlinks:   %u\n", stats.n_sym);
	printf("Sockets:    %u\n", stats.n_sock);
	printf("\n");
	printf("Bytes total: %lu\n", stats.total_bytes);

	return 0;
}
