# Своя сборка zlib.
# gz*.c не входят: файловый gzip программе не нужен и тянет лишние
# вызовы libc, которых у blob нет.

BUILD := ../build

# -fPIC нужен и .so, и .a: архив вшивается в PIE-blob.
# Защита стека и FORTIFY тянут символы glibc (__stack_chk_fail и др.).
# -MMD -MP записывает в .d файлы, от каких заголовков зависит .o.
CFLAGS := -O2 -fPIC -fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -MMD -MP

SRCS := adler32.c crc32.c deflate.c infback.c inffast.c inflate.c inftrees.c \
	trees.c zutil.c compress.c uncompr.c
# adler32.c -> object_files/adler32.o
OBJS := $(addprefix object_files/,$(SRCS:.c=.o))

.PHONY: static dynamic blob clean

# static и blob используют один и тот же архив.
static blob: $(BUILD)/libz.a
dynamic: $(BUILD)/libz.so.1

# Шаблон: любой object_files/имя.o собирается из имя.c в этом каталоге.
# $< — первый пререквизит (исходник), $@ — цель (.o).
object_files/%.o: %.c
	@mkdir -p $(dir $@)
	gcc $(CFLAGS) -c $< -o $@

# ar rcs: r заменить/добавить .o, c создать архив, s построить индекс.
$(BUILD)/libz.a: $(OBJS)
	@mkdir -p $(BUILD)
	ar rcs $@ $(OBJS)

# soname libz.so.1 — то имя, которое потом открывает app-dynamic.
# Симлинк libz.so нужен линкеру: флаг -lz ищет файл libz.so.
$(BUILD)/libz.so.1: $(OBJS)
	@mkdir -p $(BUILD)
	gcc -shared -Wl,-soname,libz.so.1 -o $@ $(OBJS)
	ln -sfn libz.so.1 $(BUILD)/libz.so

clean:
	rm -rf object_files
	rm -f $(BUILD)/libz.a $(BUILD)/libz.so $(BUILD)/libz.so.1

# Подключить зависимости от заголовков. «-» — не ошибка, если .d ещё нет.
-include $(wildcard object_files/*.d)
