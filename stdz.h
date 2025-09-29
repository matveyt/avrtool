#if !defined(STDZ_H)
#define STDZ_H

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__cplusplus)
extern "C" {
#endif

#undef min
#undef max
#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))

extern const char* z_progname;

void z_error(int status, int errnum, const char* fmt, ...);
int z_strerror_r(int errnum, char* buf, size_t n);
FILE* z_fopen(const char* fname, const char* mode);
void* z_malloc(size_t size);
void* z_realloc(void* ptr, size_t size);
char* z_strdup(const char* str);

#if defined(__cplusplus)
}
#endif

#endif // STDZ_H
