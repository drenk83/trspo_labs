BUILD := ../build
# -std=gnu11: при стандарте C23 glibc подменяет strtol на __isoc23_strtol,
# а blob определяет только имя strtol.
# -DFT2_BUILD_LIBRARY — внутренние заголовки FreeType.
# -DFT_CONFIG_OPTION_SYSTEM_ZLIB — ftgzip зовёт наш zlib, а не свою копию.
#   Иначе имена inflate/crc32 столкнутся с libz.a.
# bzip2, встроенный PNG и harfbuzz в ftoption.h уже выключены.
CFLAGS := -std=gnu11 -O2 -fPIC -fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
	-Iinclude -I../zlib -DFT2_BUILD_LIBRARY -DFT_CONFIG_OPTION_SYSTEM_ZLIB

.PHONY: static dynamic blob clean

static blob: $(BUILD)/libfreetype.a
dynamic: $(BUILD)/libfreetype.so

# Имя .o — это имя .c без каталога
OBJS := \
	object_files/ftsystem.o object_files/ftinit.o object_files/ftdebug.o \
	object_files/ftbase.o object_files/ftbitmap.o object_files/ftglyph.o \
	object_files/autofit.o object_files/truetype.o object_files/type1.o \
	object_files/cff.o object_files/type1cid.o object_files/pfr.o \
	object_files/type42.o object_files/winfnt.o object_files/pcf.o \
	object_files/bdf.o object_files/sfnt.o object_files/psaux.o \
	object_files/psnames.o object_files/pshinter.o object_files/raster.o \
	object_files/smooth.o object_files/ftgzip.o object_files/ftlzw.o

object_files/ftsystem.o: src/base/ftsystem.c
object_files/ftinit.o: src/base/ftinit.c
object_files/ftdebug.o: src/base/ftdebug.c
object_files/ftbase.o: src/base/ftbase.c
object_files/ftbitmap.o: src/base/ftbitmap.c
object_files/ftglyph.o: src/base/ftglyph.c
object_files/autofit.o: src/autofit/autofit.c
object_files/truetype.o: src/truetype/truetype.c
object_files/type1.o: src/type1/type1.c
object_files/cff.o: src/cff/cff.c
object_files/type1cid.o: src/cid/type1cid.c
object_files/pfr.o: src/pfr/pfr.c
object_files/type42.o: src/type42/type42.c
object_files/winfnt.o: src/winfonts/winfnt.c
object_files/pcf.o: src/pcf/pcf.c
object_files/bdf.o: src/bdf/bdf.c
object_files/sfnt.o: src/sfnt/sfnt.c
object_files/psaux.o: src/psaux/psaux.c
object_files/psnames.o: src/psnames/psnames.c
object_files/pshinter.o: src/pshinter/pshinter.c
object_files/raster.o: src/raster/raster.c
object_files/smooth.o: src/smooth/smooth.c
object_files/ftgzip.o: src/gzip/ftgzip.c
object_files/ftlzw.o: src/lzw/ftlzw.c

object_files/%.o:
	@mkdir -p $(dir $@)
	gcc $(CFLAGS) -c $< -o $@

# Создаёт статическую библиотеку libfreetype.a
# ar rcs: r заменить/добавить .o, c создать архив, s построить индекс
$(BUILD)/libfreetype.a: $(OBJS)
	@mkdir -p $(BUILD)
	ar rcs $@ $(OBJS)

# -lz подключает zlib
# -lm подключает libm (pow, floor)
$(BUILD)/libfreetype.so: $(OBJS) $(BUILD)/libz.so.1
	@mkdir -p $(BUILD)
	gcc -shared -Wl,-soname,libfreetype.so -Wl,-rpath,'$$ORIGIN' \
		-o $@ $(OBJS) -L$(BUILD) -lz -lm

clean:
	rm -rf object_files
	rm -f $(BUILD)/libfreetype.a $(BUILD)/libfreetype.so