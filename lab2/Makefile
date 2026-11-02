# Сборка трёх версий программы. Библиотеки собираются своими штатными
# системами: ./configure и make. Свои правила компиляции zlib, libpng
# и FreeType здесь не пишутся — это было бы дополнительное задание 6.2.

ROOT := $(abspath .)
BUILD := $(ROOT)/build
PREFIX := $(BUILD)/prefix
OBJ := $(BUILD)/obj

# -fPIC нужен и .so, и статическим .a внутри PIE-блоба.
# Защита стека и FORTIFY тянут символы glibc, которых у blob нет.
LIB_CFLAGS := -O2 -fPIC -fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0

.PHONY: all static dynamic blob clean clean-bins stage-libs \
	zlib-rebuild libpng-rebuild freetype-rebuild copy-libs

all:
	$(MAKE) clean-bins
	$(MAKE) stage-libs
	$(MAKE) -C $(ROOT)/app static
	$(MAKE) -C $(ROOT)/app dynamic
	$(MAKE) -C $(ROOT)/app blob
	$(MAKE) -C $(ROOT)/elfloader

clean-bins:
	rm -f $(BUILD)/app-static $(BUILD)/app-dynamic $(BUILD)/app-blob

static: stage-libs
	$(MAKE) -C $(ROOT)/app static

dynamic: stage-libs
	$(MAKE) -C $(ROOT)/app dynamic

blob: stage-libs
	$(MAKE) -C $(ROOT)/app blob
	$(MAKE) -C $(ROOT)/elfloader

stage-libs:
	$(MAKE) zlib-rebuild
	$(MAKE) libpng-rebuild
	$(MAKE) freetype-rebuild
	$(MAKE) copy-libs

$(OBJ)/zlib/.configured:
	mkdir -p $(OBJ)/zlib
	cd $(OBJ)/zlib && CFLAGS='$(LIB_CFLAGS)' \
		$(ROOT)/zlib/configure \
		--prefix=$(PREFIX) \
		--libdir=$(PREFIX)/lib \
		--sharedlibdir=$(PREFIX)/lib \
		--includedir=$(PREFIX)/include
	touch $@

$(PREFIX)/include/zlib.h: $(OBJ)/zlib/.configured
	$(MAKE) -C $(OBJ)/zlib -j$$(nproc)
	$(MAKE) -C $(OBJ)/zlib install

zlib-rebuild: $(PREFIX)/include/zlib.h
	$(MAKE) -C $(OBJ)/zlib -j$$(nproc)
	@if [ ! -f $(PREFIX)/lib/libz.a ] || [ $(OBJ)/zlib/libz.a -nt $(PREFIX)/lib/libz.a ]; then \
		cp -a $(OBJ)/zlib/libz.a $(PREFIX)/lib/libz.a; \
		cp -a $(OBJ)/zlib/libz.so.1.2.11 $(PREFIX)/lib/libz.so.1.2.11; \
		ln -sfn libz.so.1.2.11 $(PREFIX)/lib/libz.so.1; \
		ln -sfn libz.so.1.2.11 $(PREFIX)/lib/libz.so; \
	fi

$(OBJ)/libpng/.configured: $(PREFIX)/include/zlib.h
	mkdir -p $(OBJ)/libpng
	cd $(OBJ)/libpng && \
		CFLAGS='$(LIB_CFLAGS)' \
		CPPFLAGS='-I$(PREFIX)/include' \
		LDFLAGS='-L$(PREFIX)/lib -Wl,-rpath,$(PREFIX)/lib' \
		LD_LIBRARY_PATH='$(PREFIX)/lib' \
		$(ROOT)/libpng/configure \
		--prefix=$(PREFIX) \
		--libdir=$(PREFIX)/lib
	touch $@

libpng-rebuild: $(OBJ)/libpng/.configured
	LD_LIBRARY_PATH='$(PREFIX)/lib' $(MAKE) -C $(OBJ)/libpng -j$$(nproc)
	@if [ ! -f $(PREFIX)/include/png.h ]; then \
		LD_LIBRARY_PATH='$(PREFIX)/lib' $(MAKE) -C $(OBJ)/libpng install; \
	elif [ $(OBJ)/libpng/.libs/libpng16.a -nt $(PREFIX)/lib/libpng16.a ]; then \
		cp -a $(OBJ)/libpng/.libs/libpng16.a $(PREFIX)/lib/libpng16.a; \
		cp -a $(OBJ)/libpng/.libs/libpng16.so.16.37.0 $(PREFIX)/lib/libpng16.so.16.37.0; \
		ln -sfn libpng16.so.16.37.0 $(PREFIX)/lib/libpng16.so.16; \
		ln -sfn libpng16.so.16 $(PREFIX)/lib/libpng16.so; \
		ln -sfn libpng16.so $(PREFIX)/lib/libpng.so; \
		ln -sfn libpng16.a $(PREFIX)/lib/libpng.a; \
	fi

$(OBJ)/freetype/.configured: $(PREFIX)/include/zlib.h
	mkdir -p $(OBJ)/freetype
	cd $(OBJ)/freetype && \
		CFLAGS='$(LIB_CFLAGS)' \
		PKG_CONFIG_PATH='$(PREFIX)/lib/pkgconfig' \
		LDFLAGS='-L$(PREFIX)/lib -Wl,-rpath,$(PREFIX)/lib' \
		LD_LIBRARY_PATH='$(PREFIX)/lib' \
		$(ROOT)/freetype/configure \
		--prefix=$(PREFIX) \
		--libdir=$(PREFIX)/lib \
		--with-zlib=yes \
		--with-bzip2=no \
		--with-png=no \
		--with-harfbuzz=no
	touch $@

freetype-rebuild: $(OBJ)/freetype/.configured
	LD_LIBRARY_PATH='$(PREFIX)/lib' $(MAKE) -C $(OBJ)/freetype -j$$(nproc)
	@if [ ! -f $(PREFIX)/include/freetype2/ft2build.h ]; then \
		LD_LIBRARY_PATH='$(PREFIX)/lib' $(MAKE) -C $(OBJ)/freetype install; \
	elif [ $(OBJ)/freetype/.libs/libfreetype.a -nt $(PREFIX)/lib/libfreetype.a ]; then \
		cp -a $(OBJ)/freetype/.libs/libfreetype.a $(PREFIX)/lib/libfreetype.a; \
		cp -a $(OBJ)/freetype/.libs/libfreetype.so.6.17.0 $(PREFIX)/lib/libfreetype.so.6.17.0; \
		ln -sfn libfreetype.so.6.17.0 $(PREFIX)/lib/libfreetype.so.6; \
		ln -sfn libfreetype.so.6 $(PREFIX)/lib/libfreetype.so; \
	fi

# Рядом с бинарником лежат копии .so: app-dynamic открывает их по имени.
copy-libs:
	@mkdir -p $(BUILD)
	@sh -c ' \
	copy_file() { \
	  src=$$1; dest=$$2; \
	  if [ ! -f "$$dest" ] || [ "$$src" -nt "$$dest" ] || ! cmp -s "$$src" "$$dest"; then \
	    cp -L "$$src" "$$dest"; \
	  fi; \
	}; \
	pick() { \
	  for c in "$$@"; do [ -e "$$c" ] && { printf "%s" "$$c"; return 0; }; done; \
	  echo "missing library: $$*" >&2; return 1; \
	}; \
	copy_file "$$(pick $(PREFIX)/lib/libz.a)" $(BUILD)/libz.a; \
	copy_file "$$(pick $(PREFIX)/lib/libz.so.1)" $(BUILD)/libz.so.1; \
	ln -sfn libz.so.1 $(BUILD)/libz.so; \
	copy_file "$$(pick $(PREFIX)/lib/libpng.a $(PREFIX)/lib/libpng16.a)" $(BUILD)/libpng.a; \
	copy_file "$$(pick $(PREFIX)/lib/libpng.so $(PREFIX)/lib/libpng16.so)" $(BUILD)/libpng.so; \
	copy_file "$$(pick $(PREFIX)/lib/libfreetype.a)" $(BUILD)/libfreetype.a; \
	copy_file "$$(pick $(PREFIX)/lib/libfreetype.so)" $(BUILD)/libfreetype.so; \
	'

clean:
	$(MAKE) -C $(ROOT)/app clean
	$(MAKE) -C $(ROOT)/elfloader clean
	rm -rf $(BUILD) \
		$(ROOT)/zlib/object_files \
		$(ROOT)/libpng/object_files \
		$(ROOT)/freetype/object_files \
		$(ROOT)/libpng/pnglibconf.h
