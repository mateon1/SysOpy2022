// Mateusz Naściszewski, 2022
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#ifdef SYS
#include <fcntl.h>
#include <unistd.h>
#define PROGNAME "main-sys"
#else
#define PROGNAME "main-lib"
#endif

static int interpret_char(char* str) {
#define RETC(c) return (str[1] != '\0') ? -1 : (int)(unsigned)(c)
    switch (*str) {
        case '\0': return -1;
        case '\\':
            switch (*++str) {
                case '0': case '1': case '2': case '3':
                    if (!str[1]) { // \0
                        return 0;
                    } else {
                        // octal \ooo
                        uint8_t val = 0;
                        for (int i = 0; *str && i < 3; i++) {
                            if (*str < '0' || *str > '7') return -1;
                            val <<= 3;
                            val |= (*str++ - '0');
                        }
                        --str;
                        RETC(val);
                    }
                case 'r': RETC('\r');
                case 'n': RETC('\n');
                case 't': RETC('\t');
                case 'b': RETC('\b');
                case 'v': RETC('\v');
                case 'a': RETC('\a');
                case 'f': RETC('\f');
                case '\\': RETC('\\');
                default: return -1;
            }
        default:
            RETC(*str);
    }
#undef RETC
}

int main(int argc, char** argv) {
    int chr;
    if (argc != 3 || (chr = interpret_char(argv[1])) == -1) {
        printf("Usage: %s <character> <file>\n", argc > 0 ? argv[0] : PROGNAME);
        exit(2);
    }
    assert(chr >= 0 && chr < 256);

#ifdef SYS
    int fd = open(argv[2], O_RDONLY);
    if (fd == -1) {
        perror("Failed to open file");
        exit(1);
    }
#else
    FILE *file = fopen(argv[2], "rb");
    if (file == NULL) {
        perror("Failed to open file");
        exit(1);
    }
#endif

    ssize_t nbuf = 0;
    char buf[1024];

    unsigned matchlines = 0, matchbytes = 0;
    bool freshline = true;

    do {
#ifdef SYS
        nbuf = read(fd, buf, sizeof(buf));
        if (nbuf == -1) {
            perror("Failed to read from file");
            exit(1);
        }
#else
        nbuf = (ssize_t) fread(buf, 1, sizeof(buf), file);
        if (nbuf == 0 && ferror(file) != 0) {
            perror("Failed to read from file");
            exit(1);
        }
#endif
        for (int i = 0; i < nbuf; i++) {
            if (buf[i] == (char)chr) {
                matchbytes++;
                if (freshline) matchlines++;
                freshline = false;
            }
            if (buf[i] == '\n')
                freshline = true;
        }
    } while (nbuf > 0);

    printf("%s\t%d\t%d\n", argv[2], matchlines, matchbytes);

    return 0;
}
