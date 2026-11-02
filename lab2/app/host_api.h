#ifndef HOST_API_H
#define HOST_API_H

/* Таблица функций libc. Её заполняет elfloader и передаёт в blob_entry. */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>

struct host_api {
    FILE *(*fopen)(const char *path, const char *mode);
    int (*fclose)(FILE *stream);
    size_t (*fread)(void *ptr, size_t size, size_t nmemb, FILE *stream);
    size_t (*fwrite)(const void *ptr, size_t size, size_t nmemb, FILE *stream);
    int (*fseek)(FILE *stream, long offset, int whence);
    long (*ftell)(FILE *stream);
    int (*fflush)(FILE *stream);
    int (*ferror)(FILE *stream);
    int (*fputc)(int c, FILE *stream);
    int (*remove)(const char *pathname);

    void *(*malloc)(size_t size);
    void *(*calloc)(size_t nmemb, size_t size);
    void *(*realloc)(void *ptr, size_t size);
    void (*free)(void *ptr);

    void *(*memcpy)(void *dest, const void *src, size_t n);
    void *(*memmove)(void *dest, const void *src, size_t n);
    void *(*memset)(void *s, int c, size_t n);
    int (*memcmp)(const void *a, const void *b, size_t n);
    void *(*memchr)(const void *s, int c, size_t n);

    size_t (*strlen)(const char *s);
    int (*strcmp)(const char *a, const char *b);
    int (*strncmp)(const char *a, const char *b, size_t n);
    char *(*strcpy)(char *dest, const char *src);
    char *(*strncpy)(char *dest, const char *src, size_t n);
    char *(*strcat)(char *dest, const char *src);
    char *(*strchr)(const char *s, int c);
    char *(*strrchr)(const char *s, int c);
    char *(*strstr)(const char *haystack, const char *needle);
    char *(*stpcpy)(char *dest, const char *src);
    long (*strtol)(const char *nptr, char **endptr, int base);
    double (*strtod)(const char *nptr, char **endptr);
    char *(*getenv)(const char *name);
    char *(*strerror)(int errnum);

    int (*vprintf)(const char *fmt, va_list ap);
    int (*vsprintf)(char *buf, const char *fmt, va_list ap);
    int (*vfprintf)(FILE *stream, const char *fmt, va_list ap);
    int (*vsnprintf)(char *buf, size_t n, const char *fmt, va_list ap);

    void (*qsort)(void *base, size_t nmemb, size_t size,
                  int (*compar)(const void *, const void *));

    double (*pow)(double x, double y);
    double (*frexp)(double x, int *exp);
    double (*modf)(double x, double *iptr);
    double (*floor)(double x);
    double (*fabs)(double x);

    int (*clock_gettime)(clockid_t clk, struct timespec *ts);
    struct tm *(*gmtime)(const time_t *timep);
    int (*setjmp_fn)(jmp_buf env);
    void (*longjmp_fn)(jmp_buf env, int val);
    int *(*errno_location)(void);

    FILE *stderr_file;
    FILE *stdout_file;
    void (*abort_fn)(void);
};

struct blob_params {
    const char *font_path;
    const char *png_path;
    const char *text;
    struct host_api api;
};

void host_install(const struct host_api *api);

#endif
