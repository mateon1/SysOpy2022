.PHONY: all test clean

all: test

clean:
	$(MAKE) -C ../zad3a clean

test:
	rm -f results3b.txt
	CFLAGS="-O0" $(MAKE) -C ../zad3a clean all test
	echo -e   "===== Optimize level -O0 =====" >> results3b.txt
	cat ../zad3a/results3a.txt >> results3b.txt
	CFLAGS="-O1" $(MAKE) -C ../zad3a clean all test
	echo -e   "===== Optimize level -O1 =====" >> results3b.txt
	cat ../zad3a/results3a.txt >> results3b.txt
	CFLAGS="-O2" $(MAKE) -C ../zad3a clean all test
	echo -e   "===== Optimize level -O2 =====" >> results3b.txt
	cat ../zad3a/results3a.txt >> results3b.txt
	CFLAGS="-O3" $(MAKE) -C ../zad3a clean all test
	echo -e "\n===== Optimize level -O3 =====" >> results3b.txt
	cat ../zad3a/results3a.txt >> results3b.txt
	CFLAGS="-Os" $(MAKE) -C ../zad3a clean all test
	echo -e "\n===== Optimize level -Os =====" >> results3b.txt
	cat ../zad3a/results3a.txt >> results3b.txt
