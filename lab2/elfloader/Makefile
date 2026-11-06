# Загрузчик blob. Обычная программа со своей libc, не blob.
# Собирается одним вызовом gcc: исходников мало, отдельные .o не нужны.

BUILD := ../build

SRCS := main.c elfload/elfload.c elfload/elfreloc_amd64.c
# Заголовок таблицы функций лежит в app/.
HDRS := ../app/host_api.h elfload/elfload.h elfload/elfarch.h elfload/elf.h

.PHONY: all clean

all: $(BUILD)/elfloader

# _GNU_SOURCE включает pread, mmap и остальные POSIX-объявления.
# -I../app — найти host_api.h. -Ielfload — заголовки elfload.
# -lm: обёртка загрузчика пользуется libm так же, как и программа.
$(BUILD)/elfloader: $(SRCS) $(HDRS)
	@mkdir -p $(BUILD)
	gcc -D_GNU_SOURCE -Wall -Wextra -O2 -I../app -Ielfload \
		-o $@ $(SRCS) -lm

clean:
	rm -f $(BUILD)/elfloader
