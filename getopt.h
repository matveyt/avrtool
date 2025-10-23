/*
 * getopt - getopt(3), getopt_long(3), getopt_long_only(3) and getsubopt(3) impl.
 *
 * Aimed to be fully portable, complete, compatible (GNU/POSIX) and bug-free.
 * Written from scratch and released into the public domain.
 *
 * Last Change:  2025 Oct 21
 * License:      https://unlicense.org
 * URL:          https://github.com/matveyt/getopt
 */

#if !defined(GETOPT_H)
#define GETOPT_H

#if defined(__cplusplus)
extern "C" {
#endif

// GETOPT_PREFIX for globals
#if defined(GETOPT_PREFIX)
#define _getopt_concat_nx(x, y) x ## y
#define _getopt_concat(x, y)    _getopt_concat_nx(x, y)
#define _getopt(x)              _getopt_concat(GETOPT_PREFIX, x)
#else
#define _getopt(x)              x
#endif

enum {
    _getopt(no_argument),
    _getopt(required_argument),
    _getopt(optional_argument),
};

struct _getopt(option) {
    const char* name;
    int has_arg;
    int* flag;
    int val;
};

extern char* _getopt(optarg);
extern int _getopt(opterr), _getopt(optind), _getopt(optopt), _getopt(optreset);

int _getopt(getopt)(int argc, char* const argv[], const char* optstring);
int _getopt(getopt_long)(int argc, char* const argv[], const char* optstring,
    const struct _getopt(option)* longopts, int* longindex);
int _getopt(getopt_long_only)(int argc, char* const argv[], const char* optstring,
    const struct _getopt(option)* longopts, int* longindex);
int _getopt(getsubopt)(char** optionp, const char* const* tokens, char** valuep);

#undef _getopt
#undef _getopt_concat
#undef _getopt_concat_nx

#if defined(__cplusplus)
}
#endif

#endif // GETOPT_H
