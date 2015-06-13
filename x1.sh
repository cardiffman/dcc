#!/bin/bash
./dccsuper $1 > $1.c
cat base.c withadd.c $1.c > $1.lnk.c
gcc -o $1.exe -g $1.lnk.c
