#if !defined(STDZ_H)
#define STDZ_H

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#if defined(__cplusplus)
extern "C" {
#endif

#undef min
#undef max
#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))

const char* z_getprogname(void);
void z_setprogname(const char* progname);
char* z_basename(const char* path); // GNU
char* z_xpg_basename(char* path);   // XPG
char* z_dirname(char* path);
int z_asprintf(char** strp, const char* fmt, ...);
int z_vasprintf(char** strp, const char* fmt, va_list args);
ssize_t z_getdelim(char** linep, size_t* n, int delimiter, FILE* stream);
ssize_t z_getline(char** linep, size_t* n, FILE* stream);
FILE* z_fopen(const char* fname, const char* mode);
void* z_malloc(size_t n);
void* z_realloc(void* ptr, size_t n);
int z_strcasecmp(const char* str1, const char* str2);
int z_strncasecmp(const char* str1, const char* str2, size_t n);
char* z_strchrnul(const char* str, int c);
char* z_strdup(const char* str);
char* z_strndup(const char* str, size_t n);
void* z_memccpy(void* dst, const void* src, int c, size_t n);
ssize_t z_strscpy(char* dst, const char* src, size_t n);
char* z_stpecpy(char* dst, char* end, const char* src);
int z_strerror_r(int errnum, char* buf, size_t n);
void z_delay(uint32_t ms);
void z_error(int status, int errnum, const char* fmt, ...);
void z_warnx(const char* fmt, ...);
void z__warnx(const char* fmt, ...);
void z__vwarnx(const char* fmt, va_list args);

// getopt(3) impl.
#define GETOPT_PREFIX z_
#if !defined(__has_include)
// hope for the best
#include "getopt.h"
#elif __has_include("getopt.h")
#include "getopt.h"
#endif

#if defined(__cplusplus)
}
#endif

#endif // STDZ_H
