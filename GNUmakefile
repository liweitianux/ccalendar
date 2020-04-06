PROG=		calendar
MAN=		calendar.1
SRCS=		$(wildcard src/*.c)
OBJS=		$(SRCS:.c=.o)
DISTFILES=	GNUmakefile LICENSE README.md calendars src $(MAN).in

PREFIX?=	/usr/local
MAN_DIR?=	$(PREFIX)/share/man
CALENDAR_DIR?=	$(PREFIX)/share/calendar

TMPDIR?=	/tmp
ARCHBUILD_DIR?=	$(TMPDIR)/$(PROG)-archbuild

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
	install -Dm 0644 $(MAN) $(MAN_DIR)/man1/$(MAN)
	gzip -9 $(MAN_DIR)/man1/$(MAN)
	[ -d "$(CALENDAR_DIR)" ] || mkdir -p $(CALENDAR_DIR)
	cp -R calendars/* $(CALENDAR_DIR)/
	find $(CALENDAR_DIR)/ -type d | xargs chmod 0755
	find $(CALENDAR_DIR)/ -type f | xargs chmod 0644

clean:
	rm -f $(PROG) $(OBJS) $(MAN) $(MAN).gz

archpkg:
	mkdir -p $(ARCHBUILD_DIR)/src
	cp linux/PKGBUILD $(ARCHBUILD_DIR)/
	cp -Rp $(DISTFILES) $(ARCHBUILD_DIR)/src/
	( cd $(ARCHBUILD_DIR) && makepkg )
	@pkg=`( cd $(ARCHBUILD_DIR); ls $(PROG)-*.pkg.* )` ; \
		cp -v $(ARCHBUILD_DIR)/$${pkg} . ; \
		rm -rf $(ARCHBUILD_DIR) ; \
		echo "Install with: 'sudo pacman -U $${pkg}'"

.PHONY: all install clean archpkg
