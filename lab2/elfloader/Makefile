BUILD := ../build

SRCS := main.c elfload/elfload.c elfload/elfreloc_amd64.c
HDRS := ../app/host_api.h elfload/elfload.h elfload/elfarch.h elfload/elf.h

.PHONY: all clean

all: $(BUILD)/elfloader

$(BUILD)/elfloader: $(SRCS) $(HDRS)
	@mkdir -p $(BUILD)
	gcc -D_GNU_SOURCE -Wall -Wextra -O2 -I../app -Ielfload \
		-o $@ $(SRCS) -lm

clean:
	rm -f $(BUILD)/elfloader
