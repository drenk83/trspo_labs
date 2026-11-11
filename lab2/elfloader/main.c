#include "host_api.h"
#include "elf_loader.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/* файл blob целиком: загрузчик потом копирует сегменты в свою память */
static int read_file(const char *path, void **buf, size_t *len)
{
    int fd;
    struct stat st;
    unsigned char *data;
    size_t got;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;
    if (fstat(fd, &st) != 0 || st.st_size <= 0) {
        close(fd);
        return -1;
    }

    data = malloc((size_t)st.st_size);
    if (!data) {
        close(fd);
        return -1;
    }

    got = 0;
    while (got < (size_t)st.st_size) {
        ssize_t n;

        n = read(fd, data + got, (size_t)st.st_size - got);
        if (n < 0) {
            free(data);
            close(fd);
            return -1;
        }
        if (n == 0)
            break;
        got += (size_t)n;
    }
    close(fd);
    if (got != (size_t)st.st_size) {
        free(data);
        return -1;
    }

    *buf = data;
    *len = got;
    return 0;
}

/* настоящие функции libc этого процесса, blob получит их указателями */
static void fill_api(struct host_api *api)
{
    memset(api, 0, sizeof *api);
    api->fopen = fopen;
    api->fclose = fclose;
    api->fread = fread;
    api->fwrite = fwrite;
    api->fseek = fseek;
    api->ftell = ftell;
    api->fflush = fflush;
    api->ferror = ferror;
    api->fputc = fputc;
    api->remove = remove;
    api->malloc = malloc;
    api->calloc = calloc;
    api->realloc = realloc;
    api->free = free;
    api->memcpy = memcpy;
    api->memmove = memmove;
    api->memset = memset;
    api->memcmp = memcmp;
    api->memchr = memchr;
    api->strlen = strlen;
    api->strcmp = strcmp;
    api->strncmp = strncmp;
    api->strcpy = strcpy;
    api->strncpy = strncpy;
    api->strcat = strcat;
    api->strchr = strchr;
    api->strrchr = strrchr;
    api->strstr = strstr;
    api->stpcpy = stpcpy;
    api->strtol = strtol;
    api->strtod = strtod;
    api->getenv = getenv;
    api->strerror = strerror;
    api->vprintf = vprintf;
    api->vsprintf = vsprintf;
    api->vfprintf = vfprintf;
    api->vsnprintf = vsnprintf;
    api->qsort = qsort;
    api->pow = pow;
    api->frexp = frexp;
    api->modf = modf;
    api->floor = floor;
    api->fabs = fabs;
    api->clock_gettime = clock_gettime;
    api->gmtime = gmtime;
    api->setjmp_fn = _setjmp;
    api->longjmp_fn = longjmp;
    api->errno_location = __errno_location;
    /* open и fcntl в libc с переменным числом аргументов, в таблице сигнатура фиксирована */
    api->open_fn = (int (*)(const char *, int, mode_t))open;
    api->close_fn = close;
    api->read_fn = read;
    api->fcntl_fn = (int (*)(int, int, int))fcntl;
    api->fstat_fn = fstat;
    api->mmap_fn = mmap;
    api->munmap_fn = munmap;
    api->stderr_file = stderr;
    api->stdout_file = stdout;
    api->abort_fn = abort;
}

int main(int argc, char **argv)
{
    void *bin;                          /* байты файла app-blob */
    size_t len;                         /* сколько байт прочитали */
    struct elf_module *module;          /* образ после релокаций */
    struct blob_params params;          /* пути, текст и таблица libc для blob */
    int (*entry)(struct blob_params *); /* адрес blob_entry после загрузки */
    const char *name;                   /* имя модуля для unload */

    if (argc != 5) {
        printf("usage: %s blob font png text\n", argc > 0 ? argv[0] : "elfloader");
        return 0;
    }

    if (read_file(argv[1], &bin, &len) != 0) {
        printf("cannot open blob '%s': %s\n", argv[1], strerror(errno));
        return 0;
    }

    name = filename_from_path(argv[1]);
    module = load_elf_module(name, bin, len);
    free(bin);
    if (!module) {
        printf("cannot load blob '%s'\n", argv[1]);
        return 0;
    }
    if (module->entry == 0) {
        printf("blob has no entry point\n");
        unload_elf_module(name);
        return 0;
    }

    memset(&params, 0, sizeof params);
    params.font_path = argv[2];
    params.png_path = argv[3];
    params.text = argv[4];
    fill_api(&params.api);

    /* e_entry уже сложен с базой загрузки внутри load_elf_module */
    entry = (int (*)(struct blob_params *))module->entry;
    entry(&params);

    unload_elf_module(name);
    return 0;
}
