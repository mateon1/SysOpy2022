.PHONY: all clean

CFLAGS += -Wall

all: libwc.a libwc.so libwc.so.1
clean:
	rm -f libwc.a libwc.o libwc.so*

libwc.a: libwc.o
	$(AR) rcs libwc.a libwc.o

libwc.so.1.0.0: libwc.c
	$(LINK.c) -fPIC -shared -Wl,-soname,libwc.so.1 $< -o $@

libwc.so libwc.so.1: libwc.so.1.0.0
	rm -f $@ && ln -s $< $@
