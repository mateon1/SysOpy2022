.PHONY: all clean test zad2

all: zad2

clean:
	$(MAKE) -C ../zad2 rclean

zad2:
	$(MAKE) -C ../zad2 all

test:
	(\
		cd ../zad2; \
		echo === STATIC ===; \
		./main-invocation.sh ./main-static; \
		echo === SHARED ===; \
		./main-invocation.sh ./main-shared; \
		echo === DYNAMIC ===; \
		./main-invocation.sh ./main-dynamic; \
	) | tee results3a.txt
