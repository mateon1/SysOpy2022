// Mateusz Naściszewski, 2022

#include <stdbool.h> // bool
#include <stdint.h> // (u)intX_t
#include <stddef.h> // size_t
#include <stdlib.h> // calloc, free
#include <string.h> // strlen
#include <assert.h> // assert
#include <unistd.h> // unlink
#include <stdio.h> // fopen, fseeko, ftello, fread
#include <sys/types.h> // off_t

typedef struct libwc_context {
    // Unowned pointer
    char* tmpfile;
    // len/cap/data akin to standard vector implementation
    size_t len;
    size_t cap;
    // Pointer and contents owned by this struct
    char** data;
} raw_context;

typedef raw_context* libwc_context;

#define _LIBWC_NO_OPAQUE_TYPES
#include "libwc.h"
#undef _LIBWC_NO_OPAQUE_TYPES


// --- Context management ---

static bool ensure_space(libwc_context ctx) {
    if (ctx->cap > ctx->len) return true;

    size_t new_size = 2 * ctx->cap;
    if (new_size == 0) new_size = 32 / sizeof(char*); // arbitrary, fill a cache line

    char** new_data = calloc(new_size, sizeof(char*));
    if (new_data == NULL) return false;

    memcpy(new_data, ctx->data, ctx->len * sizeof(char*));
    free(ctx->data);

    ctx->cap = new_size;
    ctx->data = new_data;
    return true;
}

libwc_context libwc_create(void) {
    libwc_context ctx = calloc(1, sizeof(raw_context));
    if (ctx == NULL) return NULL;
    ctx->tmpfile = "/tmp/libwc.txt";
    // Unnecessary: zeroed memory with calloc
    // ctx->len = ctx->cap = 0;
    // ctx->data = NULL;
    return ctx;
}
libwc_context libwc_create_custom(char* tmpfile) {
    assert(tmpfile != NULL);
    assert(strlen(tmpfile) > 0);
    assert(tmpfile[0] != ' ');

    libwc_context ctx = calloc(1, sizeof(raw_context));
    if (ctx == NULL) return NULL;
    ctx->tmpfile = tmpfile;
    // Unnecessary: zeroed memory with calloc
    // ctx->len = ctx->cap = 0;
    // ctx->data = NULL;
    return NULL;
}

void libwc_destroy(libwc_context ctx) {
    if (ctx->data != NULL) {
        for (size_t i = 0; i < ctx->len; i++) {
            char* p = ctx->data[i];
            if (p != NULL) free(p);
        }
    }
    unlink(ctx->tmpfile); // Failure is fine here, in most cases it's ENOENT
    free(ctx);
}

// --- Functionality ---

// Checks whether a character is safe in an argument position of a shell command
// SAFETY NOTE: Assuming ASCII or ASCII-compatible encoding
static bool is_arg_safe(char c) {
    if ('-' <= c && c <= ':') return true;
    if ('@' <= c && c <= 'Z') return true;
    if ('a' <= c && c <= 'z') return true;
    return c == '_' || c == '~' || c == '@' || c == '+';
}

void libwc_stats_to_tmpfile(libwc_context ctx, char* filepaths) {
    char *sysbuf = calloc(
        2 * (strlen(filepaths) + 1) + // "- -''' -" might be escaped to "./- ./-\'\'\' ./-"
        2 * strlen(ctx->tmpfile) + // Each character may be escaped with '\'
        6 // length of "wc >" + space + final NUL byte
        , 1);

    char *p = sysbuf;
    strcat(p, "wc >");
    p += 4;

    for (char *t = ctx->tmpfile; *t; t++) {
        if (!is_arg_safe(*t)) *p++ = '\\';
        *p++ = *t;
    }

    *p++ = ' ';

    bool start = true;
    for (char *a = filepaths; *a; a++) {
        if (start && *a == '-') {
            *p++ = '.';
            *p++ = '/';
        }
        start = false;
        if (!is_arg_safe(*a) && *a != ' ') *p++ = '\\';
        *p++ = *a;
        if (*a == ' ') start = true;
    }

    // "wc >TMPFILE file1 file\$\#\! ./-file- /path/to/file"
    // printf("System: %s\n", sysbuf);
    int err = system(sysbuf);
    assert(err == 0 && "system() call failed");

    free(sysbuf);
}
int32_t libwc_load_result(libwc_context ctx) {
    if (!ensure_space(ctx)) return -1;

    FILE *f = fopen(ctx->tmpfile, "rb");
    if (f == NULL) return -1;

    // Calculate file size
    if (fseeko(f, 0, SEEK_END) != 0) return -1;
    off_t size = ftello(f);
    if (size == -1) return -1;
    // Return to beginning
    if (fseeko(f, 0, SEEK_SET) != 0) return -1;

    size_t idx = ctx->len++;
    assert(ctx->data[idx] == NULL);
    ctx->data[idx] = calloc(1, (size_t)size + 1); // +1 for final NUL byte
    if (ctx->data[idx] == NULL) {
        ctx->len--;
        return -1;
    }

    size_t written = fread(ctx->data[idx], 1, size, f);

    assert(written == (size_t)size);

    int err = fclose(f);
    assert(err == 0 && "fclose() failed");

    return idx;
}

bool libwc_del_result(libwc_context ctx, int32_t handle) {
    if (handle < 0 || (size_t)handle >= ctx->len) return false;
    if (ctx->data[handle] == NULL) return false;

    free(ctx->data[handle]);
    ctx->data[handle] = NULL;

    return true;
}


// Returns a reference to a managed result.
// Lifetime note: The resulting pointer is only valid as long as the context remains unmodified, calling libwc_load_result or libwc_del_result (or libwc_destroy) may invalidate the pointer.
char* libwc_get_result(libwc_context ctx, int32_t handle) {
    assert(handle >= 0 && (size_t)handle < ctx->len);
    return ctx->data[handle];
}
