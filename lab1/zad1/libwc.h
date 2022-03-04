// Mateusz Naściszewski, 2022

#pragma once

#include <stdbool.h> // bool
#include <stdint.h> // (u)intX_t

#ifndef _LIBWC_NO_OPAQUE_TYPES
typedef void* libwc_context;
#endif

// --- Context management ---

// Creates an empty libwc context with default settings
libwc_context libwc_create(void);
// Creates an empty libwc context with a custom temporary filepath
// Lifetime note: the path string must not be freed before calling libwc_destroy
libwc_context libwc_create_custom(char* tmpfile);

// Destroys the libwc context and frees all allocated data.
// Removes the temporary file, if it exists.
void libwc_destroy(libwc_context);

// --- Functionality ---

// Performs a `wc` count for the specified files, separated by spaces, and saves the result into a temporary file.
void libwc_stats_to_tmpfile(libwc_context, char* filepaths);

// Loads result from the temporary file.
// If successful, returns a non-negative integer handle to an internal results table.
// If unsuccessful, returns -1.
int32_t libwc_load_result(libwc_context);

// Delete result matching a given handle.
// Returns true if deleted successfully, false otherwise (e.g. invalid index, already deleted, etc.)
// Safety: Always safe to call, even with invalid or already freed indexes.
bool libwc_del_result(libwc_context, int32_t handle);


// Yes, the spec does not require providing any functionality for actually reading the managed data.
// But here it is anyway, mostly for debugging.

// Returns a reference to a managed result.
// Lifetime note: The resulting pointer is only valid as long as the context remains unmodified, calling libwc_load_result or libwc_del_result (or libwc_destroy) may invalidate the pointer.
char* libwc_get_result(libwc_context, int32_t handle);
