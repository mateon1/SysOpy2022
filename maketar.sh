#!/usr/bin/env sh
tar --xform='s:^[.]/lab:./NasciszewskiMateusz/lab:' --exclude 'README.*' -cvaf NasciszewskiMateusz-$1.tar.gz --sort=name ./$1
