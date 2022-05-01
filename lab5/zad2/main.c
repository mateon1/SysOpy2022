// Mateusz Naściszewski, 2022
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

static void mail_headers(int sort_method) {
	FILE *fmail = popen("mail -H | cut -c3-", "r"); // cut off the stupid markers at the beginning, they break sort by field number
	if (!fmail) {
		perror("popen failed");
		exit(1);
	}
	FILE *fsort = popen(sort_method == 1 ? "sort -k2,2" : "cat", "w"); // already sorted by date?
	if (!fsort) {
		perror("popen failed");
		exit(1);
	}

	char buf[4096];
	int nread;
	while ((nread = fread(buf, 1, sizeof(buf), fmail)) > 0) {
		if (fwrite(buf, nread, 1, fsort) <= 0) {
			fprintf(stderr, "Short write, aborting");
			if (pclose(fmail) < 1) perror("pclose failed");
			if (pclose(fsort) < 1) perror("pclose failed");
			exit(1);
		}
	}

	int mret = pclose(fmail);
	if (mret < 0) perror("pclose failed");
	int sret = pclose(fsort);
	if (sret < 0) perror("pclose failed");
	if (mret < 0 || sret < 0) exit(1);
}

static void mail_compose(char *to, char *subject, char *contents) {
	char *cmdline = calloc(strlen(to) * 2 + 8, 1);
	{
		char *p = cmdline;
		*p++ = 'm';
		*p++ = 'a';
		*p++ = 'i';
		*p++ = 'l';
		*p++ = ' ';
		while (*to) { *p++ = '\\'; *p++ = *to++; }
		*p = '\0';
	}
	FILE *f = popen(cmdline, "w");
	free(cmdline);
	if (!f) {
		perror("popen failed");
		exit(1);
	}

#define TRYWRITE(str, len) do {\
	if (fwrite((str), (len), 1, f) <= 0) {\
		fprintf(stderr, "Short write, aborting");\
		if (pclose(f) < 1) perror("pclose failed");\
		exit(1);\
	}\
} while (0);

	// No CC: header
	TRYWRITE("\n", 1);
	// Subject:
	TRYWRITE(subject, strlen(subject));
	TRYWRITE("\n", 1);
	// Message body
	TRYWRITE(contents, strlen(contents));
	TRYWRITE("\n", 1);

#undef TRYWRITE

	int ret = pclose(f);
	if (ret < 0) {
		perror("pclose failed");
		exit(1);
	}
}

static __attribute__((noreturn)) void usage_and_exit(char *pname) {
	printf("Usage:\n  %s <\"date\"|\"author\">\n  %s <email address> <subject> <contents>\n", pname, pname);
	exit(2);
}

int main(int argc, char** argv) {
	char *pname = argc > 0 ? argv[0] : "main";
	if (argc != 2 && argc != 4)
		usage_and_exit(pname);

	if (argc == 2) {
		if (strcmp(argv[1], "date") == 0)
			mail_headers(0);
		else if (strcmp(argv[1], "author") == 0)
			mail_headers(1);
		else
			usage_and_exit(pname);
	} else {
		assert(argc == 4);
		mail_compose(argv[1], argv[2], argv[3]);
	}

	return 0;
}
