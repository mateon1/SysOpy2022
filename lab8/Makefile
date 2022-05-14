CFLAGS += -Wall

.PHONY: all clean test

all: main

clean:
	rm -f main out.pgm out.ppm

deepclean: clean
	rm -f fractal_tree.ppm fractal_tree.pgm fractal_tree.png fractal_tree.ascii.pgm

main: main.c
	$(LINK.c) -lpthread -D_GNU_SOURCE $< -o $@

fractal_tree.ascii.pgm:
	wget https://people.sc.fsu.edu/~jburkardt/data/pgma/fractal_tree.ascii.pgm

fractal_tree.png:
	wget https://people.sc.fsu.edu/~jburkardt/data/pgma/fractal_tree.png

fractal_tree.ppm: fractal_tree.png
	convert $< $@

fractal_tree.pgm: fractal_tree.png
	convert $< $@

test: main fractal_tree.pgm
	./main 1 num fractal_tree.pgm out.pgm
	@echo
	./main 1 blk fractal_tree.pgm out.pgm
	@echo ---
	./main 2 num fractal_tree.pgm out.pgm
	@echo
	./main 2 blk fractal_tree.pgm out.pgm
	@echo ---
	./main 4 num fractal_tree.pgm out.pgm
	@echo
	./main 4 blk fractal_tree.pgm out.pgm
	@echo ---
	./main 8 num fractal_tree.pgm out.pgm
	@echo
	./main 8 blk fractal_tree.pgm out.pgm
	@echo ---
	./main 16 num fractal_tree.pgm out.pgm
	@echo
	./main 16 blk fractal_tree.pgm out.pgm
	@echo ---
	./main 32 num fractal_tree.pgm out.pgm
	@echo
	./main 32 blk fractal_tree.pgm out.pgm

testascii: main fractal_tree.ascii.pgm
	./main 1 num fractal_tree.ascii.pgm out.pgm
	@echo
	./main 1 blk fractal_tree.ascii.pgm out.pgm
	@echo ---
	./main 2 num fractal_tree.ascii.pgm out.pgm
	@echo
	./main 2 blk fractal_tree.ascii.pgm out.pgm
	@echo ---
	./main 4 num fractal_tree.ascii.pgm out.pgm
	@echo
	./main 4 blk fractal_tree.ascii.pgm out.pgm
	@echo ---
	./main 8 num fractal_tree.ascii.pgm out.pgm
	@echo
	./main 8 blk fractal_tree.ascii.pgm out.pgm
	@echo ---
	./main 16 num fractal_tree.ascii.pgm out.pgm
	@echo
	./main 16 blk fractal_tree.ascii.pgm out.pgm
	@echo ---
	./main 32 num fractal_tree.ascii.pgm out.pgm
	@echo
	./main 32 blk fractal_tree.ascii.pgm out.pgm

testppm: main fractal_tree.ppm
	./main 1 num fractal_tree.ppm out.ppm
	@echo
	./main 1 blk fractal_tree.ppm out.ppm
	@echo ---
	./main 2 num fractal_tree.ppm out.ppm
	@echo
	./main 2 blk fractal_tree.ppm out.ppm
	@echo ---
	./main 4 num fractal_tree.ppm out.ppm
	@echo
	./main 4 blk fractal_tree.ppm out.ppm
	@echo ---
	./main 8 num fractal_tree.ppm out.ppm
	@echo
	./main 8 blk fractal_tree.ppm out.ppm
	@echo ---
	./main 16 num fractal_tree.ppm out.ppm
	@echo
	./main 16 blk fractal_tree.ppm out.ppm
	@echo ---
	./main 32 num fractal_tree.ppm out.ppm
	@echo
	./main 32 blk fractal_tree.ppm out.ppm
