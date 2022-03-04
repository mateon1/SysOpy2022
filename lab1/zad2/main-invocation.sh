#!/usr/bin/env sh
./main \
 timer "Small file (Makefile)" \
  count "Makefile" \
 endtimer \
 timer "Medium file (libwc.c)" \
  count "../zad1/libwc.c" \
 endtimer \
 timer "Big file (main)" \
  count "../zad2/main" \
 endtimer \
 timer "10 small files (Makefile)" \
  count "$(yes Makefile | head -10 | tr '\n' ' ')" \
 endtimer \
 timer "10 medium files (libwc.c)" \
  count "$(yes ../zad1/libwc.c | head -10 | tr '\n' ' ')" \
 endtimer \
 timer "10 big files (main)" \
  count "$(yes ../zad2/main | head -10 | tr '\n' ' ')" \
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
  $(for i in 6 7 8 9 10 11 12 13 14; do
      echo count Makefile del $i
    done) \
 endtimer
