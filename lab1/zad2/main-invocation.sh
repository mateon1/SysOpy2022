#!/usr/bin/env sh
LD_LIBRARY_PATH=./libwc "$1" \
 header \
 timer "Small file" \
  count "../zad2/data/rand1K.txt" \
 endtimer \
 timer "Medium file" \
  count "../zad2/data/rand16K.txt" \
 endtimer \
 timer "Big file" \
  count "../zad2/data/rand4M.txt" \
 endtimer \
 timer "10 small files" \
  count "$(yes ../zad2/data/rand1K.txt | head -10 | tr '\n' ' ')" \
 endtimer \
 timer "10 medium files" \
  count "$(yes ../zad2/data/rand16K.txt | head -10 | tr '\n' ' ')" \
 endtimer \
 timer "10 big files" \
  count "$(yes ../zad2/data/rand4M.txt | head -10 | tr '\n' ' ')" \
 endtimer \
 timer "Delete blocks" \
  del 0 \
  del 1 \
  del 2 \
  del 3 \
  del 4 \
  del 5 \
 endtimer \
 timer "Add & delete blocks" \
  $(for i in 6 7 8 9 10 11 12 13 14 15 16; do
      echo count ../zad2/data/empty.txt del $i
    done) \
 endtimer
