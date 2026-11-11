BUILD := ../build

# -O2 включает обычную оптимизацию
# -fPIC делает позиционно-независимый код
# -fno-stack-protector, -D_FORTIFY_SOURCE=0 отключают защиту стека и проверки glibc
CFLAGS := -O2 -fPIC -fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0

SRCS := adler32.c crc32.c deflate.c infback.c inffast.c inflate.c inftrees.c \
	trees.c zutil.c compress.c uncompr.c
# adler32.c -> object_files/adler32.o
OBJS := $(addprefix object_files/,$(SRCS:.c=.o))

.PHONY: static dynamic blob clean

# static и blob используют один и тот же архив
static blob: $(BUILD)/libz.a
dynamic: $(BUILD)/libz.so.1

# object_files/имя.o собирается из имя.c в этом каталоге
# $< — первый исходник (.c), $@ — цель (.o)
object_files/%.o: %.c
	@mkdir -p $(dir $@)
	gcc $(CFLAGS) -c $< -o $@

# Создаёт статическую библиотеку libz.a
# ar rcs: r заменить/добавить .o, c создать архив, s построить индекс
$(BUILD)/libz.a: $(OBJS)
	@mkdir -p $(BUILD)
	ar rcs $@ $(OBJS)

# soname libz.so.1 — то имя, которое потом открывает app-dynamic
# ln -sf: s создать ссылку, f перезаписать
$(BUILD)/libz.so.1: $(OBJS)
	@mkdir -p $(BUILD)
	gcc -shared -Wl,-soname,libz.so.1 -o $@ $(OBJS)
	ln -sf libz.so.1 $(BUILD)/libz.so

clean:
	rm -rf object_files
	rm -f $(BUILD)/libz.a $(BUILD)/libz.so $(BUILD)/libz.so.1
