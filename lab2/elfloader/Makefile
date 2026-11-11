BUILD := ../build

SRCS := main.c elf_loader.c wheelc/list.c
HDRS := ../app/host_api.h elf_loader.h wheelc/wheelc.h wheelc/list.h wheelc/log.h

.PHONY: all clean

all: $(BUILD)/elfloader

# _GNU_SOURCE включает POSIX-объявления.
# -I../app — таблица host_api.h. -I. — заголовки этого загрузчика.
# -ldl подключает libdl
# -lm подключает libm
$(BUILD)/elfloader: $(SRCS) $(HDRS)
	@mkdir -p $(BUILD)
	gcc -D_GNU_SOURCE -Wall -Wextra -O2 -I../app -I. \
		-o $@ $(SRCS) -ldl -lm

clean:
	rm -f $(BUILD)/elfloader
