CFLAGS += -Wall

.PHONY: all clean test

all: posix sysv

clean:
	rm -f posix sysv

posix: main.c
	$(LINK.c) -lrt -DPOSIX $< -o $@

sysv: main.c
	$(LINK.c) -lrt $< -o $@

test: posix sysv
	echo "Please run ./posix <N> <M> and ./sysv <N> <M> manually"
