#include "stdz.h"
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

void z_error(int status, int errnum, const char* fmt, ...)
{
    fflush(stdout);
    fprintf(stderr, "%s: ", z_progname);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    if (errnum >= 0) {
        char buf[100];
        z_strerror_r(errnum, buf, sizeof(buf));
        fprintf(stderr, ": %s", buf);
    }
    putc('\n', stderr);

    if (status)
        exit(status);
}

int z_strerror_r(int errnum, char* buf, size_t n)
{
#if defined(_WIN32)
    DWORD dwError;
    if (errnum == 0 && (dwError = GetLastError()) != ERROR_SUCCESS) {
        return FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, dwError, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), buf, n, NULL) ?
            0 : ERANGE;
    }
#endif
    strncpy(buf, strerror(errnum), n - 1);
    buf[n - 1] = 0;
    return 0;
}

FILE* z_fopen(const char* fname, const char* mode)
{
    FILE* f = fopen(fname, mode);
    if (f == NULL)
        z_error(EXIT_FAILURE, errno, "fopen(\"%s\")", fname);
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
