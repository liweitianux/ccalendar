PROG=		calendar
MAN=		calendar.1
SRCS=		calendar.c dates.c day.c events.c io.c locale.c ostern.c \
		parsedata.c paskha.c pom.c sunpos.c utils.c
OBJS=		$(SRCS:.c=.o)

CFLAGS=		-O2 -pipe -std=c99 -pedantic -D_GNU_SOURCE
CFLAGS+=	-Wall -Wextra -Wlogical-op -Wshadow -Wformat=2 \
		-Wwrite-strings -Wcast-qual -Wcast-align
CFLAGS+=	-Wduplicated-cond -Wduplicated-branches \
		-Wrestrict -Wnull-dereference \
#CFLAGS+=	-Wconversion

CFLAGS+=	$(shell pkg-config --cflags libbsd-overlay)
LIBS=		$(shell pkg-config --libs   libbsd-overlay) -lm

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f $(PROG) $(OBJS)

.PHONY: all clean
