BUILD := ../build
OBJDIR := object_files

# Заголовки библиотек из их исходников
INCLUDES := -I../freetype/include -I../libpng -I../zlib -I.

# -Wall -Wextra включают предупреждения
# -O2 включает обычную оптимизацию
# -fPIC делает позиционно-независимый код
# -fno-stack-protector, -D_FORTIFY_SOURCE=0 отключают защиту стека и проверки glibc
HOST_CFLAGS := -Wall -Wextra -O2 -fPIC -fno-stack-protector \
	-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 $(INCLUDES)

# -fPIE в конце заменяет -fPIC: blob не -shared
# -fno-builtin — компилятор не подменяет вызовы libc своими вставками
# -fno-asynchronous-unwind-tables убирает таблицы раскрутки стека
BLOB_CFLAGS := $(HOST_CFLAGS) -std=gnu11 -fPIE -fno-builtin -fno-asynchronous-unwind-tables

# FreeType и libpng ссылаются на zlib, поэтому zlib последний
LIBS := $(BUILD)/libfreetype.a $(BUILD)/libpng.a $(BUILD)/libz.a

.PHONY: static dynamic blob clean

static: $(BUILD)/app-static
dynamic: $(BUILD)/app-dynamic
blob: $(BUILD)/app-blob

# object_files/static/имя.o собирается из имя.c
# -DMODE_STATIC — прямые вызовы библиотек в main.c
$(OBJDIR)/static/%.o: %.c
	@mkdir -p $(dir $@)
	gcc $(HOST_CFLAGS) -DMODE_STATIC -c $< -o $@

# -DMODE_DYNAMIC — funcs.c берёт указатели из dynload
$(OBJDIR)/dynamic/%.o: %.c
	@mkdir -p $(dir $@)
	gcc $(HOST_CFLAGS) -DMODE_DYNAMIC -c $< -o $@

# -DMODE_BLOB смотрит только main.c: точка входа blob_entry
$(OBJDIR)/blob/%.o: %.c
	@mkdir -p $(dir $@)
	gcc $(BLOB_CFLAGS) -DMODE_BLOB -c $< -o $@

# -lm подключает libm
$(BUILD)/app-static: $(OBJDIR)/static/main.o $(OBJDIR)/static/funcs.o $(LIBS)
	@mkdir -p $(BUILD)
	gcc -o $@ $(OBJDIR)/static/main.o $(OBJDIR)/static/funcs.o $(LIBS) -lm

# .so в программу не попадают, их открывает dynload.c
# -ldl подключает libdl
$(BUILD)/app-dynamic: $(OBJDIR)/dynamic/main.o $(OBJDIR)/dynamic/funcs.o \
		$(OBJDIR)/dynamic/dynload.o \
		$(BUILD)/libz.so.1 $(BUILD)/libpng.so $(BUILD)/libfreetype.so
	@mkdir -p $(BUILD)
	gcc -o $@ $(OBJDIR)/dynamic/main.o $(OBJDIR)/dynamic/funcs.o \
		$(OBJDIR)/dynamic/dynload.o -ldl

# -nostdlib — не линковать libc и стартовые файлы
# -pie -fPIE — позиционно-независимый код
# -e blob_entry — точка входа, адрес берётся из заголовка ELF
# --no-undefined — каждое имя определено внутри
# --no-dynamic-linker — нет интерпретатора
# -Bsymbolic — вызовы привязаны к своим символам
# blob.map прячет все символы наружу
# -lgcc — вспомогательные функции компилятора, не libc
$(BUILD)/app-blob: $(OBJDIR)/blob/main.o $(OBJDIR)/blob/funcs.o \
		$(OBJDIR)/blob/host_shim.o \
		$(LIBS) blob.map
	@mkdir -p $(BUILD)
	gcc -nostdlib -pie -fPIE \
		-Wl,-e,blob_entry -Wl,--no-dynamic-linker -Wl,--no-undefined \
		-Wl,-Bsymbolic -Wl,--version-script,blob.map \
		-o $@ $(OBJDIR)/blob/main.o $(OBJDIR)/blob/funcs.o \
		$(OBJDIR)/blob/host_shim.o \
		$(LIBS) -lgcc

clean:
	rm -rf $(OBJDIR)
