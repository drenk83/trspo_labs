#include "host_api.h"
#include "elfload.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

extern int _setjmp(jmp_buf env);

/* Дескриптор blob-файла для pread-колбэка elfload. Загрузчик однопоточный. */
static int blob_fd = -1;

static bool pread_cb(el_ctx *ctx, void *dest, size_t nb, size_t offset)
{
    ssize_t got;

    (void)ctx;
    got = pread(blob_fd, dest, nb, (off_t)offset);
    return got == (ssize_t)nb;
}

/* elfload уже отобразил память целиком; колбэк только возвращает адрес. */
static void *alloc_cb(el_ctx *ctx, Elf_Addr phys, Elf_Addr virt, Elf_Addr size)
{
    (void)ctx;
    (void)phys;
    (void)size;
    return (void *)(uintptr_t)virt;
}

static const char *status_name(el_status status)
{
    switch (status) {
    case EL_OK:
        return "OK";
    case EL_EIO:
        return "read error";
    case EL_ENOMEM:
        return "out of memory";
    case EL_NOTELF:
        return "not an ELF file";
    case EL_WRONGBITS:
        return "wrong ELF class";
    case EL_WRONGENDIAN:
        return "wrong byte order";
    case EL_WRONGARCH:
        return "wrong architecture";
    case EL_WRONGOS:
        return "wrong OS ABI";
    case EL_NOTEXEC:
        return "not an executable";
    case EL_NODYN:
        return "no dynamic section";
    case EL_BADREL:
        return "bad relocation";
    default:
        return "unknown error";
    }
}

/* Настоящие функции libc этого процесса. Blob получит их указателями. */
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
    el_ctx ctx;
    el_status status;
    struct blob_params params;
    unsigned char *mapping;
    size_t map_size;
    int (*entry)(struct blob_params *);

    if (argc != 5) {
        printf("usage: %s blob font png text\n", argc > 0 ? argv[0] : "elfloader");
        return 0;
    }

    blob_fd = open(argv[1], O_RDONLY);
    if (blob_fd < 0) {
        printf("cannot open blob '%s': %s\n", argv[1], strerror(errno));
        return 0;
    }

    memset(&ctx, 0, sizeof ctx);
    ctx.pread = pread_cb;

    status = el_init(&ctx);
    if (status != EL_OK) {
        printf("el_init failed: %s (%d)\n", status_name(status), status);
        close(blob_fd);
        return 0;
    }

    if (ctx.memsz == 0) {
        printf("blob has no loadable segments\n");
        close(blob_fd);
        return 0;
    }

    map_size = (size_t)ctx.memsz;
    mapping = mmap(NULL, map_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapping == MAP_FAILED) {
        printf("mmap failed: %s\n", strerror(errno));
        close(blob_fd);
        return 0;
    }

    ctx.base_load_paddr = ctx.base_load_vaddr = (Elf_Addr)(uintptr_t)mapping;

    status = el_load(&ctx, alloc_cb);
    if (status != EL_OK) {
        printf("el_load failed: %s (%d)\n", status_name(status), status);
        munmap(mapping, map_size);
        close(blob_fd);
        return 0;
    }

    status = el_relocate(&ctx);
    if (status != EL_OK) {
        printf("el_relocate failed: %s (%d)\n", status_name(status), status);
        munmap(mapping, map_size);
        close(blob_fd);
        return 0;
    }

    memset(&params, 0, sizeof params);
    params.font_path = argv[2];
    params.png_path = argv[3];
    params.text = argv[4];
    fill_api(&params.api);

    entry = (int (*)(struct blob_params *))((uintptr_t)mapping +
                                            (uintptr_t)ctx.ehdr.e_entry);
    entry(&params);

    munmap(mapping, map_size);
    close(blob_fd);
    return 0;
}
