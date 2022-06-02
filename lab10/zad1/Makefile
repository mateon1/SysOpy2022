CFLAGS += -Wall

.PHONY: all clean test

all: server client

clean:
	rm -f server client

server: main.c
	$(LINK.c) -DSERVER $< -o $@

client: main.c
	$(LINK.c) -DCLIENT $< -o $@

test: server client
	@echo "Please run ./server <port> <sock> and ./client <id> <url|sock> manually"
