CFLAGS += -Wall

.PHONY: all clean test

all: main

clean:
	rm -f main

main: main.c
	$(LINK.c) $< -o $@

test: main
	./main
