CFLAGS += -Wall

.PHONY: all clean test

all: server client

clean:
	rm -f server client

server: server.c
	$(LINK.c) $< -o $@

client: client.c
	$(LINK.c) $< -o $@

test: server client
	@echo "Please run ./server and ./client manually"
	# print usages
	@./client || true
	@./server || true
