BUILD := ../build
OBJDIR := object_files

# Заголовки библиотек — из их исходников, не из установки.
# pnglibconf.h лежит рядом с остальными заголовками libpng.
INCLUDES := -I../freetype/include -I../libpng -I../zlib -I.
# Те же ограничения, что у библиотек: blob не может звать __stack_chk_fail.
HOST_CFLAGS := -Wall -Wextra -O2 -fPIC -fno-stack-protector \
	-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -MMD -MP $(INCLUDES)
# -fPIE в конце заменяет -fPIC из общих флагов: blob больше не -shared.
# -std=gnu11: при C23 glibc переименовывает strtol в __isoc23_strtol.
# -fno-builtin — компилятор не подменяет вызовы libc своими вставками.
BLOB_CFLAGS := $(HOST_CFLAGS) -std=gnu11 -fPIE -fno-builtin -fno-asynchronous-unwind-tables

# Порядок важен: FreeType и libpng ссылаются на zlib, zlib — ни на кого.
LIBS := $(BUILD)/libfreetype.a $(BUILD)/libpng.a $(BUILD)/libz.a

.PHONY: static dynamic blob clean

static: $(BUILD)/app-static
dynamic: $(BUILD)/app-dynamic
blob: $(BUILD)/app-blob

# %.o: %.c — из main.c получается object_files/static/main.o и так далее.
# -DMODE_STATIC попадает и в funcs.c. Там проверяется только MODE_DYNAMIC,
# поэтому лишний макрос поведение не меняет.
$(OBJDIR)/static/%.o: %.c
	@mkdir -p $(dir $@)
	gcc $(HOST_CFLAGS) -DMODE_STATIC -c $< -o $@

$(OBJDIR)/dynamic/%.o: %.c
	@mkdir -p $(dir $@)
	gcc $(HOST_CFLAGS) -DMODE_DYNAMIC -c $< -o $@

# MODE_BLOB смотрит только main.c. На funcs.c и host_shim.c флаг безразличен.
$(OBJDIR)/blob/%.o: %.c
	@mkdir -p $(dir $@)
	gcc $(BLOB_CFLAGS) -DMODE_BLOB -c $< -o $@

# Статически вшиты только три библиотеки. libc и libm остаются динамическими:
# задание не требует полностью статический бинарник.
$(BUILD)/app-static: $(OBJDIR)/static/main.o $(OBJDIR)/static/funcs.o $(LIBS)
	@mkdir -p $(BUILD)
	gcc -o $@ $(OBJDIR)/static/main.o $(OBJDIR)/static/funcs.o $(LIBS) -lm

# В бинарник библиотеки не попадают. .so в зависимостях нужны, чтобы
# make перелинковал программу, если библиотека пересобралась.
# Имена .so открывает dynload.c из каталога самого бинарника.
$(BUILD)/app-dynamic: $(OBJDIR)/dynamic/main.o $(OBJDIR)/dynamic/funcs.o \
		$(OBJDIR)/dynamic/dynload.o \
		$(BUILD)/libz.so.1 $(BUILD)/libpng.so $(BUILD)/libfreetype.so
	@mkdir -p $(BUILD)
	gcc -o $@ $(OBJDIR)/dynamic/main.o $(OBJDIR)/dynamic/funcs.o \
		$(OBJDIR)/dynamic/dynload.o -ldl

# -nostdlib — не линковать libc и стартовые файлы.
# --no-undefined — каждое имя определено внутри, таблицы импорта нет.
# --no-dynamic-linker — нет интерпретатора.
# blob.map прячет все символы, таблицы экспорта нет.
# -e blob_entry — точка входа, загрузчик берёт её адрес из заголовка ELF.
# -lgcc — вспомогательные функции компилятора, не libc.
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

-include $(wildcard $(OBJDIR)/*/*.d)
