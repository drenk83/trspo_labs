BUILD := ../build
OBJDIR := object_files

SRCS := src/base/ftsystem.c src/base/ftinit.c src/base/ftdebug.c \
	src/base/ftbase.c src/base/ftbitmap.c src/sfnt/sfnt.c \
	src/truetype/truetype.c src/psnames/psnames.c src/smooth/smooth.c
OBJS := $(patsubst %.c,$(OBJDIR)/%.o,$(SRCS))

CFLAGS := -fPIC -O2 -fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
	-DFT2_BUILD_LIBRARY \
	-DFT_CONFIG_OPTIONS_H='<ftoption.h>' \
	-DFT_CONFIG_MODULES_H='<ftmodule.h>' \
	-Icustom -Iinclude -MMD -MP

.PHONY: static dynamic blob clean

static: $(BUILD)/libfreetype.a
dynamic: $(BUILD)/libfreetype.so
blob: static

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	gcc $(CFLAGS) -c $< -o $@

$(BUILD)/libfreetype.a: $(OBJS)
	@mkdir -p $(BUILD)
	ar rcs $@ $(OBJS)

$(BUILD)/libfreetype.so: $(OBJS)
	@mkdir -p $(BUILD)
	gcc -shared -Wl,-soname,libfreetype.so -o $@ $(OBJS)

clean:
	rm -rf $(OBJDIR)

-include $(OBJS:.o=.d)
