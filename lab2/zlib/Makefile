BUILD := ../build
OBJDIR := object_files

SRCS := adler32.c crc32.c deflate.c infback.c inffast.c inflate.c inftrees.c \
	trees.c zutil.c compress.c uncompr.c
OBJS := $(addprefix $(OBJDIR)/,$(SRCS:.c=.o))

CFLAGS := -fPIC -O2 -fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
	-DHAVE_UNISTD_H -D_LARGEFILE64_SOURCE=1 -Wno-old-style-definition -MMD -MP

.PHONY: static dynamic blob clean

static: $(BUILD)/libz.a
dynamic: $(BUILD)/libz.so.1
blob: static

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	gcc $(CFLAGS) -c $< -o $@

$(BUILD)/libz.a: $(OBJS)
	@mkdir -p $(BUILD)
	ar rcs $@ $(OBJS)

$(BUILD)/libz.so.1: $(OBJS)
	@mkdir -p $(BUILD)
	gcc -shared -Wl,-soname,libz.so.1 -o $@ $(OBJS)
	ln -sfn libz.so.1 $(BUILD)/libz.so

clean:
	rm -rf $(OBJDIR)

-include $(OBJS:.o=.d)
