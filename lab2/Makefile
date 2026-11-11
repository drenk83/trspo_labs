.PHONY: all static dynamic blob clean clean-bins

all:
	$(MAKE) clean-bins
	$(MAKE) static
	$(MAKE) dynamic
	$(MAKE) blob

clean-bins:
	rm -f build/app-static build/app-dynamic build/app-blob

# $(MAKE) -C <dir> <target> - вызывает make в dir
static:
	$(MAKE) -C zlib static
	$(MAKE) -C libpng static
	$(MAKE) -C freetype static
	$(MAKE) -C app static

dynamic:
	$(MAKE) -C zlib dynamic
	$(MAKE) -C libpng dynamic
	$(MAKE) -C freetype dynamic
	$(MAKE) -C app dynamic

blob:
	$(MAKE) -C zlib blob
	$(MAKE) -C libpng blob
	$(MAKE) -C freetype blob
	$(MAKE) -C app blob
	$(MAKE) -C elfloader

clean:
	$(MAKE) -C zlib clean
	$(MAKE) -C libpng clean
	$(MAKE) -C freetype clean
	$(MAKE) -C app clean
	$(MAKE) -C elfloader clean
	rm -rf build
