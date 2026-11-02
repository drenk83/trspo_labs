#define _GNU_SOURCE
#include "host_api.h"

#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

extern int _setjmp(jmp_buf env);

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

static int read_file(const char *path, unsigned char **bytes, size_t *length)
{
    FILE *fp;
    long size;

    fp = fopen(path, "rb");
    if (!fp) {
        printf("cannot open blob '%s'\n", path);
        return 0;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return 0;
    }
    size = ftell(fp);
    if (size < (long)sizeof(Elf64_Ehdr)) {
        printf("blob '%s' is too small\n", path);
        fclose(fp);
        return 0;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return 0;
    }
    *bytes = malloc((size_t)size);
    if (!*bytes) {
        fclose(fp);
        return 0;
    }
    if (fread(*bytes, 1, (size_t)size, fp) != (size_t)size) {
        printf("cannot read blob '%s'\n", path);
        free(*bytes);
        *bytes = NULL;
        fclose(fp);
        return 0;
    }
    fclose(fp);
    *length = (size_t)size;
    return 1;
}

/* PIE собран так, будто его загрузили по адресу 0.
 * bias — куда mmap положил образ на самом деле.
 * R_X86_64_RELATIVE: записать в ячейку bias + addend. Других типов быть не должно:
 * у blob нет импортов. */
static int apply_relocations(uintptr_t bias, Elf64_Dyn *dynamic)
{
    Elf64_Rela *rela = NULL;
    Elf64_Xword rela_size = 0;
    Elf64_Xword rela_ent = sizeof(Elf64_Rela);

    for (Elf64_Dyn *entry = dynamic; entry->d_tag != DT_NULL; entry++) {
        if (entry->d_tag == DT_NEEDED) {
            printf("blob has an import (DT_NEEDED)\n");
            return 0;
        }
        if (entry->d_tag == DT_RELA)
            rela = (Elf64_Rela *)(bias + entry->d_un.d_ptr);
        else if (entry->d_tag == DT_RELASZ)
            rela_size = entry->d_un.d_val;
        else if (entry->d_tag == DT_RELAENT)
            rela_ent = entry->d_un.d_val;
    }
    if (!rela || rela_ent == 0)
        return 1;

    for (Elf64_Xword offset = 0; offset < rela_size; offset += rela_ent) {
        Elf64_Rela *item = (Elf64_Rela *)((unsigned char *)rela + offset);
        unsigned int type = ELF64_R_TYPE(item->r_info);
        uint64_t *slot;

        if (type == R_X86_64_NONE)
            continue;
        if (type != R_X86_64_RELATIVE) {
            printf("unsupported relocation type %u\n", type);
            return 0;
        }
        slot = (uint64_t *)(bias + item->r_offset);
        *slot = bias + (uint64_t)item->r_addend;
    }
    return 1;
}

static int load_blob(unsigned char *file, size_t length, struct blob_params *params)
{
    Elf64_Ehdr *ehdr;
    Elf64_Phdr *phdr;
    uintptr_t min_addr = ~(uintptr_t)0;
    uintptr_t max_addr = 0;
    long page;
    uintptr_t span;
    unsigned char *mapping;
    uintptr_t bias;
    Elf64_Dyn *dynamic = NULL;
    int (*entry)(struct blob_params *);

    if (length < sizeof(Elf64_Ehdr))
        return 0;
    ehdr = (Elf64_Ehdr *)file;
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0 ||
        ehdr->e_ident[EI_CLASS] != ELFCLASS64 ||
        ehdr->e_ident[EI_DATA] != ELFDATA2LSB ||
        ehdr->e_machine != EM_X86_64 || ehdr->e_type != ET_DYN) {
        printf("blob is not an x86-64 ET_DYN ELF\n");
        return 0;
    }
    if (ehdr->e_phoff + (size_t)ehdr->e_phnum * ehdr->e_phentsize > length ||
        ehdr->e_phentsize != sizeof(Elf64_Phdr)) {
        printf("blob program headers are invalid\n");
        return 0;
    }
    phdr = (Elf64_Phdr *)(file + ehdr->e_phoff);

    /* Найти, сколько памяти занимают все PT_LOAD. */
    for (int i = 0; i < ehdr->e_phnum; i++) {
        uintptr_t end;
        if (phdr[i].p_type != PT_LOAD)
            continue;
        if (phdr[i].p_offset + phdr[i].p_filesz > length) {
            printf("blob segment is out of file\n");
            return 0;
        }
        if (phdr[i].p_vaddr < min_addr)
            min_addr = phdr[i].p_vaddr;
        end = phdr[i].p_vaddr + phdr[i].p_memsz;
        if (end > max_addr)
            max_addr = end;
    }
    if (min_addr == ~(uintptr_t)0 || ehdr->e_entry == 0) {
        printf("blob has no loadable entry\n");
        return 0;
    }

    page = sysconf(_SC_PAGESIZE);
    min_addr &= ~(uintptr_t)(page - 1);
    max_addr = (max_addr + (uintptr_t)page - 1) & ~(uintptr_t)(page - 1);
    span = max_addr - min_addr;
    mapping = mmap(NULL, span, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
                   -1, 0);
    if (mapping == MAP_FAILED) {
        printf("mmap failed\n");
        return 0;
    }
    /* Адрес, по которому оказался бы байт с виртуальным адресом 0. */
    bias = (uintptr_t)mapping - min_addr;

    /* Скопировать сегменты и обнулить хвост (BSS). */
    for (int i = 0; i < ehdr->e_phnum; i++) {
        unsigned char *dest;
        if (phdr[i].p_type == PT_DYNAMIC)
            dynamic = (Elf64_Dyn *)(bias + phdr[i].p_vaddr);
        if (phdr[i].p_type != PT_LOAD)
            continue;
        dest = (unsigned char *)(bias + phdr[i].p_vaddr);
        memcpy(dest, file + phdr[i].p_offset, phdr[i].p_filesz);
        if (phdr[i].p_memsz > phdr[i].p_filesz)
            memset(dest + phdr[i].p_filesz, 0, phdr[i].p_memsz - phdr[i].p_filesz);
    }

    if (!dynamic || !apply_relocations(bias, dynamic)) {
        munmap(mapping, span);
        return 0;
    }

    /* Права страниц — после релокаций, иначе в код и данные ещё нельзя писать. */
    for (int i = 0; i < ehdr->e_phnum; i++) {
        uintptr_t start;
        uintptr_t end;
        int prot = 0;
        if (phdr[i].p_type != PT_LOAD)
            continue;
        start = (bias + phdr[i].p_vaddr) & ~(uintptr_t)(page - 1);
        end = (bias + phdr[i].p_vaddr + phdr[i].p_memsz + (uintptr_t)page - 1) &
              ~(uintptr_t)(page - 1);
        if (phdr[i].p_flags & PF_R)
            prot |= PROT_READ;
        if (phdr[i].p_flags & PF_W)
            prot |= PROT_WRITE;
        if (phdr[i].p_flags & PF_X)
            prot |= PROT_EXEC;
        if (mprotect((void *)start, end - start, prot) != 0) {
            printf("mprotect failed\n");
            munmap(mapping, span);
            return 0;
        }
    }

    /* e_entry — смещение blob_entry внутри файла, не имя символа. */
    entry = (int (*)(struct blob_params *))(bias + ehdr->e_entry);
    entry(params);
    munmap(mapping, span);
    return 1;
}

int main(int argc, char **argv)
{
    unsigned char *file = NULL;
    size_t length = 0;
    struct blob_params params;

    if (argc != 5) {
        printf("usage: %s blob font png text\n", argc > 0 ? argv[0] : "elfloader");
        return 0;
    }
    if (!read_file(argv[1], &file, &length))
        return 0;

    memset(&params, 0, sizeof params);
    params.font_path = argv[2];
    params.png_path = argv[3];
    params.text = argv[4];
    fill_api(&params.api);
    load_blob(file, length, &params);
    free(file);
    return 0;
}
