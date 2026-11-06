# Своя сборка libpng. configure не запускается.
# pnglibconf.h берётся из готового scripts/pnglibconf.h.prebuilt.
# pngtest.c и SIMD (arm/, intel/, mips/, powerpc/) не компилируются.

BUILD := ../build
# -Iobject_files — туда копируется pnglibconf.h.
# -I../zlib — заголовок zlib.h.
CFLAGS := -O2 -fPIC -fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
	-MMD -MP -Iobject_files -I../zlib

SRCS := png.c pngerror.c pngget.c pngmem.c pngpread.c pngread.c pngrio.c \
	pngrtran.c pngrutil.c pngset.c pngtrans.c pngwio.c pngwrite.c \
	pngwtran.c pngwutil.c
OBJS := $(addprefix object_files/,$(SRCS:.c=.o))

.PHONY: static dynamic blob clean

static blob: $(BUILD)/libpng.a
dynamic: $(BUILD)/libpng.so

# Заголовок не кладём в дерево исходников: только в object_files/.
object_files/pnglibconf.h: scripts/pnglibconf.h.prebuilt
	@mkdir -p $(dir $@)
	cp $< $@

# Каждый .o зависит от заголовка, иначе первая сборка начнётся раньше копии.
object_files/%.o: %.c object_files/pnglibconf.h
	@mkdir -p $(dir $@)
	gcc $(CFLAGS) -c $< -o $@

$(BUILD)/libpng.a: $(OBJS)
	@mkdir -p $(BUILD)
	ar rcs $@ $(OBJS)

# Пререквизит libz.so.1: make не начнёт .so, пока zlib не собран.
# $$ORIGIN в рецепте становится $ORIGIN для линкера.
# Это каталог самой libpng.so, рядом с ней лежит libz.so.1.
# -lm: в libpng есть pow и floor, без libm dlopen падает.
$(BUILD)/libpng.so: $(OBJS) $(BUILD)/libz.so.1
	@mkdir -p $(BUILD)
	gcc -shared -Wl,-soname,libpng.so -Wl,-rpath,'$$ORIGIN' \
		-o $@ $(OBJS) -L$(BUILD) -lz -lm

clean:
	rm -rf object_files
	rm -f $(BUILD)/libpng.a $(BUILD)/libpng.so

-include $(wildcard object_files/*.d)
