#include "stdz.h"
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__unix__)
#include <sys/select.h>
#endif

static const char* _z_progname = "stdz";

static int _z_is_pathsep(int c)
{
#if defined(_WIN32) || defined(__CYGWIN__)
        return c == '/' || c == '\\';
#else
        return c == '/';
#endif
}

// getprogname(3) impl.
const char* z_getprogname(void)
{
    return _z_progname;
}

// setprogname(3) impl.
void z_setprogname(const char* progname)
{
    size_t i = strlen(progname);
    while (!_z_is_pathsep(progname[i - 1]))
        if (--i == 0)
            break;
    _z_progname = &progname[i];
}

// basename(3) impl.
char* z_basename(char* path)
{
    if (path == NULL || *path == 0)
        return ".";

    char* p = path + strlen(path) - 1;
    for (; _z_is_pathsep(*p); *p-- = 0)
        if (p == path)
            return "/";
    for (; !_z_is_pathsep(*p); p--)
        if (p == path)
            return p;

    return p + 1;
}

// dirname(3) impl.
char* z_dirname(char* path)
{
    if (path == NULL || *path == 0)
        return ".";

    char* p = path + strlen(path) - 1;
    for (; _z_is_pathsep(*p); p--)
        if (p == path)
            return "/";
    for (; !_z_is_pathsep(*p); p--)
        if (p == path)
            return ".";
    for (; _z_is_pathsep(*p); p--)
        if (p == path)
            return "/";

    p[1] = 0;
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
    va_list copy;
    va_copy(copy, args);
    int n = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);

    if (n < 0)
        z_error(EXIT_FAILURE, errno, "asprintf(%s)", fmt);

    *strp = z_malloc(n + 1);
    vsnprintf(*strp, n + 1, fmt, args);
    return n;
}

// getdelim(3) impl.
ssize_t z_getdelim(char** lineptr, size_t* n, int delimiter, FILE* stream)
{
    if (lineptr == NULL || n == NULL)
        z_error(EXIT_FAILURE, EINVAL, "getdelim(%p, %p)", lineptr, n);
    if (*lineptr == NULL)
        *n = 0;

    size_t offset = 0;
    for (;;) {
        int c = getc(stream);
        if (c == EOF)
            break;
        if (offset + 1 >= *n)
            *lineptr = z_realloc(*lineptr, *n += max(*n, 128));
        (*lineptr)[offset++] = (char)c;
        if (c == delimiter)
            break;
    }
    if (offset == 0 || ferror(stream))
        return -1;

    (*lineptr)[offset] = 0;
    return offset;
}

// getline(3) impl.
ssize_t z_getline(char** lineptr, size_t* n, FILE* stream)
{
    return z_getdelim(lineptr, n, '\n', stream);
}

// fopen(3) safe
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

// malloc(3) safe
void* z_malloc(size_t size)
{
    void* ptr = malloc(size);
    if (ptr == NULL)
        z_error(EXIT_FAILURE, errno, "malloc(%zu)", size);
    return ptr;
}

// realloc(3) safe
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

// strcasecmp(3) impl.
int z_strcasecmp(const char* s1, const char* s2)
{
    const uint8_t* p1 = (const uint8_t*)s1;
    const uint8_t* p2 = (const uint8_t*)s2;

    if (p1 == p2)
        return 0;

    for (;;) {
        int c1 = *p1++, c2 = *p2++;
        if (!c1 || !c2 || tolower(c1) != tolower(c2))
            return c1 - c2;
    }
}

// strdup(3) impl.
char* z_strdup(const char* str)
{
    return strcpy((char*)z_malloc(strlen(str) + 1), str);
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
            NULL, dwError, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), buf, (DWORD)n,
            NULL) ? 0 : ERANGE;
#endif

    return EINVAL;
}

// delay(milliseconds)
void z_delay(unsigned long milliseconds)
{
#if defined(_WIN32)
    Sleep(milliseconds);
#elif defined(__unix__)
    select(0, NULL, NULL, NULL, &(struct timeval){ .tv_sec = milliseconds / 1000,
        .tv_usec = (milliseconds % 1000) * 1000 });
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

    putc('\n', stderr);

    if (status != 0)
        exit(status);
}

// warnx(3) impl.
void z_warnx(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    z__vwarnx(fmt, args);
    putc('\n', stderr);
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
void z__vwarnx(const char * fmt, va_list args)
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
