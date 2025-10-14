#include "stdz.h"
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static const char* z_progname;

const char* z_getprogname(void)
{
    return (z_progname != NULL) ? z_progname : "stdz";
}

void z_setprogname(const char* progname)
{
    if (progname != NULL) {
        for (size_t i = strlen(progname); i > 0; --i) {
            if (progname[i - 1] == '/'
#if defined(_WIN32)
                || progname[i - 1] == '\\'
#endif
               ) {
                z_progname = progname + i;
                return;
            }
        }
    }
    z_progname = progname;
}

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

void* z_malloc(size_t size)
{
    void* ptr = malloc(size);
    if (ptr == NULL)
        z_error(EXIT_FAILURE, errno, "malloc(%zu)", size);
    return ptr;
}

void* z_realloc(void* ptr, size_t size)
{
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    ptr = realloc(ptr, size);
    if (ptr == NULL)
        z_error(EXIT_FAILURE, errno, "realloc(%zu)", size);
    return ptr;
}

char* z_strdup(const char* str)
{
    return strcpy((char*)z_malloc(strlen(str) + 1), str);
}

// error(3) impl.
void z_error(int status, int errnum, const char* fmt, ...)
{
    fflush(stdout);

    va_list args;
    va_start(args, fmt);
    z_vwarnx(0, fmt, args);
    va_end(args);

    char buf[128];
    if (z_strerror_r(errnum, buf, sizeof(buf)) == 0)
        fprintf(stderr, ": %s", buf);

    putc('\n', stderr);

    if (status != 0)
        exit(status);
}

// strerror_r(3) impl.
int z_strerror_r(int errnum, char* buf, size_t n)
{
    if (errnum > 0) {
        strncpy(buf, strerror(errnum), n - 1);
        buf[n - 1] = 0;
        return 0;
    }
#if defined(_WIN32)
    DWORD dwError;
    if (errnum == 0 && (dwError = GetLastError()) != ERROR_SUCCESS)
        return FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, dwError, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), buf, n, NULL) ?
            0 : ERANGE;
#endif
    return EINVAL;
}

// like vwarnx(3) but with 'nl'
void z_vwarnx(int nl, const char* fmt, va_list args)
{
    fprintf(stderr, "%s: ", z_getprogname());
    if (fmt != NULL)
        vfprintf(stderr, fmt, args);
    if (nl)
        putc('\n', stderr);
}

// like warnx(3) but with 'nl'
void z_warnx(int nl, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    z_vwarnx(nl, fmt, args);
    va_end(args);
}

// getopt(3) impl.
#define GETOPT_PREFIX       z_
#define GETOPT_PRINTN(...)  z_warnx(0, __VA_ARGS__)
#define GETOPT_PRINTLN(...) z_warnx(1, __VA_ARGS__)
#if !defined(__has_include)
// hope for the best
#include "getopt.c"
#elif __has_include("getopt.c")
#include "getopt.c"
#endif
