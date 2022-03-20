CFLAGS += -Wall

.PHONY: all clean test

all: main-lib main-sys

clean:
	rm -f main-lib main-sys

main-lib: main.c
	$(LINK.c) $< -o $@

main-sys: main.c
	$(LINK.c) -DSYS $< -o $@

test: main-lib main-sys
	./main-lib ' ' main.c
	./main-sys ' ' main.c
