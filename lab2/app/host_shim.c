/* у blob нет libc: каждая функция ниже зовёт указатель из таблицы загрузчика */
#include "host_api.h"

#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

/* glibc превращает эти имена в макросы, blob должен экспортировать функции */
#undef memchr
#undef memcpy
#undef memmove
#undef memset
#undef memcmp
#undef strchr
#undef strrchr
#undef strstr
#undef strcmp
#undef strncmp
#undef strcpy
#undef strncpy
#undef strcat
#undef strlen
#undef stpcpy

int (*host_setjmp_fn)(jmp_buf);       /* куда прыгает стаб setjmp */
void (*host_longjmp_fn)(jmp_buf, int); /* куда прыгает стаб longjmp */

FILE *stderr; /* библиотеки пишут ошибки сюда */
FILE *stdout; /* обычный вывод, его тоже даёт загрузчик */

static const struct host_api *api; /* таблица, её ставит host_install */

void host_install(const struct host_api *src)
{
    api = src;
    host_setjmp_fn = src ? src->setjmp_fn : NULL;
    host_longjmp_fn = src ? src->longjmp_fn : NULL;
    if (src) {
        stderr = src->stderr_file;
        stdout = src->stdout_file;
    }
}

FILE *fopen(const char *path, const char *mode)
{
    return (api && api->fopen) ? api->fopen(path, mode) : NULL;
}

int fclose(FILE *stream)
{
    return (api && api->fclose) ? api->fclose(stream) : EOF;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return (api && api->fread) ? api->fread(ptr, size, nmemb, stream) : 0;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return (api && api->fwrite) ? api->fwrite(ptr, size, nmemb, stream) : 0;
}

int fseek(FILE *stream, long offset, int whence)
{
    return (api && api->fseek) ? api->fseek(stream, offset, whence) : -1;
}

long ftell(FILE *stream)
{
    return (api && api->ftell) ? api->ftell(stream) : -1L;
}

int fflush(FILE *stream)
{
    return (api && api->fflush) ? api->fflush(stream) : EOF;
}

int ferror(FILE *stream)
{
    return (api && api->ferror) ? api->ferror(stream) : 1;
}

int fputc(int c, FILE *stream)
{
    return (api && api->fputc) ? api->fputc(c, stream) : EOF;
}

int remove(const char *pathname)
{
    return (api && api->remove) ? api->remove(pathname) : -1;
}

void *malloc(size_t size)
{
    return (api && api->malloc) ? api->malloc(size) : NULL;
}

void *calloc(size_t nmemb, size_t size)
{
    return (api && api->calloc) ? api->calloc(nmemb, size) : NULL;
}

void *realloc(void *ptr, size_t size)
{
    return (api && api->realloc) ? api->realloc(ptr, size) : NULL;
}

void free(void *ptr)
{
    if (api && api->free)
        api->free(ptr);
}

void *memcpy(void *dest, const void *src, size_t n)
{
    return (api && api->memcpy) ? api->memcpy(dest, src, n) : dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
    return (api && api->memmove) ? api->memmove(dest, src, n) : dest;
}

void *memset(void *s, int c, size_t n)
{
    return (api && api->memset) ? api->memset(s, c, n) : s;
}

int memcmp(const void *a, const void *b, size_t n)
{
    return (api && api->memcmp) ? api->memcmp(a, b, n) : 0;
}

void *memchr(const void *s, int c, size_t n)
{
    return (api && api->memchr) ? api->memchr(s, c, n) : NULL;
}

size_t strlen(const char *s)
{
    return (api && api->strlen) ? api->strlen(s) : 0;
}

int strcmp(const char *a, const char *b)
{
    return (api && api->strcmp) ? api->strcmp(a, b) : 0;
}

int strncmp(const char *a, const char *b, size_t n)
{
    return (api && api->strncmp) ? api->strncmp(a, b, n) : 0;
}

char *strcpy(char *dest, const char *src)
{
    return (api && api->strcpy) ? api->strcpy(dest, src) : dest;
}

char *strncpy(char *dest, const char *src, size_t n)
{
    return (api && api->strncpy) ? api->strncpy(dest, src, n) : dest;
}

char *strcat(char *dest, const char *src)
{
    return (api && api->strcat) ? api->strcat(dest, src) : dest;
}

char *strchr(const char *s, int c)
{
    return (api && api->strchr) ? api->strchr(s, c) : NULL;
}

char *strrchr(const char *s, int c)
{
    return (api && api->strrchr) ? api->strrchr(s, c) : NULL;
}

char *strstr(const char *haystack, const char *needle)
{
    return (api && api->strstr) ? api->strstr(haystack, needle) : NULL;
}

char *stpcpy(char *dest, const char *src)
{
    return (api && api->stpcpy) ? api->stpcpy(dest, src) : dest;
}

double strtod(const char *nptr, char **endptr)
{
    return (api && api->strtod) ? api->strtod(nptr, endptr) : 0.0;
}

char *strerror(int errnum)
{
    return (api && api->strerror) ? api->strerror(errnum) : (char *)"unknown";
}

/* имя blob_strtol, а не strtol: иначе glibc подменит его на __isoc23_strtol */
long blob_strtol(const char *nptr, char **endptr, int base)
{
    return (api && api->strtol) ? api->strtol(nptr, endptr, base) : 0;
}

char *getenv(const char *name)
{
    return (api && api->getenv) ? api->getenv(name) : NULL;
}

int printf(const char *fmt, ...)
{
    va_list ap;
    int result;

    if (!api || !api->vprintf)
        return -1;
    va_start(ap, fmt);
    result = api->vprintf(fmt, ap);
    va_end(ap);
    return result;
}

int sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    int result;

    if (!api || !api->vsprintf)
        return -1;
    va_start(ap, fmt);
    result = api->vsprintf(buf, fmt, ap);
    va_end(ap);
    return result;
}

int fprintf(FILE *stream, const char *fmt, ...)
{
    va_list ap;
    int result;

    if (!api || !api->vfprintf)
        return -1;
    va_start(ap, fmt);
    result = api->vfprintf(stream, fmt, ap);
    va_end(ap);
    return result;
}

int snprintf(char *buf, size_t n, const char *fmt, ...)
{
    va_list ap;
    int result;

    if (!api || !api->vsnprintf)
        return -1;
    va_start(ap, fmt);
    result = api->vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return result;
}

void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *))
{
    if (api && api->qsort)
        api->qsort(base, nmemb, size, compar);
}

double pow(double x, double y)
{
    return (api && api->pow) ? api->pow(x, y) : 0.0;
}

double frexp(double x, int *exp)
{
    return (api && api->frexp) ? api->frexp(x, exp) : 0.0;
}

double modf(double x, double *iptr)
{
    return (api && api->modf) ? api->modf(x, iptr) : 0.0;
}

double floor(double x)
{
    return (api && api->floor) ? api->floor(x) : 0.0;
}

double fabs(double x)
{
    return (api && api->fabs) ? api->fabs(x) : 0.0;
}

int clock_gettime(clockid_t clk, struct timespec *ts)
{
    return (api && api->clock_gettime) ? api->clock_gettime(clk, ts) : -1;
}

struct tm *gmtime(const time_t *timep)
{
    return (api && api->gmtime) ? api->gmtime(timep) : NULL;
}

int *__errno_location(void)
{
    static int fallback; /* если таблица ещё не стоит */

    if (api && api->errno_location)
        return api->errno_location();
    return &fallback;
}

void abort(void)
{
    if (api && api->abort_fn)
        api->abort_fn();
    for (;;)
        ;
}

int blob_open(const char *path, int flags, ...)
{
    return (api && api->open_fn) ? api->open_fn(path, flags, 0) : -1;
}

int blob_close(int fd)
{
    return (api && api->close_fn) ? api->close_fn(fd) : -1;
}

ssize_t blob_read(int fd, void *buf, size_t count)
{
    return (api && api->read_fn) ? api->read_fn(fd, buf, count) : -1;
}

int blob_fcntl(int fd, int cmd, ...)
{
    va_list ap;
    int arg;

    va_start(ap, cmd);
    arg = va_arg(ap, int);
    va_end(ap);
    return (api && api->fcntl_fn) ? api->fcntl_fn(fd, cmd, arg) : -1;
}

int blob_fstat(int fd, struct stat *st)
{
    return (api && api->fstat_fn) ? api->fstat_fn(fd, st) : -1;
}

void *blob_mmap(void *addr, size_t length, int prot, int flags, int fd,
                       off_t offset)
{
    return (api && api->mmap_fn) ? api->mmap_fn(addr, length, prot, flags, fd, offset)
                                 : (void *)-1;
}

int blob_munmap(void *addr, size_t length)
{
    return (api && api->munmap_fn) ? api->munmap_fn(addr, length) : -1;
}

/* библиотеки зовут open и strtol, а не blob_* */
__asm__(
    ".globl strtol\n\t.set strtol, blob_strtol\n\t"
    ".globl open\n\t.set open, blob_open\n\t"
    ".globl close\n\t.set close, blob_close\n\t"
    ".globl read\n\t.set read, blob_read\n\t"
    ".globl fcntl\n\t.set fcntl, blob_fcntl\n\t"
    ".globl fstat\n\t.set fstat, blob_fstat\n\t"
    ".globl mmap\n\t.set mmap, blob_mmap\n\t"
    ".globl munmap\n\t.set munmap, blob_munmap\n\t"
);
