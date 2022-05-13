CFLAGS += -Wall

.PHONY: all clean test

all: main

clean:
	rm -f main

main: main.c
	$(LINK.c) -lpthread $< -o $@

test: main
	./main 1 num fractal_tree.ascii.pgm out.pgm
	./main 1 blk fractal_tree.ascii.pgm out.pgm
	./main 2 num fractal_tree.ascii.pgm out.pgm
	./main 2 blk fractal_tree.ascii.pgm out.pgm
	./main 4 num fractal_tree.ascii.pgm out.pgm
	./main 4 blk fractal_tree.ascii.pgm out.pgm
	./main 8 num fractal_tree.ascii.pgm out.pgm
	./main 8 blk fractal_tree.ascii.pgm out.pgm
	./main 16 num fractal_tree.ascii.pgm out.pgm
	./main 16 blk fractal_tree.ascii.pgm out.pgm
	./main 32 num fractal_tree.ascii.pgm out.pgm
	./main 32 blk fractal_tree.ascii.pgm out.pgm
