CFLAGS += -Wall

.PHONY: all clean test

all: main

clean:
	rm -f main w*.bin

main: main.c
	$(LINK.c) $< -o $@

test: main
	time ./main 500000000 1
	time ./main 2000000000 4
	time ./main 16000000000 32
	time ./main 64000000000 128
#	./main 8000000 8
