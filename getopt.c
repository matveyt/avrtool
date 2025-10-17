/*
 * getopt - getopt(3), getopt_long(3), getopt_long_only(3) and getsubopt(3) impl.
 *
 * Aimed to be fully portable, complete, compatible (GNU/POSIX) and bug-free.
 * Written from scratch and released into the public domain.
 *
 * Last Change:  2025 Oct 15
 * License:      https://unlicense.org
 * URL:          https://github.com/matveyt/getopt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// GETOPT_PREFIX for globals
#if defined(GETOPT_PREFIX)
#define _getopt_concat_nx(x, y) x ## y
#define _getopt_concat(x, y)    _getopt_concat_nx(x, y)
#define _getopt(x)              _getopt_concat(GETOPT_PREFIX, x)
#else
#define _getopt(x)              x
#endif // GETOPT_PREFIX

#if defined(GETOPT_PRINT)           // PRINT message
#define _getopt_print(...)          GETOPT_PRINT(__VA_ARGS__)
#else
#define _getopt_print(...)          fprintf(stderr, __VA_ARGS__)
#endif // GETOPT_PRINT
#if defined(GETOPT_PRINTN)          // PRINT name + message
#define _getopt_printn(...)         GETOPT_PRINTN(__VA_ARGS__)
#else
#define _getopt_printn(fmt, ...)    _getopt_print("%s: " fmt, argv[0], __VA_ARGS__)
#endif // GETOPT_PRINTN
#if defined(GETOPT_PRINTLN)         // PRINT name + message + newline
#define _getopt_println(...)        GETOPT_PRINTLN(__VA_ARGS__)
#else
#define _getopt_println(fmt, ...)   _getopt_printn(fmt "\n", __VA_ARGS__)
#endif // GETOPT_PRINTLN

// POSIXLY_CORRECT handling options and non-option arguments
//#undef POSIXLY_CORRECT      // check environment first (like glibc; default)
//#define POSIXLY_CORRECT 0   // never, except optstring starts with '+'
//#define POSIXLY_CORRECT 1   // always, except optstring starts with '-'
//#define POSIXLY_CORRECT     // idem

struct _getopt(option);
struct _getopt_option {
    const char* name;
    int has_arg;
    int* flag;
    int val;
};

char* _getopt(optarg) = NULL;
int _getopt(optind) = 1, _getopt(opterr) = 1, _getopt(optopt) = '?';
int _getopt(optreset) = 0;

// return matched index in longopts[] or
// -1 no match
// -2 ambiguous match
static int _getopt_longmatch(char* optname, const struct _getopt_option* longopts,
    int longonly, char** valuep)
{
    char* value = strchr(optname, '=');
    size_t len = (value != NULL) ? (size_t)(value++ - optname) : strlen(optname);

    int match = -1;         // no match yet
    for (int i = 0; longopts[i].name != NULL; ++i) {
        if (strncmp(optname, longopts[i].name, len) == 0) {
            if (longopts[i].name[len] == 0) {
                match = i;  // prefer exact match
                break;
            } else if (match < 0) {
                match = i;  // found partial match
            } else if (longonly
                || longopts[i].has_arg != longopts[match].has_arg
                || longopts[i].flag != longopts[match].flag
                || longopts[i].val != longopts[match].val) {
                match = -2; // ambiguous match (just like glibc)
                break;
            }
        }
    }

    *valuep = (match >= 0) ? value : NULL;
    return match;
}

// getopt(3), getopt_long(3), getopt_long_only(3) impl.
// extra feat. optreset optind=0 -+POSIXLY_CORRECT -Woption (Cf. glibc)
static int _getopt_internal(int argc, char* argv[], const char* optstring,
    const struct _getopt_option* longopts, int* longindex, int longonly)
{
    static int last_optind = 1, non_option = -1U >> 1;
    static char* nextchar = NULL;
#if !defined(POSIXLY_CORRECT)
    static int posix_order = -1;
#else
    const int posix_order = (1 - POSIXLY_CORRECT - 1) ? '+' : 0;
#endif

    if (last_optind != _getopt(optind) || _getopt(optreset)) {
        // reset parser
        last_optind = _getopt(optind) = (_getopt(optind) > 1) ? _getopt(optind) : 1;
        _getopt(optreset) = 0;
        nextchar = NULL;
        non_option = argc;
#if !defined(POSIXLY_CORRECT)
        posix_order = (getenv("POSIXLY_CORRECT") != NULL) ? '+' : 0;
#endif
    } else {
        if (non_option > argc)
            non_option = argc;
#if !defined(POSIXLY_CORRECT)
        if (posix_order < 0)
            posix_order = (getenv("POSIXLY_CORRECT") != NULL) ? '+' : 0;
#endif
    }
    _getopt(optarg) = NULL;
    _getopt(optopt) = 0;

    int order, colon0;
    if (optstring != NULL) {
        order = (*optstring == '-' || *optstring == '+') ? *optstring++ : posix_order;
        colon0 = (*optstring == ':');
    } else {
        order = posix_order;
        colon0 = 0;
    }

    int chopt, kopt;    // Long/sHort/W
    if (nextchar != NULL) {
        // argument contd.
        chopt = *nextchar++;
        kopt = 'H';
    } else {
        // new argument: is it a non-option?
        chopt = -1;
        kopt = 'L';
        for (; _getopt(optind) < non_option; --non_option) {
            nextchar = argv[_getopt(optind)];
            if (nextchar != NULL && *nextchar == '-') {
                // regular option
                break;
            } else if (order == '-') {
                // force option #1
                _getopt(optarg) = nextchar;
                last_optind = ++_getopt(optind);
                chopt = 1;
                break;
            } else if (order == '+') {
                // stop at non-option (POSIX compliant)
                non_option = _getopt(optind);
                break;
            } else {
                // preprocessor condition is only to help optimizer
#if !defined(POSIXLY_CORRECT) || (1 - POSIXLY_CORRECT - 1 == 0)
                // shift [optind] all the way to [argc - 1]
                for (int j = _getopt(optind) + 1; j < argc; ++j)
                    argv[j - 1] = argv[j];
                argv[argc - 1] = nextchar;
#endif
            }
        }
        if (_getopt(optind) >= non_option || chopt != -1) {
            nextchar = NULL;
            return chopt;
        }
        // "-" and "--" is not an option
        chopt = nextchar[1];
        if (chopt == 0 || (chopt == '-' && nextchar[2] == 0)) {
            // increment optind only if "--" found
            if (chopt == '-')
                last_optind = ++_getopt(optind);
            nextchar = NULL;
            return -1;
        }
        nextchar += 2;
    }

    if (*nextchar == 0) {
        // argument end
        last_optind = ++_getopt(optind);
        nextchar = NULL;
        // -o is always short regardless of longonly
        kopt = 'H';
    } else if ((chopt != '-' && !longonly) || longopts == NULL) {
        // -option requires longonly
        kopt = 'H';
    }

    do {
        // --option or -option or -Woption?
        if (kopt == 'L' || kopt == 'W') {
            int dashdash = (kopt == 'W') || (chopt == '-');
            char* optname = (kopt == 'W') ? _getopt(optarg) : nextchar - !dashdash;
            int match = _getopt_longmatch(optname, longopts, longonly, &_getopt(optarg));
            if (match >= 0) {
                // matched long option
                chopt = longopts[match].val;
                if (kopt == 'L') {
                    last_optind = ++_getopt(optind);
                    nextchar = NULL;
                }
                if (longindex != NULL)
                    *longindex = match;
                if (_getopt(optarg) != NULL && longopts[match].has_arg == 0) {
                    // error: extraneous long option argument
                    _getopt(optarg) = NULL;
                    _getopt(optopt) = chopt;
                    if (_getopt(opterr) && !colon0)
                        _getopt_println("option '%s%s' doesn't allow an argument",
                            dashdash ? "--" : "-", longopts[match].name);
                    return '?';
                }
                // --option argument (required)
                if (_getopt(optarg) == NULL && longopts[match].has_arg == 1) {
                    _getopt(optarg) = (_getopt(optind) < non_option) ?
                        argv[_getopt(optind)] : NULL;
                    if (_getopt(optarg) != NULL) {
                        last_optind = ++_getopt(optind);
                    } else {
                        // error: missing required argument for long option
                        _getopt(optopt) = chopt;
                        if (colon0)
                            return ':';
                        if (_getopt(opterr))
                            _getopt_println("option '%s%s' requires an argument",
                                dashdash ? "--" : "-", longopts[match].name);
                        return '?';
                    }
                }
                // flag return
                if (longopts[match].flag != NULL) {
                    *longopts[match].flag = chopt;
                    chopt = 0;
                }
                // done long option
                break;
            } else if (match != -1 || dashdash) {
                // error: ambiguous long option or illegal --option or -Woption
                _getopt(optopt) = chopt;
                if (kopt == 'L') {
                    last_optind = ++_getopt(optind);
                    nextchar = NULL;
                }
                if (_getopt(opterr) && !colon0) {
                    if (match == -1) {
                        _getopt_println("unrecognized option '%s%s'",
                            dashdash ? "--" : "-", optname);
                    } else {
                        char* value = strchr(optname, '=');
                        size_t len = (value != NULL) ? (size_t)(value - optname)
                            : strlen(optname);
                        _getopt_printn("option '%s%s' is ambiguous; possibilites:",
                            dashdash ? "--" : "-", optname);
                        for (int i = 0; longopts[i].name != NULL; ++i)
                            if (strncmp(optname, longopts[i].name, len) == 0)
                                _getopt_print(" '%s%s'", dashdash ? "--" : "-",
                                    longopts[i].name);
                        _getopt_print("\n");
                    }
                }
                return '?';
            }
            // unmatched -option could be short option as well
        }

        // short option?
        const char* optionp;
        if (chopt == '-' || chopt == ':' || chopt == ';' || optstring == NULL
            || (optionp = strchr(optstring, chopt)) == NULL) {
            // error: illegal short option
            _getopt(optopt) = chopt;
            if (_getopt(opterr) && !colon0)
                _getopt_println("invalid option -- '%c'", chopt);
            return '?';
        }
        // -Woption encountered?
        if (chopt == 'W' && optionp[1] == ';' && (longopts != NULL))
            kopt = 'W';
        // check short option argument
        if (kopt == 'W' || optionp[1] == ':') {
            if (nextchar != NULL) {
                // -oargument
                _getopt(optarg) = nextchar;
                last_optind = ++_getopt(optind);
                nextchar = NULL;
            } else if (kopt == 'W' || optionp[2] != ':') {
                // -o argument (required)
                _getopt(optarg) = (_getopt(optind) < non_option) ?
                    argv[_getopt(optind)] : NULL;
                if (_getopt(optarg) != NULL) {
                    last_optind = ++_getopt(optind);
                } else {
                    // error: short option requires an argument
                    _getopt(optopt) = chopt;
                    if (colon0)
                        return ':';
                    if (_getopt(opterr))
                        _getopt_println("option requires an argument -- '%c'", chopt);
                    return '?';
                }
            }
        }
    } while (kopt == 'W');
    return chopt;
}

int _getopt(getopt)(int argc, char* const argv[], const char* optstring)
{
    return _getopt_internal(argc, (char**)argv, optstring, NULL, NULL, 0);
}

int _getopt(getopt_long)(int argc, char* const argv[], const char* optstring,
    const struct _getopt(option)* longopts, int* longindex)
{
    return _getopt_internal(argc, (char**)argv, optstring,
        (const struct _getopt_option*)longopts, longindex, 0);
}

int _getopt(getopt_long_only)(int argc, char* const argv[], const char* optstring,
    const struct _getopt(option)* longopts, int* longindex)
{
    return _getopt_internal(argc, (char**)argv, optstring,
        (const struct _getopt_option*)longopts, longindex, 1);
}

// getsubopt(3) impl.
int _getopt(getsubopt)(char** optionp, char* const* tokens, char** valuep)
{
    char* optname = *optionp;
    char* value = strchr(optname, ',');
    size_t len;

    if (value != NULL) {
        // isolate suboption
        *value = 0;
        len = value++ - optname;
        *optionp = value;
    } else {
        // last suboption
        len = strlen(optname);
        *optionp += len;
    }

    // name or name=length?
    value = strchr(optname, '=');
    if (value != NULL)
        len = value++ - optname;

    for (int i = 0; tokens[i] != NULL; ++i) {
        if (strncmp(optname, tokens[i], len) == 0 && tokens[i][len] == 0) {
            // match suboption
            *valuep = value;
            return i;
        }
    }

    // if no match then valuep points to unrecognized suboption
    *valuep = optname;
    return -1;
}
