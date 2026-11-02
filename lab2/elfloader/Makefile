BUILD := ../build

.PHONY: all clean

all: $(BUILD)/elfloader

$(BUILD)/elfloader: main.c ../app/host_api.h
	@mkdir -p $(BUILD)
	gcc -Wall -Wextra -O2 -I../app -o $@ main.c -lm

clean:
	rm -f $(BUILD)/elfloader
