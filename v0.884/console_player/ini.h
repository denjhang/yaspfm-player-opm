/* inih -- simple .INI file parser
inih is a simple .INI file parser written in C. It's only a couple of pages of
code, and it was designed to be small and simple, so it's good for embedded
systems. It's also cross-platform.
See https://github.com/benhoyt/inih for C++ and other bindings.
See https://github.com/benhoyt/inih/wiki/Examples for examples.
Browse the source code to see how to use it.

The "simple" version is designed for small systems, and may not support all
features. The "normal" version supports most features, including multi-line
values. The "full" version supports all features, including interpolation.

This file is a combined header and source file. To use it,
#define INI_IMPLEMENTATION in one C or C++ file before including this file.

*/

#ifndef __INI_H__
#define __INI_H__

/* Make this header file easier to include in C++ code */
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

/* Non-zero if ini_handler callback should accept null values */
#ifndef INI_ALLOW_NULL_VALUES
#define INI_ALLOW_NULL_VALUES 0
#endif

/* Non-zero if ini_handler callback should accept empty values */
#ifndef INI_ALLOW_EMPTY_VALUES
#define INI_ALLOW_EMPTY_VALUES 1
#endif

/* Non-zero if ini_parse() should support multi-line values */
#ifndef INI_ALLOW_MULTILINE
#define INI_ALLOW_MULTILINE 1
#endif

/* Non-zero if ini_parse() should support BOM (UTF-8 byte order mark) */
#ifndef INI_USE_BOM
#define INI_USE_BOM 1
#endif

/* Non-zero if ini_parse_file() should support file path */
#ifndef INI_HANDLER_LINENO
#define INI_HANDLER_LINENO 1
#endif

/* Non-zero if ini_parse_string() should support newlines */
#ifndef INI_ALLOW_INLINE_COMMENTS
#define INI_ALLOW_INLINE_COMMENTS 1
#endif

/* Non-zero if ini_handler callback should receive the line number */
#ifndef INI_HANDLER_LINENO
#define INI_HANDLER_LINENO 1
#endif

/* Non-zero to support quotes values */
#ifndef INI_SUPPORT_QUOTES
#define INI_SUPPORT_QUOTES 1
#endif

/* Typedef for prototype of handler function. */
#if INI_HANDLER_LINENO
typedef int (*ini_handler)(void* user, const char* section,
                           const char* name, const char* value,
                           int lineno);
#else
typedef int (*ini_handler)(void* user, const char* section,
                           const char* name, const char* value);
#endif

/* Parse given INI-style file. May have [section]s, name=value pairs
   (whitespace stripped), and comments starting with ';' (semicolon). Section
   is "" if name=value pair parsed before any section heading. name:value
   pairs are also supported as a concession to Python's configparser.

   For each name=value pair parsed, call handler function with given user
   pointer as the first argument, section name as the second argument,
   name as the third argument, and value as the fourth argument. If
   INI_HANDLER_LINENO is enabled, the fifth argument is the line number.

   Returns 0 on success, line number of first error on failure.
*/
int ini_parse(const char* filename, ini_handler handler, void* user);

/* Same as ini_parse(), but takes a FILE* instead of filename. This doesn't
   close the file when it's finished -- the caller must do that. */
int ini_parse_file(FILE* file, ini_handler handler, void* user);

/* Same as ini_parse(), but takes a string instead of a filename. */
int ini_parse_string(const char* string, ini_handler handler, void* user);

#ifdef __cplusplus
}
#endif

#endif /* __INI_H__ */

#ifdef INI_IMPLEMENTATION

#include <ctype.h>
#include <string.h>

#if !INI_USE_BOM
#define INI_USE_BOM 0
#endif

#define MAX_SECTION 50
#define MAX_NAME 50

/* Used by ini_parse_string() to keep track of string information */
typedef struct {
    const char* ptr;
    size_t num_left;
} ini_parse_string_ctx;

/* Strip whitespace chars off end of given string, in place. Return s. */
static char* rstrip(char* s)
{
    char* p = s + strlen(s);
    while (p > s && isspace((unsigned char)(*--p)))
        *p = '\0';
    return s;
}

/* Return pointer to first non-whitespace char in given string. */
static char* lskip(const char* s)
{
    while (*s && isspace((unsigned char)(*s)))
        s++;
    return (char*)s;
}

/* Return pointer to first char c or ';' in given string, or pointer to
   null at end of string if neither found. ';' must be prefixed by a
   whitespace character to be noticed. */
static char* find_char_or_comment(const char* s, char c)
{
#if INI_ALLOW_INLINE_COMMENTS
    int was_space = 0;
    while (*s && *s != c && !(was_space && *s == ';')) {
        was_space = isspace((unsigned char)(*s));
        s++;
    }
#else
    while (*s && *s != c) {
        s++;
    }
#endif
    return (char*)s;
}

/* Version of strncpy that ensures dest (size bytes) is null-terminated. */
static char* strncpy0(char* dest, const char* src, size_t size)
{
    strncpy(dest, src, size - 1);
    dest[size - 1] = '\0';
    return dest;
}

/* See documentation in header file. */
int ini_parse_file(FILE* file, ini_handler handler, void* user)
{
    /* Uses a fair bit of stack (use heap instead if you need to) */
    char line[200];
    char section[MAX_SECTION] = "";
    char prev_name[MAX_NAME] = "";

    char* start;
    char* end;
    char* name;
    char* value;
    int lineno = 0;
    int error = 0;

    /* Scan through file line by line */
    while (fgets(line, sizeof(line), file) != NULL) {
        lineno++;

        start = line;
#if INI_USE_BOM
        if (lineno == 1 && (unsigned char)start[0] == 0xEF &&
                           (unsigned char)start[1] == 0xBB &&
                           (unsigned char)start[2] == 0xBF) {
            start += 3;
        }
#endif
        start = lskip(rstrip(start));

        if (*start == ';' || *start == '#') {
            /* Per Python configparser, allow '#' comments at start of line */
        }
#if INI_ALLOW_MULTILINE
        else if (*prev_name && *start && start > line) {
            /* Non-blank line with leading whitespace, treat as continuation
               of previous name's value (as per Python configparser). */
            if (!handler(user, section, prev_name, start, lineno) && !error)
                error = lineno;
        }
#endif
        else if (*start == '[') {
            /* A "[section]" line */
            end = find_char_or_comment(start + 1, ']');
            if (*end == ']') {
                *end = '\0';
                strncpy0(section, start + 1, sizeof(section));
                *prev_name = '\0';
            }
            else if (!error) {
                /* No ']' found on section line */
                error = lineno;
            }
        }
        else if (*start) {
            /* A "name=value" line */
            end = find_char_or_comment(start, '=');
            if (*end != '=') {
                end = find_char_or_comment(start, ':');
            }
            if (*end == '=' || *end == ':') {
                *end = '\0';
                name = rstrip(start);
                value = end + 1;
#if INI_ALLOW_INLINE_COMMENTS
                end = find_char_or_comment(value, ';');
                if (*end == ';')
                    *end = '\0';
#endif
                value = lskip(value);
                rstrip(value);

#if INI_SUPPORT_QUOTES
                if (value[0] == '"' && value[strlen(value) - 1] == '"') {
                    value++;
                    value[strlen(value) - 1] = '\0';
                }
#endif

                strncpy0(prev_name, name, sizeof(prev_name));
                if (!handler(user, section, name, value, lineno) && !error)
                    error = lineno;
            }
            else if (!error) {
                /* No '=' or ':' found on name=value line */
                error = lineno;
            }
        }

#if INI_HANDLER_LINENO
        if (error)
            break;
#endif
    }

    return error;
}

/* See documentation in header file. */
int ini_parse(const char* filename, ini_handler handler, void* user)
{
    FILE* file;
    int error;

    file = fopen(filename, "r");
    if (!file)
        return -1;
    error = ini_parse_file(file, handler, user);
    fclose(file);
    return error;
}

/* See documentation in header file. */
int ini_parse_string(const char* string, ini_handler handler, void* user)
{
    ini_parse_string_ctx ctx;
    ctx.ptr = string;
    ctx.num_left = strlen(string);
    return ini_parse_file(NULL, handler, user);
}

#endif /* INI_IMPLEMENTATION */
