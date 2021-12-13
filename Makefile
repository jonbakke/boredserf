# boredserf - simple browser
# See LICENSE file for copyright and license details.
.POSIX:

include config.mk

SRC = boredserf.c messages.c filter.c
WSRC = webext-boredserf.c
OBJ = $(SRC:.c=.o)
WOBJ = $(WSRC:.c=.o)
WLIB = $(WSRC:.c=.so)

all: options boredserf $(WLIB)

options:
	@echo boredserf build options:
	@echo "CC            = $(CC)"
	@echo "CFLAGS        = $(BS_CFLAGS) $(CFLAGS)"
	@echo "WEBEXTCFLAGS  = $(WEBEXTCFLAGS) $(CFLAGS)"
	@echo "LDFLAGS       = $(LDFLAGS)"

boredserf: $(OBJ)
	$(CC) $(BS_LDFLAGS) $(LDFLAGS) -o $@ $(OBJ) $(LIBS)

$(OBJ) $(WOBJ): config.h common.h config.mk

config.h:
	cp config.def.h $@

$(OBJ): $(SRC)
	$(CC) $(BS_CFLAGS) $(CFLAGS) -c $(SRC)

$(WLIB): $(WOBJ)
	$(CC) -shared -Wl,-soname,$@ $(LDFLAGS) -o $@ $? $(WEBEXTLIBS)

$(WOBJ): $(WSRC)
	$(CC) $(WEBEXTCFLAGS) $(CFLAGS) -c $(WSRC)

clean:
	rm -f boredserf $(OBJ)
	rm -f $(WLIB) $(WOBJ)

distclean: clean
	rm -f config.h boredserf-$(VERSION).tar.gz

dist: distclean
	mkdir -p boredserf-$(VERSION)
	cp -R LICENSE Makefile config.mk config.def.h README \
	    boredserf-open.sh arg.h TODO.md \
	    boredserf.1 common.h $(SRC) $(WSRC) boredserf-$(VERSION)
	tar -cf boredserf-$(VERSION).tar boredserf-$(VERSION)
	gzip boredserf-$(VERSION).tar
	rm -rf boredserf-$(VERSION)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f boredserf $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/boredserf
	mkdir -p $(DESTDIR)$(LIBDIR)
	cp -f $(WLIB) $(DESTDIR)$(LIBDIR)
	for wlib in $(WLIB); do \
	    chmod 644 $(DESTDIR)$(LIBDIR)/$$wlib; \
	done
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	sed "s/VERSION/$(VERSION)/g" < boredserf.1 > \
		$(DESTDIR)$(MANPREFIX)/man1/boredserf.1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/boredserf.1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/boredserf
	rm -f $(DESTDIR)$(MANPREFIX)/man1/boredserf.1
	for wlib in $(WLIB); do \
	    rm -f $(DESTDIR)$(LIBDIR)/$$wlib; \
	done
	- rmdir $(DESTDIR)$(LIBDIR)

.PHONY: all options distclean clean dist install uninstall
