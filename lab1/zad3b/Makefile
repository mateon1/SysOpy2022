.PHONY: all

all:
	rm -f results3b.txt
	CFLAGS="-O0" make -C ../zad3a rebuild test
	echo -e   "===== Optimize level -O0 =====" >> results3b.txt
	cat ../zad3a/results3a.txt >> results3b.txt
	CFLAGS="-O3" make -C ../zad3a rebuild
	echo -e "\n===== Optimize level -O3 =====" >> results3b.txt
	cat ../zad3a/results3a.txt >> results3b.txt
	CFLAGS="-Os" make -C ../zad3a rebuild
	echo -e "\n===== Optimize level -Os =====" >> results3b.txt
	cat ../zad3a/results3a.txt >> results3b.txt
