// Mateusz Naściszewski, 2022
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <ctype.h> // isspace

#ifdef SYS
#include <fcntl.h>
#include <unistd.h>
#define PROGNAME "main-sys"
#define WRITE(buf, nbytes) do {\
        ssize_t total = 0; \
        while (total < (nbytes)) { \
            ssize_t written = write(fdout, (buf) + total, (nbytes) - total); \
            if (written == -1) { \
                perror("Write failed"); \
                exit(1); \
            } \
            total += written; \
        } \
    } while (0)
#else
#define PROGNAME "main-lib"
#define WRITE(buf, nbytes) do {\
        ssize_t written = fwrite((buf), 1, (nbytes), ofile); \
        if (written < (nbytes)) { /* Can't reliably distinguish errors here */ \
            perror("Write failed"); \
            exit(1); \
        } \
    } while (0)
#endif

int main(int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: %s <input file> <output file>\n", argc > 0 ? argv[0] : PROGNAME);
        exit(2);
    }
#ifdef SYS
    int fdin = open(argv[1], O_RDONLY);
    if (fdin == -1) {
        perror("Failed to open input file");
        exit(1);
    }
    int fdout = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fdout == -1) {
        perror("Failed to open output file");
        exit(1);
    }
#else
    FILE *ifile = fopen(argv[1], "r");
    if (ifile == NULL) {
        perror("Failed to open file");
        exit(1);
    }
    FILE *ofile = fopen(argv[2], "w");
    if (ofile == NULL) {
        perror("Failed to open file");
        exit(1);
    }
#endif

    off_t curseekoff = 0, lineseekoff = 0;
    int curpos = 0, linepos = 0;
    ssize_t nbuf = 0;
    char buf[4096];

    bool writing = false;
    bool postseek = false;

    do {
read:
#ifdef SYS
        nbuf = read(fdin, buf, sizeof(buf));
        if (nbuf == -1) {
            perror("Failed to read from file");
            exit(1);
        }
#else
        curseekoff = ftell(ifile);
        nbuf = (ssize_t) fread(buf, 1, sizeof(buf), ifile);
        if (nbuf == 0 && ferror(ifile) != 0) {
            perror("Failed to read from file");
            exit(1);
        }
#endif
        if (postseek) {
            curpos = linepos;
            curseekoff = lineseekoff;
        } else {
            curpos = 0;
        }
        postseek = false;
        // High level overview:
        // Scanning mode:
        //   When you see a newline, remember where it started (seek offset + buffer offset)
        //   When you see a space character, consume it, ignore it
        //   When you see a nonspace character, go to start of line, go into writing mode.
        // Writing mode:
        //   Write everything up to a newline.
        //   Then record location of line start, go to scanning mode.
        while (curpos < nbuf) {
            if (writing) {
                char *eol = (char*)memchr(buf + curpos, '\n', nbuf - curpos);
                if (eol == NULL) {
                    WRITE(buf + curpos, nbuf - curpos);
                    curpos = nbuf;
                } else {
                    WRITE(buf + curpos, eol - (buf + curpos) + 1);
                    curpos = eol - buf + 1;
                    lineseekoff = curseekoff;
                    linepos = curpos; // one past newline
                    writing = false;
                }
            } else {
                if (buf[curpos] == '\n') {
                    // Haven't seen non-space character
                    lineseekoff = curseekoff;
                    linepos = ++curpos;
                } else if (!isspace(buf[curpos])) {
                    if (curseekoff != lineseekoff) {
#ifdef SYS
                        lseek(fdin, lineseekoff, SEEK_SET);
#else
                        fseek(ifile, lineseekoff, SEEK_SET);
#endif
                        curseekoff = lineseekoff;
                        writing = true;
                        postseek = true;
                        goto read;
                    } else {
                        curpos = linepos;
                        writing = true;
                        continue;
                    }
                } else {
                    curpos++;
                }
            }
        }
#ifdef SYS
        curseekoff += nbuf; // no ftell() equivalent, needs tracking bytes manually
#endif
    } while (nbuf > 0);

    return 0;
}
