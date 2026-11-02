BUILD := ../build
OBJDIR := object_files

INCLUDES := -I../freetype/include -I../libpng -I../zlib -I.
HOST_CFLAGS := -Wall -Wextra -O2 -fPIC -fno-stack-protector \
	-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -MMD -MP $(INCLUDES)
BLOB_CFLAGS := $(HOST_CFLAGS) -fPIE -fno-builtin -fno-asynchronous-unwind-tables

LIBS := $(BUILD)/libfreetype.a $(BUILD)/libpng.a $(BUILD)/libz.a

.PHONY: static dynamic blob clean

static: $(BUILD)/app-static
dynamic: $(BUILD)/app-dynamic
blob: $(BUILD)/app-blob

$(OBJDIR)/funcs.o: funcs.c funcs.h
	@mkdir -p $(dir $@)
	gcc $(HOST_CFLAGS) -c funcs.c -o $@

$(OBJDIR)/static/main.o: main.c funcs.h
	@mkdir -p $(dir $@)
	gcc $(HOST_CFLAGS) -DMODE_STATIC -c main.c -o $@

$(OBJDIR)/dynamic/main.o: main.c funcs.h dynload.h
	@mkdir -p $(dir $@)
	gcc $(HOST_CFLAGS) -DMODE_DYNAMIC -c main.c -o $@

$(OBJDIR)/dynamic/funcs.o: funcs.c funcs.h dynload.h
	@mkdir -p $(dir $@)
	gcc $(HOST_CFLAGS) -DMODE_DYNAMIC -c funcs.c -o $@

$(OBJDIR)/dynamic/dynload.o: dynload.c dynload.h
	@mkdir -p $(dir $@)
	gcc $(HOST_CFLAGS) -c dynload.c -o $@

$(OBJDIR)/blob/main.o: main.c funcs.h host_api.h
	@mkdir -p $(dir $@)
	gcc $(BLOB_CFLAGS) -DMODE_BLOB -c main.c -o $@

$(OBJDIR)/blob/funcs.o: funcs.c funcs.h
	@mkdir -p $(dir $@)
	gcc $(BLOB_CFLAGS) -c funcs.c -o $@

$(OBJDIR)/blob/host_shim.o: host_shim.c host_api.h
	@mkdir -p $(dir $@)
	gcc $(BLOB_CFLAGS) -c host_shim.c -o $@

$(OBJDIR)/blob/setjmp_stub.o: setjmp_stub.S
	@mkdir -p $(dir $@)
	gcc -c setjmp_stub.S -o $@

$(BUILD)/app-static: $(OBJDIR)/static/main.o $(OBJDIR)/funcs.o $(LIBS)
	@mkdir -p $(BUILD)
	gcc -o $@ $(OBJDIR)/static/main.o $(OBJDIR)/funcs.o $(LIBS) -lm

$(BUILD)/app-dynamic: $(OBJDIR)/dynamic/main.o $(OBJDIR)/dynamic/funcs.o \
		$(OBJDIR)/dynamic/dynload.o \
		$(BUILD)/libz.so.1 $(BUILD)/libpng.so $(BUILD)/libfreetype.so
	@mkdir -p $(BUILD)
	gcc -o $@ $(OBJDIR)/dynamic/main.o $(OBJDIR)/dynamic/funcs.o \
		$(OBJDIR)/dynamic/dynload.o -ldl

$(BUILD)/app-blob: $(OBJDIR)/blob/main.o $(OBJDIR)/blob/funcs.o \
		$(OBJDIR)/blob/host_shim.o $(OBJDIR)/blob/setjmp_stub.o $(LIBS) blob.map
	@mkdir -p $(BUILD)
	gcc -nostdlib -pie -fPIE \
		-Wl,-e,blob_entry -Wl,--no-dynamic-linker -Wl,--no-undefined \
		-Wl,-Bsymbolic -Wl,--version-script,blob.map \
		-o $@ $(OBJDIR)/blob/main.o $(OBJDIR)/blob/funcs.o \
		$(OBJDIR)/blob/host_shim.o $(OBJDIR)/blob/setjmp_stub.o \
		$(LIBS) -lgcc

clean:
	rm -rf $(OBJDIR)

-include $(OBJDIR)/funcs.d $(OBJDIR)/static/main.d \
	$(OBJDIR)/dynamic/main.d $(OBJDIR)/dynamic/funcs.d $(OBJDIR)/dynamic/dynload.d \
	$(OBJDIR)/blob/main.d $(OBJDIR)/blob/funcs.d $(OBJDIR)/blob/host_shim.d
