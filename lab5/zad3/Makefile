CFLAGS += -Wall

.PHONY: all clean test

all: producer consumer

clean:
	rm -f producer consumer
	rm -rf test

producer: producer.c
	$(LINK.c) $< -o $@

consumer: consumer.c
	$(LINK.c) -D_GNU_SOURCE $< -o $@

test: producer consumer
	bash ./test.sh 5     1 1 "Single producer, single consumer"
	bash ./test.sh 3000  1 1 "Single producer, single consumer"
	bash ./test.sh 40000 1 1 "Single producer, single consumer"
	bash ./test.sh 5     5 1 "Multiple producer, single consumer"
	bash ./test.sh 3000  5 1 "Multiple producer, single consumer"
	bash ./test.sh 40000 5 1 "Multiple producer, single consumer"
	bash ./test.sh 5     1 5 "Single producer, multiple consumer"
	bash ./test.sh 3000  1 5 "Single producer, multiple consumer"
	bash ./test.sh 40000 1 5 "Single producer, multiple consumer"
	bash ./test.sh 5     5 5 "Multiple producer, multiple consumer"
	bash ./test.sh 3000  5 5 "Multiple producer, multiple consumer"
	bash ./test.sh 40000 5 5 "Multiple producer, multiple consumer"
