.PHONY: all clean

CFLAGS += -I ./libwc

all: main
clean:
	rm -f main

main: main.c
	$(LINK.c) $< -o $@
