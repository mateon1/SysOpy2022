CFLAGS += -Wall

.PHONY: all clean test

all: main

clean:
	rm -f main

main: main.c
	$(LINK.c) -D_GNU_SOURCE -D_XOPEN_SOURCE=500 $< -o $@

test: main
	./main
