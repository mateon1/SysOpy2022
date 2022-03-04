.PHONY: all clean rclean libwc

CFLAGS += -Wall -I ./libwc
SHAREDFLAGS = -L ./libwc

all: main-shared main-static main-dynamic
clean:
	rm -f main-shared main-static main-dynamic
rclean: clean
	$(MAKE) -C libwc clean

main-shared: main.c | libwc
	$(LINK.c) $(SHAREDFLAGS) $< -lwc -o $@

main-static: main.c | libwc
	$(LINK.c) -static $< libwc/libwc.a -o $@

main-dynamic: main.c | libwc
	$(LINK.c) -DDYNAMIC $< -ldl -o $@

libwc:
	$(MAKE) -C libwc
