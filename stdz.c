#include "stdz.h"
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__unix__)
#include <sys/select.h>
#endif

static const char* _z_progname = "stdz";

// getprogname(3) impl.
const char* z_getprogname(void)
{
    return _z_progname;
}

// setprogname(3) impl.
void z_setprogname(const char* progname)
{
    _z_progname = z_basename(progname);
}

static int _z_is_pathsep(int c)
{
#if defined(_WIN32) || defined(__CYGWIN__)
        return c == '/' || c == '\\';
#else
        return c == '/';
#endif
}

// basename(3) GNU impl.
char* z_basename(const char* path)
{
    size_t i = strlen(path);
    while (!_z_is_pathsep(path[i - 1]))
        if (--i == 0)
            break;
    return (char*)&path[i];
}

// basename(3) XPG impl.
char* z_xpg_basename(char* path)
{
    if (path == NULL || *path == 0)
        return (char*)".";

    char* ptr = path + strlen(path) - 1;
    for (; _z_is_pathsep(*ptr); *ptr-- = 0)
        if (ptr == path)
            return (char*)"/";
    for (; !_z_is_pathsep(*ptr); ptr--)
        if (ptr == path)
            return ptr;

    return ptr + 1;
}

// dirname(3) impl.
char* z_dirname(char* path)
{
    if (path == NULL || *path == 0)
        return (char*)".";

    char* ptr = path + strlen(path) - 1;
    for (; _z_is_pathsep(*ptr); ptr--)
        if (ptr == path)
            return (char*)"/";
    for (; !_z_is_pathsep(*ptr); ptr--)
        if (ptr == path)
            return (char*)".";
    for (; _z_is_pathsep(*ptr); ptr--)
        if (ptr == path)
            return (char*)"/";

    ptr[1] = 0;
    return path;
}

// asprintf(3) impl.
int z_asprintf(char** strp, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int n = z_vasprintf(strp, fmt, args);
    va_end(args);
    return n;
}

// vasprintf(3) impl.
int z_vasprintf(char** strp, const char* fmt, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);
    int n = vsnprintf(NULL, 0, fmt, args_copy);
    if (n < 0)
        z_error(EXIT_FAILURE, errno, "asprintf(%s)", fmt);
    va_end(args_copy);

    return vsnprintf(*strp = (char*)z_malloc(n + 1), n + 1, fmt, args);
}

// getdelim(3) impl.
ssize_t z_getdelim(char** linep, size_t* n, int delimiter, FILE* stream)
{
    if (linep == NULL || n == NULL)
        z_error(EXIT_FAILURE, EINVAL, "getdelim(%p, %p)", linep, n);

    size_t sz = (linep != NULL) ? *n : 0, offset = 0;
    for (;;) {
        int c = fgetc(stream);
        if (c == EOF)
            break;
        if (offset + 1 >= sz) {
            if (sz >= INT_MAX)
                z_error(EXIT_FAILURE, EOVERFLOW, "getdelim(%zu)", sz);
            *linep = (char*)z_realloc(*linep, sz += sz/2 + 16);
        }
        (*linep)[offset++] = (char)c;
        if (c == delimiter)
            break;
    }
    *n = sz;

    if (offset == 0 || ferror(stream))
        return -1;
    (*linep)[offset] = 0;
    return offset;
}

// getline(3) impl.
ssize_t z_getline(char** linep, size_t* n, FILE* stream)
{
    return z_getdelim(linep, n, '\n', stream);
}

// fopen(3) with error checking
FILE* z_fopen(const char* fname, const char* mode)
{
    if (fname == NULL || (fname[0] == '-' && fname[1] == 0)) {
        while (*mode == ' ')
            ++mode;
        if (*mode == 'r')
            return stdin;
        if (*mode == 'w' || *mode == 'a')
            return stdout;
        z_error(EXIT_FAILURE, EINVAL, "fopen(%s, %s)", "-", mode);
    }

    FILE* f = fopen(fname, mode);
    if (f == NULL)
        z_error(EXIT_FAILURE, errno, "fopen(%s, %s)", fname, mode);
    return f;
}

// malloc(3) with error checking
void* z_malloc(size_t n)
{
    return z_realloc(NULL, n);
}

// realloc(3) with error checking
void* z_realloc(void* ptr, size_t n)
{
    if (n == 0) {
        free(ptr);
        return NULL;
    }
    ptr = realloc(ptr, n);
    if (ptr == NULL)
        z_error(EXIT_FAILURE, errno, "realloc(%zu)", n);
    return ptr;
}

// strcasecmp(3) impl.
int z_strcasecmp(const char* str1, const char* str2)
{
    return z_strncasecmp(str1, str2, INT_MAX);
}

// strncasecmp(3) impl.
int z_strncasecmp(const char* str1, const char* str2, size_t n)
{
    const uint8_t* ptr1 = (const uint8_t*)str1;
    const uint8_t* ptr2 = (const uint8_t*)str2;
    if (ptr1 == ptr2)
        return 0;

    for (; n > 0; --n) {
        int c1 = tolower(*ptr1++), c2 = tolower(*ptr2++);
        if (c1 == 0 || c1 != c2)
            return c1 - c2;
    }
    return 0;
}

// strchrnul(3) impl.
char* z_strchrnul(const char* str, int c)
{
    for (; *str != 0 && *(uint8_t*)str != c; ++str) ;
    return (char*)str;
}

// strdup(3) impl.
char* z_strdup(const char* str)
{
    return z_strndup(str, INT_MAX);
}

// strndup(3) impl.
char* z_strndup(const char* str, size_t n)
{
    const char* end = (const char*)memchr(str, 0, n);
    if (end != NULL)
        n = end - str;
    char* str_copy = (char*)memcpy(z_malloc(n + 1), str, n);
    str_copy[n] = 0;
    return str_copy;
}

// memccpy(3) impl.
void* z_memccpy(void* dst, const void* src, int c, size_t n)
{
    uint8_t* ptr1 = (uint8_t*)dst;
    const uint8_t* ptr2 = (const uint8_t*)src;

    for (; n > 0; --n)
        if ((*ptr1++ = *ptr2++) == c)
            return ptr1;

    return NULL;
}

// strscpy(9) impl.
ssize_t z_strscpy(char* dst, const char* src, size_t n)
{
    if ((ssize_t)n <= 0)
        return -E2BIG;

    char* ptr = (char*)z_memccpy(dst, src, 0, n);
    return (ptr != NULL) ? (ptr - 1 - dst) : (dst[n - 1] = 0, -E2BIG);
}

// stpecpy(7) impl.
char* z_stpecpy(char* dst, char* end, const char* src)
{
    if (dst == NULL)
        return NULL;

    char* ptr = (char*)z_memccpy(dst, src, 0, end - dst);
    return (ptr != NULL) ? (ptr - 1) : (end[-1] = 0, (char*)NULL);
}

// strerror_r(3) impl.
int z_strerror_r(int errnum, char* buf, size_t n)
{
    if (errnum > 0) {
        z_strscpy(buf, strerror(errnum), n);
        return 0;
    }

#if defined(_WIN32)
    DWORD dwError;
    if (errnum == 0 && (dwError = GetLastError()) != ERROR_SUCCESS)
        return FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, dwError, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), buf, (DWORD)n,
            NULL) ? 0 : ERANGE;
#endif

    return EINVAL;
}

// delay(milliseconds)
void z_delay(uint32_t ms)
{
#if defined(_WIN32)
    Sleep(ms);
#elif defined(__unix__)
    select(0, NULL, NULL, NULL, &(struct timeval){ .tv_sec = ms / 1000,
        .tv_usec = (ms % 1000) * 1000 });
#endif
}

// error(3) impl.
void z_error(int status, int errnum, const char* fmt, ...)
{
    fflush(stdout);

    va_list args;
    va_start(args, fmt);
    z__vwarnx(fmt, args);
    va_end(args);

    char buf[128];
    if (z_strerror_r(errnum, buf, sizeof(buf)) == 0)
        fprintf(stderr, ": %s", buf);
    fputc('\n', stderr);

    if (status != 0)
        exit(status);
}

// warnx(3) impl.
void z_warnx(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    z__vwarnx(fmt, args);
    fputc('\n', stderr);
    va_end(args);
}

// like warnx(3) but no newline
void z__warnx(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    z__vwarnx(fmt, args);
    va_end(args);
}

// like vwarnx(3) but no newline
void z__vwarnx(const char* fmt, va_list args)
{
    fprintf(stderr, "%s: ", z_getprogname());
    if (fmt != NULL)
        vfprintf(stderr, fmt, args);
}

// getopt(3) impl.
#define GETOPT_PREFIX       z_
#define GETOPT_PRINTN(...)  z__warnx(__VA_ARGS__)
#define GETOPT_PRINTLN(...) z_warnx(__VA_ARGS__)
#if !defined(__has_include)
// hope for the best
#include "getopt.c"
#elif __has_include("getopt.c")
#include "getopt.c"
#endif
