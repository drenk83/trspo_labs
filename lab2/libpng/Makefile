BUILD := ../build
# -I../zlib — заголовок zlib.h.
CFLAGS := -O2 -fPIC -fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
	-I../zlib

SRCS := png.c pngerror.c pngget.c pngmem.c pngpread.c pngread.c pngrio.c \
	pngrtran.c pngrutil.c pngset.c pngtrans.c pngwio.c pngwrite.c \
	pngwtran.c pngwutil.c
OBJS := $(addprefix object_files/,$(SRCS:.c=.o))

.PHONY: static dynamic blob clean

static blob: $(BUILD)/libpng.a
dynamic: $(BUILD)/libpng.so

# object_files/имя.o собирается из имя.c в этом каталоге
object_files/%.o: %.c
	@mkdir -p $(dir $@)
	gcc $(CFLAGS) -c $< -o $@

# Создаёт статическую библиотеку libpng.a
# ar rcs: r заменить/добавить .o, c создать архив, s построить индекс
$(BUILD)/libpng.a: $(OBJS)
	@mkdir -p $(BUILD)
	ar rcs $@ $(OBJS)

# soname libpng.so — то имя, которое потом открывает app-dynamic
# L$(BUILD) добавляет ../build в поиск библиотек
# -lz подключает zlib
# -lm подключает math.h
# $ORIGIN — каталог, где лежит сама libpng.so
$(BUILD)/libpng.so: $(OBJS) $(BUILD)/libz.so.1
	@mkdir -p $(BUILD)
	gcc -shared -Wl,-soname,libpng.so -Wl,-rpath,'$$ORIGIN' \
		-o $@ $(OBJS) -L$(BUILD) -lz -lm

clean:
	rm -rf object_files
	rm -f $(BUILD)/libpng.a $(BUILD)/libpng.so