CFLAGS += -Wall

.PHONY: all clean test

all: main-kill main-queue main-rt

clean:
	rm -f main-kill main-queue main-rt

main-kill: main.c
	$(LINK.c) -DKILL $< -o $@

main-queue: main.c
	$(LINK.c) -DQUEUE $< -o $@

main-rt: main.c
	$(LINK.c) -DRT $< -o $@

test: main-kill main-queue main-rt
	./main-kill 10000
	./main-queue 10000
	./main-rt 10000
