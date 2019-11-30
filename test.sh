#!/bin/sh

SRCS="basics.c chinese.c gregorian.c moon.c sun.c utils.c"
CFLAGS="-std=c99 -O2 -Wall -Wextra"
LDFLAGS="-lm"

gcc ${CFLAGS} -o test test.c ${SRCS} ${LDFLAGS} && ./test
