CFLAGS += -Wall -D_GNU_SOURCE

.PHONY: all clean test

all: main

clean:
	rm -f main

main: main.c
	$(LINK.c) $< -o $@

test: main
	./main ../.. main 3
	@echo ---
	./main ../.. main 2
	@echo ---
	./main .. fork 1
	@echo ---
	./main .. fork 2
