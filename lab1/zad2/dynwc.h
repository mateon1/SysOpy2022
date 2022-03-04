// Mateusz Naściszewski, 2022

#pragma once

#include <dlfcn.h> // dlopen, dlsym

#include <stdbool.h> // bool
#include <stdint.h> // (u)intX_t

typedef void* libwc_context;

libwc_context (*libwc_create)(void);
libwc_context (*libwc_create_custom)(char* tmpfile);
void (*libwc_destroy)(libwc_context);
void (*libwc_stats_to_tmpfile)(libwc_context, char* filepaths);
int32_t (*libwc_load_result)(libwc_context);
bool (*libwc_del_result)(libwc_context, int32_t handle);

static void* dynwc_handle;

void dyn_init() {
    dynwc_handle = dlopen("libwc.so", RTLD_LAZY);
    if (dynwc_handle == NULL) {
        fprintf(stderr, "Failed to dynamically load libwc\n");
        exit(1);
    }

#define SYM(name) do { \
    name = dlsym(dynwc_handle, #name); \
    if (name == NULL) { \
        fprintf(stderr, "Failed to dynamically load symbol '%s'\n", #name);\
        char *error = dlerror();\
        if (error != NULL) fprintf(stderr, "%s\n", error); \
        exit(1);\
    } \
} while (0)
    SYM(libwc_create);
    SYM(libwc_create_custom);
    SYM(libwc_destroy);
    SYM(libwc_stats_to_tmpfile);
    SYM(libwc_load_result);
    SYM(libwc_del_result);
#undef SYM
}
