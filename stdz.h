#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#undef min
#undef max
#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))

/*noreturn*/ void z_die(const char* where);
FILE* z_fopen(const char* fname, const char* mode);
void* z_malloc(size_t size);
void* z_realloc(void* ptr, size_t size);
char* z_strdup(const char* str);
char* z_strerror_r(int errnum, char* buf, size_t n);
