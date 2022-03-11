.PHONY: all clean rclean libwc static-compile

CFLAGS += -Wall -I ./libwc
SHAREDFLAGS = -L ./libwc

all: main-shared static-compile main-dynamic
clean:
	rm -f main-shared main-static main-dynamic
rclean: clean
	$(MAKE) -C libwc clean

main-shared: main.c | libwc
	$(LINK.c) $(SHAREDFLAGS) $< -lwc -o $@

main-static: main.c | libwc
	$(LINK.c) -static $< libwc/libwc.a -o $@

static-compile:
	if [ -d /nix ]; then \
		nix-shell -p glibc.static --run "$(MAKE) main-static"; \
	else \
		$(MAKE) main-static; \
	fi

main-dynamic: main.c | libwc
	$(LINK.c) -DDYNAMIC $< -ldl -o $@

libwc:
	$(MAKE) -C libwc
