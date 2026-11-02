# elfload

Исходники библиотеки загрузчика ELF взяты из проекта:

https://github.com/erincandescent/elfload

Зафиксированный commit:

d1f9da8b03a993df7b6c2b03f863c649451d00a2

В сборку включены только файлы, необходимые для x86-64:

- elfload.c
- elfload.h
- elfarch.h
- elf.h
- elfreloc_amd64.c

Файлы elfloader.c, sample.c, elfreloc_aarch64.c и elfreloc_i386.c не используются.
