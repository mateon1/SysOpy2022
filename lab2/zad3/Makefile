CFLAGS += -Wall

.PHONY: all clean test

all: main-nftw main-dir

clean:
	rm -f main-nftw main-dir

main-dir: main.c
	$(LINK.c) $< -o $@

main-nftw: main.c
	$(LINK.c) -DNFTW $< -o $@

test: main-dir main-nftw
	./main-dir ../..
	./main-nftw ../..
