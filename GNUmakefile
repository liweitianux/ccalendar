PROG=		calendar
MAN=		calendar.1
SRCS=		calendar.c dates.c day.c events.c io.c locale.c ostern.c \
		parsedata.c paskha.c pom.c sunpos.c utils.c
OBJS=		$(SRCS:.c=.o)

PREFIX?=	/usr/local
MAN_DIR?=	$(PREFIX)/man
CALENDAR_DIR?=	$(PREFIX)/share/calendar

CSTD?=		c99
CFLAGS=		-std=${CSTD} -pedantic -O2 -pipe
CFLAGS+=	-DCALENDAR_DIR='"$(CALENDAR_DIR)"'
CFLAGS+=	-Wall -Wextra -Wlogical-op -Wshadow -Wformat=2 \
		-Wwrite-strings -Wcast-qual -Wcast-align \
		-Wduplicated-cond -Wduplicated-branches \
		-Wrestrict -Wnull-dereference -Wsign-conversion

LIBS=		-lm

OS?=		$(shell uname -s)
ifeq ($(OS),Linux)
CFLAGS+=	-D_GNU_SOURCE -D__dead2=
endif

all: $(PROG) $(MAN)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

$(MAN): $(MAN).in
	sed -e 's|@@CALENDAR_DIR@@|$(CALENDAR_DIR)|' $(MAN).in > $@

install:
	install -s -Dm 0755 $(PROG) $(PREFIX)/bin/$(PROG)
	install -Dm 0644 $(MAN) $(PREFIX)/man/man1/$(MAN)
	gzip -9 $(PREFIX)/man/man1/$(MAN)
	[ -d "$(CALENDAR_DIR)" ] || mkdir -p $(CALENDAR_DIR)
	cp -R calendars/* $(CALENDAR_DIR)/
	find $(CALENDAR_DIR)/ -type d | xargs chmod 0755
	find $(CALENDAR_DIR)/ -type f | xargs chmod 0644

clean:
	rm -f $(PROG) $(OBJS) $(MAN) $(MAN).gz

.PHONY: all install clean
