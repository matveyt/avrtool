#include "stdz.h"
#include <errno.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

/*noreturn*/
void z_die(const char* where)
{
    char buf[100];
    fprintf(stderr, "%s: %s\n", where, z_strerror_r(errno, buf, sizeof(buf)));
    exit(EXIT_FAILURE);
}

FILE* z_fopen(const char* fname, const char* mode)
{
    FILE* f = fopen(fname, mode);
    if (f == NULL)
        z_die("fopen");
    return f;
}

void* z_malloc(size_t size)
{
    void* ptr = malloc(size);
    if (ptr == NULL)
        z_die("malloc");
    return ptr;
}

void* z_realloc(void* ptr, size_t size)
{
    if (size == 0)
        return free(ptr), NULL;
    ptr = realloc(ptr, size);
    if (ptr == NULL)
        z_die("realloc");
    return ptr;
}

char* z_strdup(const char* str)
{
    return strcpy(z_malloc(strlen(str) + 1), str);
}

char* z_strerror_r(int errnum, char* buf, size_t n)
{
#if defined(_WIN32)
    DWORD dwErrorCode;
#endif
    if (n == 0)
        return buf;

    if (errnum != 0)
        strncpy(buf, strerror(errnum), n - 1);
    else
#if defined(_WIN32)
    if ((dwErrorCode = GetLastError()) != ERROR_SUCCESS)
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
            dwErrorCode, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), buf, n, NULL);
    else
#endif
        strncpy(buf, "Unspecified error", n - 1);

    buf[n - 1] = '\0';
    return buf;
}
