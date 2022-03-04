.PHONY: stat dyn clean test

clean:
	$(MAKE) -C ../zad2 rclean

stat:
	$(MAKE) -C ../zad2 main-static

dyn:
	$(MAKE) -C ../zad2 main-dynamic main-shared

test:
	(\
		cd ../zad2; \
		echo === STATIC ===; \
		./main-invocation.sh ./main-static; \
		echo === SHARED ===; \
		./main-invocation.sh ./main-shared; \
		echo === DYNAMIC ===; \
		./main-invocation.sh ./main-dynamic; \
	) > results3a.txt
