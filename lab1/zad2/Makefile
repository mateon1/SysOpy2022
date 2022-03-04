.PHONY: all clean libwc

CFLAGS += -I ./libwc

all: main
clean:
	rm -f main

main: main.c | libwc
	$(LINK.c) $< libwc/libwc.a -o $@

libwc:
	$(MAKE) -C libwc
