BUILD := ../build
OBJDIR := object_files

SRCS := png.c pngerror.c pngget.c pngmem.c pngpread.c pngread.c pngrio.c \
	pngrtran.c pngrutil.c pngset.c pngtrans.c pngwio.c pngwrite.c \
	pngwtran.c pngwutil.c
OBJS := $(addprefix $(OBJDIR)/,$(SRCS:.c=.o))

CFLAGS := -fPIC -O2 -fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
	-I. -I../zlib -MMD -MP

.PHONY: static dynamic blob clean

static: $(BUILD)/libpng.a
dynamic: $(BUILD)/libpng.so
blob: static

pnglibconf.h: scripts/pnglibconf.h.prebuilt
	cp $< $@

$(OBJDIR)/%.o: %.c pnglibconf.h
	@mkdir -p $(dir $@)
	gcc $(CFLAGS) -c $< -o $@

$(BUILD)/libpng.a: $(OBJS)
	@mkdir -p $(BUILD)
	ar rcs $@ $(OBJS)

$(BUILD)/libpng.so: $(OBJS) $(BUILD)/libz.so.1
	@mkdir -p $(BUILD)
	gcc -shared -Wl,-soname,libpng.so -Wl,-rpath,'$$ORIGIN' -o $@ $(OBJS) \
		-L$(BUILD) -lz -lm

clean:
	rm -rf $(OBJDIR) pnglibconf.h

-include $(OBJS:.o=.d)
