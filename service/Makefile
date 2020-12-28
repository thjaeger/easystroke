#  Copyright (c) 2008-2009, Thomas Jaeger <ThJaeger@gmail.com>
#
#  Permission to use, copy, modify, and/or distribute this software for any
#  purpose with or without fee is hereby granted, provided that the above
#  copyright notice and this permission notice appear in all copies.
#
#  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
#  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
#  MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY
#  SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
#  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
#  OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
#  CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

DESTDIR  =
PREFIX   = /usr/local
BINDIR   = $(PREFIX)/bin
ICONDIR  = $(PREFIX)/share/icons/hicolor/scalable/apps
MENUDIR  = $(PREFIX)/share/applications
LOCALEDIR= $(PREFIX)/share/locale
DFLAGS   =
OFLAGS   = -O2
AOFLAGS  = -O3
STROKEFLAGS  = -Wall -std=c11 $(DFLAGS)
CXXSTD = -std=c++11
INCLUDES = $(shell pkg-config gtkmm-3.0 dbus-glib-1 --cflags)
CXXFLAGS = $(CXXSTD) -Wall $(DFLAGS) -DLOCALEDIR=\"$(LOCALEDIR)\" $(INCLUDES)
CFLAGS   = -std=c11 -Wall $(DFLAGS) -DLOCALEDIR=\"$(LOCALEDIR)\" $(INCLUDES) -DGETTEXT_PACKAGE='"easystroke"'
LDFLAGS  = $(DFLAGS)

LIBS     = $(DFLAGS) -lboost_serialization -lX11 -lXext -lXi -lXfixes -lXtst `pkg-config gtkmm-3.0 dbus-glib-1 --libs`

BINARY   = easy-gesture
ICON     = easystroke.svg
MENU     = easystroke.desktop
MANPAGE  = easystroke.1

CCFILES  = $(wildcard *.cc)
HFILES   = $(wildcard *.h)
OFILES   = $(patsubst %.cc,%.o,$(CCFILES)) stroke.o desktop.o version.o
POFILES  = $(wildcard po/*.po)
MOFILES  = $(patsubst po/%.po,po/%/LC_MESSAGES/easystroke.mo,$(POFILES))
MODIRS   = $(patsubst po/%.po,po/%,$(POFILES))
DEPFILES = $(wildcard *.Po)
GENFILES = gui.c desktop.c po/POTFILES.in easystroke.desktop
GZFILES  = $(wildcard *.gz)

VERSION  = $(shell test -e debian/changelog && grep '(.*)' debian/changelog | sed 's/.*(//' | sed 's/).*//' | head -n1 || (test -e version && cat version || git describe))
GIT      = $(wildcard .git/index version)
DIST     = easystroke-$(VERSION)

-include debug.mk

all: $(BINARY) $(MOFILES)

.PHONY: all clean translate update-translations compile-translations complete

clean:
	$(RM) $(OFILES) $(BINARY) $(GENFILES) $(DEPFILES) $(MANPAGE) $(GZFILES) po/*.pot
	$(RM) -r $(MODIRS)

include $(DEPFILES)

$(BINARY): $(OFILES)
	$(CXX) $(LDFLAGS) -o $@ $(OFILES) $(LIBS)

stroke.o: stroke.c
	$(CC) $(STROKEFLAGS) $(AOFLAGS) -MT $@ -MMD -MP -MF $*.Po -o $@ -c $<

%.o: %.c
	$(CC) $(CFLAGS) $(OFLAGS) -MT $@ -MMD -MP -MF $*.Po -o $@ -c $<

%.o: %.cc
	$(CXX) $(CXXFLAGS) $(OFLAGS) -MT $@ -MMD -MP -MF $*.Po -o $@ -c $<

version.o: $(GIT)
	echo 'const char *version_string = "$(VERSION)";' | $(CXX) -o $@ -c -xc++ -

easystroke.desktop: easystroke.desktop.in $(MOFILES)
	intltool-merge po/ -d -u $< $@

desktop.c: easystroke.desktop
	echo "const char *desktop_file = \"\\" > $@
	sed 's/Exec=easystroke/Exec=%1$$s/' $< | sed 's/"/\\"/g' | sed 's/.*/&\\n\\/' >> $@
	echo "\";" >> $@

po/POTFILES.in: $(CCFILES) $(HFILES)
	$(RM) $@
	for f in `grep -El "\<_\(" $^`; do echo $$f >> $@; done
	echo gui.glade >> $@
	echo easystroke.desktop.in >> $@

translate: po/POTFILES.in
	cd po && XGETTEXT_ARGS="--package-name=easystroke --copyright-holder='Thomas Jaeger <ThJaeger@gmail.com>'" intltool-update --pot -g messages

compile-translations: $(MOFILES)

update-translations: po/POTFILES.in
	cd po && for f in $(POFILES); do \
		intltool-update `echo $$f | sed "s|po/\(.*\)\.po$$|\1|"`; \
	done

strip-translations:
	for f in $(POFILES); do \
		grep -v '^#:' $$f > $$f.out; \
		mv $$f.out $$f; \
	done

po/%/LC_MESSAGES/easystroke.mo: po/%.po
	mkdir -p po/$*/LC_MESSAGES
	msgfmt -c $< -o $@

man:	$(MANPAGE)

$(MANPAGE):	$(BINARY)
	help2man -N -n "X11 gesture recognition application" ./$(BINARY) > $@

install: all
	install -Ds $(BINARY) $(DESTDIR)$(BINDIR)/$(BINARY)
	install -D -m 644 $(ICON) $(DESTDIR)$(ICONDIR)/$(ICON)
	install -D -m 644 $(MENU) $(DESTDIR)$(MENUDIR)/$(MENU)
	for f in $(MOFILES); do \
		install -D -m 644 $$f `echo $$f | sed "s|^po/|$(DESTDIR)$(LOCALEDIR)/|"`; \
	done

uninstall:
	$(RM) $(DESTDIR)$(BINDIR)/$(BINARY)
	$(RM) $(DESTDIR)$(ICONDIR)/$(ICON)
	$(RM) $(DESTDIR)$(MENUDIR)/$(MENU)
	for f in $(MOFILES); do \
		$(RM) `echo $$f | sed "s|^po/|$(DESTDIR)$(LOCALEDIR)/|"`; \
	done

tarball: $(DIST).tar.gz

tmp/$(DIST): $(GIT)
	$(RM) -r tmp
	mkdir tmp
	git archive --format=tar --prefix=$(DIST)/ HEAD | (cd tmp && tar x)
	echo $(VERSION) > $@/version
	$(RM) $@/.gitignore $@/release

$(DIST).tar.gz: tmp/$(DIST)
	tar -czf $@ -C tmp/ $(DIST)
	$(RM) -r tmp

complete: .clang_complete

.clang_complete: Makefile
	@echo $(CXXSTD) > $@
	@$(foreach inc,$(INCLUDES),echo $(inc) >> $@;)

run:
	make -j2
	./easy-gesture
