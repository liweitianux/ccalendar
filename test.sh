#!/bin/sh

SRCS="basics.c chinese.c gregorian.c moon.c sun.c utils.c"
SRCS="${SRCS} locale.c dates.c parsedata.c io.c ostern.c paskha.c"
CFLAGS="-std=c99 -pedantic -O2 -pipe"
CFLAGS="${CFLAGS} -Wall -Wextra -Wlogical-op -Wshadow -Wformat=2
	-Wwrite-strings -Wcast-qual -Wcast-align
	-Wduplicated-cond -Wduplicated-branches
	-Wrestrict -Wnull-dereference -Wsign-conversion"
CFLAGS="${CFLAGS} -I."
CFLAGS="${CFLAGS} -DCALENDAR_DIR=\"/usr/local/share/calendar\""
CFLAGS="${CFLAGS} -DCALENDAR_ETCDIR=\"/usr/local/etc/calendar\""
LDFLAGS="-lm"

if [ "$(uname -s)" = "Linux" ]; then
	CFLAGS="${CFLAGS} -D_GNU_SOURCE -D__dead2="
fi

( cd src; gcc ${CFLAGS} -o ../test ../test.c ${SRCS} ${LDFLAGS} )
