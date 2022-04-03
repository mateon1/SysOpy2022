CFLAGS += -Wall

.PHONY: all clean test

all: main-fork main-exec child

clean:
	rm -f main-* child

main-fork: main.c
	$(LINK.c) -DFORK $< -o $@

main-exec: main.c child
	$(LINK.c) $< -o $@

child: main.c
	$(LINK.c) -DCHILD $< -o $@

test: main-fork main-exec
	./main-fork 0
	./main-fork 1
	./main-fork 2
	./main-fork 3
	./main-exec 0
	./main-exec 1
	./main-exec 2
	./main-exec 3
