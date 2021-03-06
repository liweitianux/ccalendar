PROG=		calendar
MAN=		calendar.1
SRCS=		$(wildcard src/*.c)
OBJS=		$(SRCS:.c=.o)
CALFILE=	calendar.default
DISTFILES=	GNUmakefile LICENSE README.md calendars patches src \
		$(CALFILE).in $(MAN).in

PREFIX?=	/usr/local
ETC_DIR?=	$(PREFIX)/etc
CALENDAR_ETCDIR?=$(ETC_DIR)/calendar
CALENDAR_DIR?=	$(PREFIX)/share/calendar
MAN_DIR?=	$(PREFIX)/share/man

SED_EXPR?=	-e 's|@@CALENDAR_ETCDIR@@|$(CALENDAR_ETCDIR)|' \
		-e 's|@@CALENDAR_DIR@@|$(CALENDAR_DIR)|'

TMPDIR?=	/tmp
ARCHBUILD_DIR?=	$(TMPDIR)/$(PROG)-archbuild
RPMBUILD_DIR?=	$(TMPDIR)/$(PROG)-rpmbuild

CFLAGS?=	-g -O2 -pipe \
		-Wall -Wextra -Wlogical-op -Wshadow -Wformat=2 \
		-Wwrite-strings -Wcast-qual -Wcast-align \
		-Wduplicated-cond -Wduplicated-branches \
		-Wrestrict -Wnull-dereference -Wsign-conversion \
		-Winline

CFLAGS+=	-std=c99 -pedantic \
		-DCALENDAR_ETCDIR='"$(CALENDAR_ETCDIR)"' \
		-DCALENDAR_DIR='"$(CALENDAR_DIR)"'

LDFLAGS+=	-lm

ARCH?=		$(shell uname -m)
OS?=		$(shell uname -s)
ifeq ($(OS),Linux)
CFLAGS+=	-D_GNU_SOURCE
endif


.PHONY: all
all: $(PROG) $(MAN).gz $(CALFILE)

ifeq ($(OS),$(filter $(OS),Linux Darwin))
.PHONY: patch
all: patch
patch:
ifneq (,$(wildcard ./patches/calendars-zh-locale.patch))
	patch -p1 < patches/calendars-zh-locale.patch
	rm patches/calendars-zh-locale.patch
endif
endif

.PHONY: debug
debug: $(PROG)
debug: CFLAGS+=-DDEBUG

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(MAN) $(CALFILE):
	sed $(SED_EXPR) $@.in > $@
$(MAN).gz: $(MAN)
	gzip -9c $(MAN) > $@
CLEANFILES+=	$(MAN) $(MAN).gz $(CALFILE)


.PHONY: install
install:
	[ -d "$(PREFIX)/bin" ] || mkdir -p $(PREFIX)/bin
	install -m 0755 -s $(PROG) $(PREFIX)/bin/$(PROG)
	[ -d "$(MAN_DIR)/man1" ] || mkdir -p $(MAN_DIR)/man1
	install -m 0644 $(MAN).gz $(MAN_DIR)/man1/$(MAN).gz
	[ -d "$(CALENDAR_ETCDIR)" ] || mkdir -p $(CALENDAR_ETCDIR)
	install -m 0644 $(CALFILE) $(CALENDAR_ETCDIR)/default
	[ -d "$(CALENDAR_DIR)" ] || mkdir -p $(CALENDAR_DIR)
	cp -R calendars/* $(CALENDAR_DIR)/
	find $(CALENDAR_DIR)/ -type d | xargs chmod 0755
	find $(CALENDAR_DIR)/ -type f | xargs chmod 0644

.PHONY: uninstall
uninstall:
	rm -rf  $(PREFIX)/bin/$(PROG) \
		$(MAN_DIR)/man1/$(MAN).gz \
		$(CALENDAR_ETCDIR)/default \
		$(CALENDAR_DIR)
	rmdir $(CALENDAR_ETCDIR) 2>/dev/null || true

.PHONY: clean
clean:
	rm -f $(PROG) $(OBJS) $(CLEANFILES)

include autodep.mk


.PHONY: archpkg
archpkg:
	rm -rf $(ARCHBUILD_DIR)
	mkdir -p $(ARCHBUILD_DIR)/src
	cp linux/PKGBUILD $(ARCHBUILD_DIR)/
	cp -Rp $(DISTFILES) $(ARCHBUILD_DIR)/src/
	( cd $(ARCHBUILD_DIR) && makepkg )
	@pkg=`( cd $(ARCHBUILD_DIR); ls $(PROG)-*.pkg.* )` ; \
		cp -v $(ARCHBUILD_DIR)/$${pkg} . ; \
		rm -rf $(ARCHBUILD_DIR) ; \
		echo "Install with: 'sudo pacman -U $${pkg}'"

.PHONY: rpm
rpm:
	rm -rf $(RPMBUILD_DIR)
	mkdir -p $(RPMBUILD_DIR)/BUILD
	cp -Rp $(DISTFILES) $(RPMBUILD_DIR)/BUILD/
	rpmbuild -bb -v \
		--define="_topdir $(RPMBUILD_DIR)" \
		linux/$(PROG).spec
	@pkg=`( cd $(RPMBUILD_DIR)/RPMS/$(ARCH); ls $(PROG)-*.rpm )` ; \
		cp -v $(RPMBUILD_DIR)/RPMS/$(ARCH)/$${pkg} . ; \
		rm -rf $(RPMBUILD_DIR) ; \
		echo "Install with: 'sudo yum install $${pkg}'"
