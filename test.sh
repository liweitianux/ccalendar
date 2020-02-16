#!/bin/sh

SRCS="basics.c chinese.c gregorian.c moon.c sun.c utils.c"
SRCS="${SRCS} locale.c events.c dates.c parsedata.c io.c day.c ostern.c paskha.c pom.c sunpos.c"
CFLAGS="-std=c99 -pedantic -O2 -pipe"
CFLAGS="${CFLAGS} -Wall -Wextra -Wlogical-op -Wshadow -Wformat=2
	-Wwrite-strings -Wcast-qual -Wcast-align
	-Wduplicated-cond -Wduplicated-branches
	-Wrestrict -Wnull-dereference -Wconversion"
CFLAGS="${CFLAGS} $(pkg-config --cflags libbsd-overlay)"
LDFLAGS="-lm $(pkg-config --libs   libbsd-overlay)"

[ "$(uname -s)" = "Linux" ] && CFLAGS="${CFLAGS} -D_DEFAULT_SOURCE"

gcc ${CFLAGS} -o test test.c ${SRCS} ${LDFLAGS} && ./test
